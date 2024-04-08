// Copyright (c) 2022 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "Histories.hpp"
#include "Sensor.hpp"

#include <memory>

// ログ出し用
constexpr static char MAIN[] = "MAIN";
constexpr static char SEND[] = "SEND";
constexpr static char RECEIVE[] = "RECEIVE";
constexpr static char TELEMETRY[] = "TELEMETRY";

// 測定時間と測定値のペア
using MeasurementBme280 =
    std::pair<std::chrono::system_clock::time_point, Sensor::Bme280>;
using MeasurementSgp30 =
    std::pair<std::chrono::system_clock::time_point, Sensor::Sgp30>;
using MeasurementScd30 =
    std::pair<std::chrono::system_clock::time_point, Sensor::Scd30>;
using MeasurementScd41 =
    std::pair<std::chrono::system_clock::time_point, Sensor::Scd41>;
using MeasurementM5Env3 =
    std::pair<std::chrono::system_clock::time_point, Sensor::M5Env3>;

//
namespace Application {
//
// 起動時のログ
//
class BootLog {
  std::vector<char> message_cstring{'\0'};

public:
  //
  void logging(std::string_view sv) noexcept {
    message_cstring.pop_back();
    std::copy(sv.begin(), sv.end(), std::back_inserter(message_cstring));
    message_cstring.push_back('\n');
    message_cstring.push_back('\0');
  }
  //
  const char *c_str() const noexcept { return message_cstring.data(); }
  //
  size_t size() const noexcept { return message_cstring.size(); }
};
extern BootLog boot_log;

//
// 各センサーの測定値の記録用
// 現在のところ1分毎の記録なので HISTORY_BUFFER_SIZE 分の記録をする
//
constexpr static auto HISTORY_BUFFER_SIZE = 180;
using HistoriesBme280 = Histories<MeasurementBme280, HISTORY_BUFFER_SIZE>;
using HistoriesSgp30 = Histories<MeasurementSgp30, HISTORY_BUFFER_SIZE>;
using HistoriesScd30 = Histories<MeasurementScd30, HISTORY_BUFFER_SIZE>;
using HistoriesScd41 = Histories<MeasurementScd41, HISTORY_BUFFER_SIZE>;
using HistoriesM5Env3 = Histories<MeasurementM5Env3, HISTORY_BUFFER_SIZE>;
//
// 各センサーの測定値の記録インスタンス
//
extern std::unique_ptr<HistoriesBme280> historiesBme280;
extern std::unique_ptr<HistoriesSgp30> historiesSgp30;
extern std::unique_ptr<HistoriesScd30> historiesScd30;
extern std::unique_ptr<HistoriesScd41> historiesScd41;
extern std::unique_ptr<HistoriesM5Env3> historiesM5Env3;
//
constexpr static auto sqlite3_file_name =
    std::string_view{"/sd/measurements.sqlite3"};
//
constexpr static auto data_log_file_name =
    std::string_view{"/data-logging.csv"};
constexpr static auto header_log_file_name =
    std::string_view{"/header-data-logging.csv"};
} // namespace Application
