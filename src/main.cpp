// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "Application.hpp"
#include "BottomCaseLed.hpp"
#include "DataLoggingFile.hpp"
#include "Gui.hpp"
#include "LocalDatabase.hpp"
#include "Peripherals.hpp"
#include "SystemPower.hpp"
#include "Telemetry.hpp"
#include "Time.hpp"
#include "credentials.h"
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <Wire.h>
#include <chrono>
#include <ctime>
#include <esp_log.h>
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

// 記録インスタンス
std::unique_ptr<Application::HistoriesBme280> Application::historiesBme280{};
std::unique_ptr<Application::HistoriesSgp30> Application::historiesSgp30{};
std::unique_ptr<Application::HistoriesScd30> Application::historiesScd30{};
std::unique_ptr<Application::HistoriesScd41> Application::historiesScd41{};
std::unique_ptr<Application::HistoriesM5Env3> Application::historiesM5Env3{};
//
LocalDatabase local_database(Application::sqlite3_file_name);
//
DataLoggingFile data_logging_file(Application::data_log_file_name,
                                  Application::header_log_file_name);

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

//
// WiFi APへ接続確立を試みる
//
static bool connectToWiFi(std::string_view ssid, std::string_view password,
                          seconds TIMEOUT) {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.data(), password.data());
  bool ok{false};
  auto timeover{steady_clock::now() + TIMEOUT};
  while (!ok && steady_clock::now() < timeover) {
    ok = WiFi.status() == WL_CONNECTED;
    std::this_thread::sleep_for(10ms);
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
  if (auto [available, at, eco2] = local_database.get_latest_baseline_eco2(
          SensorId(Peripherals::SENSOR_DESCRIPTOR_SGP30));
      available) {
    baseline_eco2 = eco2;
    ESP_LOGD(MAIN, "latest_baseline_eco2: at(%d), baseline(%d)", at, eco2);
  }
  if (auto [available, at, tvoc] = local_database.get_latest_baseline_total_voc(
          SensorId(Peripherals::SENSOR_DESCRIPTOR_SGP30));
      available) {
    baseline_tvoc = tvoc;
    ESP_LOGD(MAIN, "latest_baseline_total_voc: at(%d), baseline(%d)", at, tvoc);
  }

  if (baseline_eco2.has_value() && baseline_tvoc.has_value()) {
    return std::make_pair(baseline_eco2.value(), baseline_tvoc.value());
  } else {
    ESP_LOGE(MAIN, "getLatestBaselines: failed.");
    return std::nullopt;
  }
}

