// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "Application.hpp"
#include "Database.hpp"
#include "Gui.hpp"
#include "Peripherals.hpp"
#include "RgbLed.hpp"
#include "Telemetry.hpp"
#include "Time.hpp"
#include "credentials.h"

#include <ArduinoOTA.h>
#include <LITTLEFS.h>
#include <WiFi.h>
#include <Wire.h>
#include <chrono>
#include <ctime>
#include <functional>
#include <future>
#include <lvgl.h>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include <M5Unified.h>

// 起動時のログ
Application::BootLog Application::boot_log{};
//
const std::string_view Application::MEASUREMENTS_DATABASE_FILE_NAME{
    "/littlefs/measurements.db"};
Database Application::measurements_database{};

using namespace std::literals::string_literals;
using namespace std::chrono;
using namespace std::chrono_literals;

//
// Over The Air update
//
static void setupOTA() {
  ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS
        // using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
          Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
          Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
          Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
          Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
          Serial.println("End Failed");
      });

  ArduinoOTA.begin();
}

static void gotWiFiEvent(WiFiEvent_t event) {
  switch (event) {
  case SYSTEM_EVENT_AP_START:
    M5_LOGI("AP Started");
    break;
  case SYSTEM_EVENT_AP_STOP:
    M5_LOGI("AP Stopped");
    break;
  case SYSTEM_EVENT_STA_START:
    M5_LOGI("STA Started");
    break;
  case SYSTEM_EVENT_STA_CONNECTED:
    M5_LOGI("STA Connected");
    break;
  case SYSTEM_EVENT_AP_STA_GOT_IP6: {
    auto localipv6 = WiFi.localIPv6();
    M5_LOGI("STA IPv6: %s", localipv6.toString());
  } break;
  case SYSTEM_EVENT_STA_GOT_IP: {
    auto localip = WiFi.localIP();
    M5_LOGI("STA IPv4: %s", localip.toString());
  } break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    M5_LOGI("STA Disconnected");
    //    WiFi.begin();
    break;
  case SYSTEM_EVENT_STA_STOP:
    M5_LOGI("STA Stopped");
    break;
  default:
    break;
  }
}

//
// WiFi APとの接続待ち
//
static bool waitingForWiFiConnection(seconds TIMEOUT) {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  auto timeover{steady_clock::now() + TIMEOUT};
  auto status = WiFi.status();
  while (status != WL_CONNECTED && steady_clock::now() < timeover) {
    std::this_thread::sleep_for(10ms);
    status = WiFi.status();
  }
  return WiFi.status() == WL_CONNECTED;
}

