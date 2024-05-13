// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "Application.hpp"
#include "Database.hpp"
#include "Gui.hpp"
#include "RgbLed.hpp"
#include "Telemetry.hpp"
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
#include <queue>
#include <string>
#include <tuple>
#include <variant>

#include <M5Unified.h>

using namespace std::literals::string_literals;
using namespace std::chrono;
using namespace std::chrono_literals;

//
// Arduinoのsetup()関数
//
void setup() {
  constexpr auto TIMEOUT{3s};

  // file system init
  if (LittleFS.begin(true)) {
    M5_LOGI("filesystem status : %lu / %lu.", LittleFS.usedBytes(),
            LittleFS.totalBytes());
  } else {
    M5_LOGE("filesystem inititalize failed.");
    M5.Display.print("filesystem inititalize failed.");
    std::this_thread::sleep_for(1min);
    esp_system_abort("filesystem inititalize failed.");
  }

  // initializing M5Stack Core2 with M5Unified
  {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Power.setVibration(0); // stop the vibration
  }

  //
  if (new Application(M5.Display)) {
    Application::getInstance()->startup();
  } else {
    M5_LOGE("out of memory");
    M5.Display.print("out of memory.");
    std::this_thread::sleep_for(1min);
    esp_system_abort("out of memory.");
  }
}

//
// 測定用
//
class Sensing final {
  //
  // それぞれの測定値毎にIoTHubに送信＆データーベースに入れる
  //
  struct Visitor {
    system_clock::time_point time_point;
    Visitor(system_clock::time_point arg) : time_point{arg} {}
    // Not Available (N/A)
    bool operator()(std::monostate) { return false; }
    // Bosch BME280: Temperature and Humidity and Pressure Sensor
    bool operator()(const Sensor::Bme280 &in) {
      Sensor::MeasurementBme280 m = {time_point, in};
      Application::getTelemetry().enqueue(m);
      Application::getMeasurementsDatabase().insert(m);
      return true;
    }
    // Sensirion SGP30: Air Quality Sensor
    bool operator()(const Sensor::Sgp30 &in) {
      Sensor::MeasurementSgp30 m = {time_point, in};
      Application::getTelemetry().enqueue(m);
      Application::getMeasurementsDatabase().insert(m);
      return true;
    }
    // Sensirion SCD30: NDIR CO2 and Temperature and Humidity Sensor
    bool operator()(const Sensor::Scd30 &in) {
      Sensor::MeasurementScd30 m = {time_point, in};
      // CO2の値でLEDの色を変える
      Application::getRgbLed().fill(
          RgbLed::colorFromCarbonDioxide(in.co2.value));
      //
      Application::getTelemetry().enqueue(m);
      Application::getMeasurementsDatabase().insert(m);
      return true;
    }
    // Sensirion SCD41: PASens CO2 and Temperature and Humidity Sensor
    bool operator()(const Sensor::Scd41 &in) {
      Sensor::MeasurementScd41 m = {time_point, in};
      Application::getTelemetry().enqueue(m);
      Application::getMeasurementsDatabase().insert(m);
      return true;
    }
    // M5Stack ENV.iii unit: Temperature and Humidity and Pressure Sensor
    bool operator()(const Sensor::M5Env3 &in) {
      Sensor::MeasurementM5Env3 m = {time_point, in};
      Application::getTelemetry().enqueue(m);
      Application::getMeasurementsDatabase().insert(m);
      return true;
    }
  };
  //
  using TimeAndMeasurement =
      std::pair<system_clock::time_point, Sensor::MeasuredValue>;
  // 測定値キュー
  std::queue<TimeAndMeasurement> _queue{};
  //
  system_clock::time_point next_queue_in_tp{};
  // 次回の実行予定時間
  system_clock::time_point next_run_tp{};

private:
  // 測定
  void measure() {
    for (auto &sensor_device : Application::getSensors()) {
      auto ready = sensor_device->readyToRead();
      std::this_thread::sleep_for(2ms);
      auto measured = ready ? sensor_device->read() : std::monostate{};
      std::this_thread::sleep_for(2ms);
    }
  }

