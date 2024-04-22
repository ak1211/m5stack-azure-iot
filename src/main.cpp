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

using namespace std::literals::string_literals;
using namespace std::chrono;
using namespace std::chrono_literals;

// 起動時のログ
Application::BootLog Application::boot_log{};

//
Database measurements_database(Application::sqlite3_file_name);

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
// get baseline for "Sensirion SGP30: Air Quality Sensor" from database
//
using BaselinesSGP30 = std::pair<BaselineECo2, BaselineTotalVoc>;
static std::optional<BaselinesSGP30> getLatestBaselineSGP30() {
  std::optional<BaselineECo2> baseline_eco2{std::nullopt};
  std::optional<BaselineTotalVoc> baseline_tvoc{std::nullopt};
  //
  if (auto baseline = measurements_database.get_latest_baseline_eco2(
          SensorId(Peripherals::SENSOR_DESCRIPTOR_SGP30));
      baseline.has_value()) {
    auto [at, eco2] = *baseline;
    baseline_eco2 = eco2;
    M5_LOGD("latest_baseline_eco2: at(%d), baseline(%d)", at, eco2);
  }
  if (auto baseline = measurements_database.get_latest_baseline_total_voc(
          SensorId(Peripherals::SENSOR_DESCRIPTOR_SGP30));
      baseline.has_value()) {
    auto [at, tvoc] = *baseline;
    baseline_tvoc = tvoc;
    M5_LOGD("latest_baseline_total_voc: at(%d), baseline(%d)", at, tvoc);
  }
  if (baseline_eco2.has_value() && baseline_tvoc.has_value()) {
    return std::make_pair(*baseline_eco2, *baseline_tvoc);
  } else {
    M5_LOGE("SGP30 baseline was not recorded.");
    return std::nullopt;
  }
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
  constexpr auto TIMEOUT{5s};
  // initializing M5Stack Core2 with M5Unified
  auto cfg = M5.config();
  M5.begin(cfg);
  // PSRAM init
  if (!psramInit()) {
    M5_LOGE("PSRAM inititalize failed.");
    goto fatal_error;
  }
  // initialize the 'arduino Wire class'
  Wire.end();
  Wire.begin(M5.Ex_I2C.getSDA(), M5.Ex_I2C.getSCL());
  // Display
  M5.Display.setColorDepth(LV_COLOR_DEPTH);
  M5.Display.setBrightness(200);
  //
  M5.Power.setChargeCurrent(280);
  // stop the vibration
  M5.Power.setVibration(0);

  // init WiFi with Station mode
  WiFi.onEvent(gotWiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.begin(Credentials.wifi_ssid, Credentials.wifi_password);

  // init RGB LEDS
  if (auto rgbled = new RgbLed(); rgbled) {
    rgbled->begin();
    rgbled->setBrightness(80);
    rgbled->fill(CRGB::White);
  }
  // init peripherals
  Peripherals::init(Wire, M5.Ex_I2C.getSDA(), M5.Ex_I2C.getSCL());
  // init GUI
  if (auto gui = new Gui(M5.Display); gui) {
    if (gui->begin()) {
      void();
    } else {
      goto fatal_error;
    }
  } else {
    goto fatal_error;
  }

  // create RTOS task
  static TaskHandle_t rtos_task_handle{};
  xTaskCreatePinnedToCore(
      [](void *arg) -> void {
        while (true) {
          lv_timer_handler_run_in_period(10);
          delay(10);
        }
      },
      "Task:LVGL", 8192, nullptr, 10, &rtos_task_handle, ARDUINO_RUNNING_CORE);

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
  if constexpr (true) {
    logging("init Database.");
    if (!measurements_database.begin()) {
      logging("Database is not available.");
    }
  }
  // get baseline for "Sensirion SGP30: Air Quality Sensor" from database
  {
    std::optional<BaselinesSGP30> baseline_sgp30{std::nullopt};
    if constexpr (true) {
      baseline_sgp30 = getLatestBaselineSGP30();
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
          //
          if (ok = sensor_device->init(); ok) {
            M5_LOGI("%s init success.", desc.str().c_str());
            break;
          }
          M5_LOGI("%s init failed, retry", desc.str().c_str());
          std::this_thread::sleep_for(100ms);
        }
        if (ok) {
          if (sensor_device->getSensorDescriptor() ==
              Peripherals::SENSOR_DESCRIPTOR_SGP30) {
            // センサーがSGP30の場合のみ
            if (baseline_sgp30.has_value()) {
              auto [eco2, tvoc] = *baseline_sgp30;
              auto sensor_ptr =
                  static_cast<Sensor::Sgp30Device *>(sensor_device.get());
              sensor_ptr->setIAQBaseline(eco2, tvoc);
            } else {
              M5_LOGI(
                  "SGP30 built-in Automatic Baseline Correction algorithm.");
            }
          }
        } else {
          oss.str("");
          oss << desc.str() << " sensor not found.";
          logging(oss.str().c_str());
        }
      }
      // 初期化に失敗した(つまり接続されていない)センサーをsensorsベクタから削除する
      auto result =
          std::remove_if(Peripherals::sensors.begin(),
                         Peripherals::sensors.end(), [](auto &sensor_device) {
                           auto active = sensor_device->available();
                           return active == false;
                         });
      Peripherals::sensors.erase(result, Peripherals::sensors.end());
    }
  }
  //
  logging("setup done.");
  Gui::getInstance()->startUi();

  return; // Successfully exit.
  //