//
// Arduinoのsetup()関数
//
void setup() {
  constexpr auto TIMEOUT{5s};
  // initializing M5Stack Core2 with M5Unified
  {
    auto cfg = M5.config();
    M5.begin(cfg);
    Wire.end();
    Wire.begin(M5.Ex_I2C.getSDA(), M5.Ex_I2C.getSCL());
    M5.Display.setColorDepth(16);
  }
  // init peripherals
  Peripherals::init(Wire, M5.Ex_I2C.getSDA(), M5.Ex_I2C.getSCL());
  // init SystemPower
  SystemPower::init();
  // init BottomCaseLed
  BottomCaseLed::init();
  // init GUI
  BottomCaseLed::showProgressive(CRGB::White);
  GUI::init();
  // init WiFi
  if constexpr (true) {
    BottomCaseLed::showProgressive(CRGB::White);
    GUI::showBootstrappingMessage("connect to WiFi AP. SSID:\""s +
                                  Credentials.wifi_ssid + "\""s);
    if (!connectToWiFi(Credentials.wifi_ssid, Credentials.wifi_password, 30s)) {
      GUI::showBootstrappingMessage("connect failed.");
    }
  }
  // init Time
  if constexpr (true) {
    BottomCaseLed::showProgressive(CRGB::White);
    GUI::showBootstrappingMessage("init time.");
    // initialize Time
    Time::init();
  }
  // init OTA update
  if constexpr (true) {
    BottomCaseLed::showProgressive(CRGB::White);
    GUI::showBootstrappingMessage("setup OTA update.");
    setupOTA();
  }
  // init LocalDatabase
  if constexpr (true) {
    BottomCaseLed::showProgressive(CRGB::White);
    GUI::showBootstrappingMessage("init LocalDatabase.");
    if (!local_database.begin()) {
      GUI::showBootstrappingMessage("LocalDatabase is not available.");
    }
  }
  // init DataLoggingFile
  if constexpr (true) {
    BottomCaseLed::showProgressive(CRGB::White);
    GUI::showBootstrappingMessage("init DataLoggingFile.");
    if (!data_logging_file.init()) {
      GUI::showBootstrappingMessage("DataLoggingFile is not available.");
    }
  }
  // get baseline for "Sensirion SGP30: Air Quality Sensor" from database
  std::optional<BaselinesSGP30> baseline_sgp30{std::nullopt};
  if constexpr (true) {
    BottomCaseLed::showProgressive(CRGB::White);
    baseline_sgp30 = getLatestBaselineSGP30();
  }
  // init sensors
  if constexpr (true) {
    ESP_LOGI(MAIN, "init sensors");
    for (auto &sensor_device : Peripherals::sensors) {
      BottomCaseLed::showProgressive(CRGB::White);
      //
      std::ostringstream oss;
      SensorDescriptor desc = sensor_device->getSensorDescriptor();
      oss << "init " << desc.str() << " sensor.";
      GUI::showBootstrappingMessage(oss.str().c_str());
      bool ok{false};
      auto timeover{steady_clock::now() + TIMEOUT};
      while (steady_clock::now() < timeover) {
        if (ok = sensor_device->init(); ok) {
          ESP_LOGI(MAIN, "%s init success.", desc.str().c_str());
          break;
        }
        ESP_LOGI(MAIN, "%s init failed, retry", desc.str().c_str());
        std::this_thread::sleep_for(10ms);
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
            ESP_LOGI(MAIN,
                     "SGP30 built-in Automatic Baseline Correction algorithm.");
          }
        }
      } else {
        oss.str("");
        oss << desc.str() << " sensor not found.";
        GUI::showBootstrappingMessage(oss.str().c_str());
      }
    }
    // 初期化に失敗した(つまり接続されていない)センサーをsensorsベクタから削除する
    auto result =
        std::remove_if(Peripherals::sensors.begin(), Peripherals::sensors.end(),
                       [](auto &sensor_device) {
                         auto active = sensor_device->active();
                         return active == false;
                       });
    Peripherals::sensors.erase(result, Peripherals::sensors.end());
    //
    // それぞれのセンサーに対応する記録インスタンスを準備する
    //
    using namespace Application;
    for (auto &sensor_device : Peripherals::sensors) {
      switch (SensorId(sensor_device->getSensorDescriptor())) {
      case SensorId(Peripherals::SENSOR_DESCRIPTOR_BME280):
        historiesBme280 = std::make_unique<HistoriesBme280>();
        break;
      case SensorId(Peripherals::SENSOR_DESCRIPTOR_SGP30):
        historiesSgp30 = std::make_unique<HistoriesSgp30>();
        break;
      case SensorId(Peripherals::SENSOR_DESCRIPTOR_SCD30):
        historiesScd30 = std::make_unique<HistoriesScd30>();
        break;
      case SensorId(Peripherals::SENSOR_DESCRIPTOR_SCD41):
        historiesScd41 = std::make_unique<HistoriesScd41>();
        break;
      case SensorId(Peripherals::SENSOR_DESCRIPTOR_M5ENV3):
        historiesM5Env3 = std::make_unique<HistoriesM5Env3>();
        break;
      default:
        ESP_LOGD(MAIN, "histories sensor_device initialization error.");
        GUI::showBootstrappingMessage(
            "histories sensor_device initialization error.");
        break;
      }
    }
  }
  // init MQTT
  if constexpr (true) {
    GUI::showBootstrappingMessage("connect MQTT.");
    bool ok{false};
    auto timeover{steady_clock::now() + TIMEOUT};
    while (!ok && steady_clock::now() < timeover) {
      BottomCaseLed::showProgressive(CRGB::White);
      ok = Telemetry::init(Credentials.iothub_fqdn, Credentials.device_id,
                           Credentials.device_key);
      std::this_thread::sleep_for(10ms);
    }
    if (!ok) {
      GUI::showBootstrappingMessage("connect failed.");
    }
  }
  // setup successful
  BottomCaseLed::offSignal();
  GUI::startUI();
}

