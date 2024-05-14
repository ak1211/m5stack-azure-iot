// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "Sensor.hpp"
#include <chrono>
#include <queue>
#include <tuple>

//
// 測定用
//
class MeasuringTask final {
  struct Visitor;
  using TimeAndMeasurement =
      std::pair<std::chrono::system_clock::time_point, Sensor::MeasuredValue>;
  // 測定値キュー
  std::queue<TimeAndMeasurement> _queue{};
  //
  std::chrono::system_clock::time_point next_queue_in_tp{};
  // 次回の実行予定時間
  std::chrono::system_clock::time_point next_run_tp{};
  // 測定
  void measure();
  // 現在値をキューに入れる
  void queueIn(std::chrono::system_clock::time_point nowtp);
  // キューに値があれば, IoTHubに送信＆データーベースに入れる
  void queueOut();

public:
  //
  bool begin(std::chrono::system_clock::time_point nowtp);
  // 測定関数
  void task_handler(std::chrono::system_clock::time_point nowtp);
};
