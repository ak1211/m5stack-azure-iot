// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "MeasuringTask.hpp"
#include "Application.hpp"
#include "Database.hpp"
#include "Sensor.hpp"
#include <chrono>
#include <future>

using namespace std::chrono;
using namespace std::chrono_literals;

//
// それぞれの測定値毎にIoTHubに送信＆データーベースに入れる
//
struct MeasuringTask::Visitor {
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
    Application::getRgbLed().fill(RgbLed::colorFromCarbonDioxide(in.co2.value));
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

// 測定
void MeasuringTask::measure() {
  for (auto &sensor_device : Application::getSensors()) {
    std::this_thread::yield();
    if (sensor_device->readyToRead()) {
      std::this_thread::sleep_for(1ms);
      sensor_device->read();
    }
  }
}

// 現在値をキューに入れる
void MeasuringTask::queueIn(system_clock::time_point nowtp) {
  for (auto &sensor_device : Application::getSensors()) {
    _queue.emplace(std::make_pair(nowtp, sensor_device->calculateSMA()));
  }
}

// キューに値があれば, IoTHubに送信＆データーベースに入れる
void MeasuringTask::queueOut() {
  if (_queue.empty() == false) {
    auto &[tp, m] = _queue.front();
    std::visit(Visitor{tp}, m);
    _queue.pop();
  }
}

//
bool MeasuringTask::begin(std::chrono::system_clock::time_point nowtp) {
  auto extra_sec =
      std::chrono::duration_cast<seconds>(nowtp.time_since_epoch()) % 60s;
  //
  next_queue_in_tp = nowtp + 1min - extra_sec;
  next_run_tp = nowtp + 1s; // 次回の実行予定時間
  return true;
}

// 測定関数
void MeasuringTask::task_handler(std::chrono::system_clock::time_point nowtp) {
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