//
// Arduinoのsetup()関数
//
void setup() {
  //
  auto logging = [](std::string_view sv) {
    Application::boot_log.logging(sv);
    if (auto p = Gui::getInstance(); p) {
      p->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
    }
  };
  constexpr auto TIMEOUT{3s};
  // initializing M5Stack Core2 with M5Unified
  auto cfg = M5.config();
  M5.begin(cfg);

  // file system init
  if (LittleFS.begin(true)) {
    M5_LOGI("filesystem status : %lu / %lu.", LittleFS.usedBytes(),
            LittleFS.totalBytes());
  } else {
    M5_LOGE("filesystem inititalize failed.");
    M5.Display.print("filesystem inititalize failed.");
    goto fatal_error;
  }

  // Display
  M5.Display.setColorDepth(LV_COLOR_DEPTH);
  M5.Display.setBrightness(160);
  // stop the vibration
  M5.Power.setVibration(0);

  // initialize the 'arduino Wire class'
  Wire.end();
  Wire.begin(M5.Ex_I2C.getSDA(), M5.Ex_I2C.getSCL(), 100000);
  std::this_thread::sleep_for(1000ms);

  // init WiFi with Station mode
  WiFi.onEvent(gotWiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.begin(Credentials.wifi_ssid, Credentials.wifi_password);

  // init RGB LEDS
  if (auto rgbled = new RgbLed(); rgbled) {
    rgbled->begin();
    rgbled->setBrightness(50);
    rgbled->fill(CRGB::White);
  }

  // init peripherals
  Peripherals::init(Wire, M5.Ex_I2C.getSDA(), M5.Ex_I2C.getSCL());

  // init GUI
  if (auto gui = new Gui(M5.Display); gui) {
    if (gui->begin()) {
      void();
    } else {
      M5.Display.print("GUI inititalize failed.");
      goto fatal_error;
    }
  } else {
    M5.Display.print("GUI inititalize failed.");
    goto fatal_error;
  }

  // create RTOS task
  static TaskHandle_t rtos_task_handle{};
  xTaskCreatePinnedToCore(
      [](void *arg) -> void {
        while (true) {
          lv_timer_handler();
          std::this_thread::sleep_for(6ms);
        }
      },
      "Task:LVGL", 16384, nullptr, 5, &rtos_task_handle, ARDUINO_RUNNING_CORE);

  logging("System Start.");

  // init WiFi
  if constexpr (true) {
    logging("connect to WiFi AP. SSID:\""s + Credentials.wifi_ssid + "\""s);
    if (!waitingForWiFiConnection(30s)) {
      logging("connect failed.");
    }
  }

  // init Time
  if constexpr (true) {
    logging("init time.");
    // initialize Time
    Time::init();
  }

  // init OTA update
  if constexpr (true) {
    logging("setup OTA update.");
    setupOTA();
  }

  // init Database
  logging("init Database.");
  if (Application::measurements_database.begin() == false) {
    // 失敗したらデータベースファイルを消去して再度初期化
    auto s = std::string(Application::MEASUREMENTS_DATABASE_FILE_NAME);
    if (!LittleFS.remove(s.c_str())) {
      // ファイルの消去に失敗したらフォーマットする
      M5_LOGI("format filesystem.");
      if (LittleFS.format() == false) {
        M5_LOGE("format failed.");
      }
    } else {
      M5_LOGI("file %s removed.", s.c_str());
    }
    //
    if (!Application::measurements_database.begin()) {
      logging("Database is not available.");
    }
  }

  // init sensors
  if constexpr (true) {
    M5_LOGI("init sensors");
    for (auto &sensor_device : Peripherals::sensors) {
      //
      std::ostringstream oss;
      SensorDescriptor desc = sensor_device->getSensorDescriptor();
      oss << "init " << desc.str() << " sensor.";
      logging(oss.str().c_str());
      bool ok{false};
      auto timeover{steady_clock::now() + TIMEOUT};
      while (steady_clock::now() < timeover) {
        std::this_thread::sleep_for(100ms);
        if (ok = sensor_device->init(); ok) {
          M5_LOGI("%s init success.", desc.str().c_str());
          break;
        }
        M5_LOGI("%s init failed, retry", desc.str().c_str());
      }
      if (!ok) {
        oss.str("");
        oss << desc.str() << " sensor not found.";
        logging(oss.str().c_str());
      }
    }
    // 初期化に失敗した(つまり接続されていない)センサーをsensorsベクタから削除する
    auto result =
        std::remove_if(Peripherals::sensors.begin(), Peripherals::sensors.end(),
                       [](auto &sensor_device) {
                         auto active = sensor_device->available();
                         return active == false;
                       });
    Peripherals::sensors.erase(result, Peripherals::sensors.end());
  }
  //
  logging("setup done.");
  Gui::getInstance()->startUi();

  return; // Successfully exit.
  //
fatal_error:
  delay(300 * 1000);
  esp_system_abort("fatal");
}

//
// 高速度loop()関数
//
inline void high_speed_loop() {
  ArduinoOTA.handle();
  M5.update();
  if (M5.BtnA.wasPressed()) {
    Gui::getInstance()->movePrev();
  } else if (M5.BtnB.wasPressed()) {
    Gui::getInstance()->home();
  } else if (M5.BtnC.wasPressed()) {
    Gui::getInstance()->moveNext();
  }
}

//
// 測定用loop関数
//
inline void measurements_loop(system_clock::time_point nowtp) {
  // 測定
  for (auto &sensor_device : Peripherals::sensors) {
    auto ready = sensor_device->readyToRead();
    std::this_thread::sleep_for(10ms);
    auto measured = ready ? sensor_device->read() : std::monostate{};
    std::this_thread::sleep_for(10ms);
  }
  //
  if (duration_cast<seconds>(nowtp.time_since_epoch()).count() % 60 == 0) {
    //
    // それぞれのセンサーによる記録値(ValueObject)を対応する記録インスタンスに入れる
    //
    struct Visitor {
      system_clock::time_point time_point;
      Visitor(system_clock::time_point arg) : time_point{arg} {}
      // Not Available (N/A)
      bool operator()(std::monostate) { return false; }
      // Bosch BME280: Temperature and Humidity and Pressure Sensor
      bool operator()(const Sensor::Bme280 &in) {
        Sensor::MeasurementBme280 m = {time_point, in};
        Telemetry::pushMessage(m);
        Application::measurements_database.insert(m);
        return true;
      }
      // Sensirion SGP30: Air Quality Sensor
      bool operator()(const Sensor::Sgp30 &in) {
        Sensor::MeasurementSgp30 m = {time_point, in};
        Telemetry::pushMessage(m);
        Application::measurements_database.insert(m);
        return true;
      }
      // Sensirion SCD30: NDIR CO2 and Temperature and Humidity Sensor
      bool operator()(const Sensor::Scd30 &in) {
        Sensor::MeasurementScd30 m = {time_point, in};
        // CO2の値でLEDの色を変える
        RgbLed::getInstance()->fill(
            RgbLed::colorFromCarbonDioxide(in.co2.value));
        //
        Telemetry::pushMessage(m);
        Application::measurements_database.insert(m);
        return true;
      }
      // Sensirion SCD41: PASens CO2 and Temperature and Humidity Sensor
      bool operator()(const Sensor::Scd41 &in) {
        Sensor::MeasurementScd41 m = {time_point, in};
        Telemetry::pushMessage(m);
        Application::measurements_database.insert(m);
        return true;
      }
      // M5Stack ENV.iii unit: Temperature and Humidity and Pressure Sensor
      bool operator()(const Sensor::M5Env3 &in) {
        Sensor::MeasurementM5Env3 m = {time_point, in};
        Telemetry::pushMessage(m);
        Application::measurements_database.insert(m);
        return true;
      }
    };
    //
    // 毎分0秒時点の値を取り込む
    //
    bool success = true;
    for (auto &sensor_device : Peripherals::sensors) {
      bool result = std::visit(Visitor{nowtp}, sensor_device->calculateSMA());
      success = success && result;
    }
  }
}

//
// 低速度loop()関数
//
inline void low_speed_loop() {
  if (WiFi.status() != WL_CONNECTED) {
    // WiFiが接続されていない場合は接続する。
    if (!waitingForWiFiConnection(30s)) {
      M5_LOGE("WiFi connect failed.");
    }
  }

  if (!Telemetry::loopMqtt()) {
    // 再接続
    if (!Telemetry::init(Credentials.iothub_fqdn, Credentials.device_id,
                         Credentials.device_key)) {
      M5_LOGE("MQTT subscribe failed.");
    }
  }
}

//
// Arduinoのloop()関数
//
void loop() {
  high_speed_loop();
  //
  static steady_clock::time_point before_epoch{};
  auto now_epoch = steady_clock::now();
  if (now_epoch - before_epoch >= 29s) {
    low_speed_loop();
    before_epoch = now_epoch;
  } else if (Time::sync_completed()) {
    static system_clock::time_point before_meas_tp{};
    static system_clock::time_point before_db_tp{};
    auto now_tp = system_clock::now();
    if (now_tp - before_meas_tp >= 1s) {
      measurements_loop(now_tp);
      before_meas_tp = now_tp;
    } else if (now_tp - before_db_tp >= 333s) {
      // データベースの整理
      system_clock::time_point tp = now_tp - minutes(Gui::CHART_X_POINT_COUNT);
      if (Application::measurements_database
              .delete_old_measurements_from_database(
                  std::chrono::floor<minutes>(tp)) == false) {
        M5_LOGE("delete old measurements failed.");
      }
      before_db_tp = now_tp;
    }
  }
}