fatal_error:
  M5.Display.clear();
  M5.Display.print("fatal error.");
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
// 低速度loop()関数
//
inline void low_speed_loop(system_clock::time_point nowtp) {
  // 測定
  for (auto &sensor_device : Peripherals::sensors) {
    auto measured =
        sensor_device->readyToRead() ? sensor_device->read() : std::monostate{};
  }
  //
  if (duration_cast<seconds>(nowtp.time_since_epoch()).count() % 60 == 0) {
    //
    // それぞれのセンサーによる記録値(ValueObject)を対応する記録インスタンスに入れる
    //
    struct Visitor {
      system_clock::time_point tp;
      Visitor(system_clock::time_point arg) : tp{arg} {}
      // Not Available (N/A)
      bool operator()(std::monostate) { return false; }
      // Bosch BME280: Temperature and Humidity and Pressure Sensor
      bool operator()(const Sensor::Bme280 &in) {
        MeasurementBme280 m = {tp, in};
        Telemetry::pushMessage(m);
        measurements_database.insert(m);
        return true;
      }
      // Sensirion SGP30: Air Quality Sensor
      bool operator()(const Sensor::Sgp30 &in) {
        MeasurementSgp30 m = {tp, in};
        Telemetry::pushMessage(m);
        measurements_database.insert(m);
        return true;
      }
      // Sensirion SCD30: NDIR CO2 and Temperature and Humidity Sensor
      bool operator()(const Sensor::Scd30 &in) {
        MeasurementScd30 m = {tp, in};
        // CO2の値でLEDの色を変える
        RgbLed::getInstance()->fill(
            RgbLed::colorFromCarbonDioxide(in.co2.value));
        //
        Telemetry::pushMessage(m);
        measurements_database.insert(m);
        return true;
      }
      // Sensirion SCD41: PASens CO2 and Temperature and Humidity Sensor
      bool operator()(const Sensor::Scd41 &in) {
        MeasurementScd41 m = {tp, in};
        Telemetry::pushMessage(m);
        measurements_database.insert(m);
        return true;
      }
      // M5Stack ENV.iii unit: Temperature and Humidity and Pressure Sensor
      bool operator()(const Sensor::M5Env3 &in) {
        MeasurementM5Env3 m = {tp, in};
        Telemetry::pushMessage(m);
        measurements_database.insert(m);
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
  } else if (WiFi.status() != WL_CONNECTED) {
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
  using namespace std::chrono;
  static system_clock::time_point before{};
  if (Time::sync_completed()) {
    auto nowtp = system_clock::now();
    //
    if (nowtp - before >= 1s) {
      low_speed_loop(nowtp);
      before = nowtp;
    }
  }
}