  // 現在値をキューに入れる
  void queueIn(system_clock::time_point nowtp) {
    for (auto &sensor_device : Application::getSensors()) {
      _queue.emplace(std::make_pair(nowtp, sensor_device->calculateSMA()));
    }
  }

  // キューに値があれば, IoTHubに送信＆データーベースに入れる
  void queueOut() {
    if (_queue.empty() == false) {
      auto &[tp, m] = _queue.front();
      std::visit(Visitor{tp}, m);
      _queue.pop();
    }
  }

public:
  //
  Sensing(system_clock::time_point nowtp) {
    auto extra_sec =
        std::chrono::duration_cast<seconds>(nowtp.time_since_epoch()) % 60s;
    //
    next_queue_in_tp = nowtp + 1min - extra_sec;
    next_run_tp = nowtp + 1s; // 次回の実行予定時間
  }
  // 測定関数
  void exec(system_clock::time_point nowtp) {
    if (nowtp >= next_queue_in_tp) {
      //
      auto extra_sec =
          std::chrono::duration_cast<seconds>(nowtp.time_since_epoch()) % 60s;
      next_queue_in_tp = nowtp + 1min - extra_sec;
      //
      queueIn(nowtp); // 現在値をキューに入れる
    } else if (nowtp >= next_run_tp) {
      next_run_tp = nowtp + 1s; // 次回の実行予定時間
      measure();                // 測定
      queueOut();               // コミット
    }
  }
};

//
// 低速度loop()関数
//
inline void low_speed_loop() {
  if (WiFi.status() != WL_CONNECTED) {
    // WiFiが接続されていない場合は接続する。
    WiFi.begin(Credentials.wifi_ssid, Credentials.wifi_password);
  } else if (Application::getTelemetry().isConnected() == false) {
    static steady_clock::time_point before{steady_clock::now()};
    // 再接続
    if (steady_clock::now() - before > 1min) {
      before = steady_clock::now();
      //
      if (!Application::getTelemetry().begin(Credentials.iothub_fqdn,
                                             Credentials.device_id,
                                             Credentials.device_key)) {
        M5_LOGE("MQTT subscribe failed.");
      }
    }
  } else {
    Application::getTelemetry().exec();
  }
}

//
// Arduinoのloop()関数
//
void loop() {
  //
  {
    ArduinoOTA.handle();
    M5.update();
    if (M5.BtnA.wasPressed()) {
      Application::getGui().movePrev();
    } else if (M5.BtnB.wasPressed()) {
      Application::getGui().home();
    } else if (M5.BtnC.wasPressed()) {
      Application::getGui().moveNext();
    }
  }
  //
  if (Application::isTimeSynced() == false) {
    return;
  }
  static std::unique_ptr<Sensing> sensing{};
  static system_clock::time_point before_db_tp{};
  auto now_tp = system_clock::now();
  //
  while (!sensing) {
    sensing = std::make_unique<Sensing>(now_tp);
  }
  // 随時測定する
  sensing->exec(now_tp);
  //
  if (now_tp - before_db_tp >= 333s) {
    // データベースの整理
    system_clock::time_point tp = now_tp - minutes(Gui::CHART_X_POINT_COUNT);
    if (Application::getMeasurementsDatabase()
            .delete_old_measurements_from_database(
                std::chrono::floor<minutes>(tp)) == false) {
      M5_LOGE("delete old measurements failed.");
    }
    before_db_tp = now_tp;
  }
  //
  static steady_clock::time_point before_epoch{};
  auto now_epoch = steady_clock::now();
  if (now_epoch - before_epoch >= 3s) {
    low_speed_loop();
    before_epoch = now_epoch;
  }
}