//
// 高速度loop()関数
//
inline void high_speed_loop() {
  ArduinoOTA.handle();
  M5.update();
  lv_task_handler();
  if (M5.BtnA.wasPressed()) {
    GUI::movePrev();
  } else if (M5.BtnB.wasPressed()) {
    GUI::home();
  } else if (M5.BtnC.wasPressed()) {
    GUI::moveNext();
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
        if (Application::historiesBme280) {
          Application::historiesBme280->insert(m);
        } else {
          ESP_LOGE(MAIN, "histories buffer had not available");
        }
        Telemetry::pushMessage(m);
        local_database.insert(m);
        return true;
      }
      // Sensirion SGP30: Air Quality Sensor
      bool operator()(const Sensor::Sgp30 &in) {
        MeasurementSgp30 m = {tp, in};
        if (Application::historiesSgp30) {
          Application::historiesSgp30->insert(m);
        } else {
          ESP_LOGE(MAIN, "histories buffer had not available");
        }
        Telemetry::pushMessage(m);
        local_database.insert(m);
        return true;
      }
      // Sensirion SCD30: NDIR CO2 and Temperature and Humidity Sensor
      bool operator()(const Sensor::Scd30 &in) {
        MeasurementScd30 m = {tp, in};
        // CO2の値でLEDの色を変える
        BottomCaseLed::showSignal(in.co2);
        //
        if (Application::historiesScd30) {
          Application::historiesScd30->insert(m);
        } else {
          ESP_LOGE(MAIN, "histories buffer had not available");
        }
        Telemetry::pushMessage(m);
        local_database.insert(m);
        return true;
      }
      // Sensirion SCD41: PASens CO2 and Temperature and Humidity Sensor
      bool operator()(const Sensor::Scd41 &in) {
        MeasurementScd41 m = {tp, in};
        if (Application::historiesScd41) {
          Application::historiesScd41->insert(m);
        } else {
          ESP_LOGE(MAIN, "histories buffer had not available");
        }
        Telemetry::pushMessage(m);
        local_database.insert(m);
        return true;
      }
      // M5Stack ENV.iii unit: Temperature and Humidity and Pressure Sensor
      bool operator()(const Sensor::M5Env3 &in) {
        MeasurementM5Env3 m = {tp, in};
        if (Application::historiesM5Env3) {
          Application::historiesM5Env3->insert(m);
        } else {
          ESP_LOGE(MAIN, "histories buffer had not available");
        }
        Telemetry::pushMessage(m);
        local_database.insert(m);
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
    // insert to local logging file
    if (success) {
      auto a = Application::historiesBme280
                   ? Application::historiesBme280->getLatestValue()
                   : std::nullopt;
      auto b = Application::historiesSgp30
                   ? Application::historiesSgp30->getLatestValue()
                   : std::nullopt;
      auto c = Application::historiesScd30
                   ? Application::historiesScd30->getLatestValue()
                   : std::nullopt;
      //
      // TODO: add M5Env3 and SCD41
      //
      if (a && b && c) {
        data_logging_file.write_data_to_log_file(a->first, a->second, b->second,
                                                 c->second);
      }
    }
  }
  Telemetry::loopMqtt();
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
