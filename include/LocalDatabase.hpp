// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "Application.hpp"
#include "value_types.hpp"
#include <ctime>
#include <functional>
#include <sqlite3.h>
#include <string>

//
//
//
class LocalDatabase final {
  bool _available{false};
  std::string_view sqlite3_filename{};
  sqlite3 *database{nullptr};

public:
  constexpr static const uint_fast8_t RETRY_COUNT = 100;
  //
  using RowId = int64_t;
  RowId rowid_temperature{-1};
  RowId rowid_relative_humidity{-1};
  RowId rowid_pressure{-1};
  RowId rowid_carbon_dioxide{-1};
  RowId rowid_total_voc{-1};
  //
  LocalDatabase(std::string_view filename) noexcept
      : rowid_temperature{-1},
        rowid_relative_humidity{-1},
        rowid_pressure{-1},
        rowid_carbon_dioxide{-1},
        rowid_total_voc{-1},
        _available{false},
        sqlite3_filename{filename},
        database{nullptr} {}
  ~LocalDatabase() noexcept { terminate(); }
  //
  bool available() const noexcept { return _available; }
  //
  bool begin() noexcept;
  //
  void terminate() noexcept;
  //
  bool insert(const MeasurementBme280 &in);
  bool insert(const MeasurementSgp30 &in);
  bool insert(const MeasurementScd30 &in);
  bool insert(const MeasurementScd41 &in);
  bool insert(const MeasurementM5Env3 &in);
  //
  RowId insert_temperature(SensorId sensor_id, std::time_t at, DegC degc);
  RowId insert_relative_humidity(SensorId sensor_id, std::time_t at, PctRH rh);
  RowId insert_pressure(SensorId sensor_id, std::time_t at, HectoPa hpa);
  RowId insert_carbon_dioxide(SensorId sensor_id, std::time_t at, Ppm ppm,
                              const uint16_t *baseline);
  RowId insert_total_voc(SensorId sensor_id, std::time_t at, Ppb ppb,
                         const uint16_t *baseline);
  //
  using CallbackRowTimeAndFloat =
      std::function<bool(size_t counter, std::time_t at, float v)>;
  //
  size_t get_temperatures_desc(SensorId sensor_id, size_t limit,
                               CallbackRowTimeAndFloat callback);
  //
  size_t get_relative_humidities_desc(SensorId sensor_id, size_t limit,
                                      CallbackRowTimeAndFloat callback);
  //
  size_t get_pressures_desc(SensorId sensor_id, size_t limit,
                            CallbackRowTimeAndFloat callback);
  //
  using CallbackRowTimeAndUint16AndNullableUint16 = std::function<bool(
      size_t counter, std::time_t at, uint16_t v1, uint16_t v2, bool has_v2)>;
  //
  size_t
  get_carbon_deoxides_desc(SensorId sensor_id, size_t limit,
                           CallbackRowTimeAndUint16AndNullableUint16 callback);
  //
  size_t
  get_total_vocs_desc(SensorId sensor_id, size_t limit,
                      CallbackRowTimeAndUint16AndNullableUint16 callback);
  //
  std::tuple<bool, std::time_t, BaselineECo2>
  get_latest_baseline_eco2(SensorId sensor_id);
  //
  std::tuple<bool, std::time_t, BaselineTotalVoc>
  get_latest_baseline_total_voc(SensorId sensor_id);

private:
  void retry_failed() { _available = false; }
  //
  int64_t raw_insert_time_and_float(std::string_view query, uint64_t sensor_id,
                                    std::time_t time, float float_value);
  //
  int64_t raw_insert_time_and_uint16_and_nullable_uint16(
      std::string_view query, uint64_t sensor_id, std::time_t time,
      uint16_t uint16_value, const uint16_t *nullable_uint16_value);
  //
  size_t raw_get_n_desc_time_and_float(std::string_view query,
                                       uint64_t sensor_id, size_t limit,
                                       CallbackRowTimeAndFloat callback);
  //
  size_t raw_get_n_time_and_uint16_and_nullable_uint16(
      std::string_view query, uint64_t sensor_id, size_t limit,
      CallbackRowTimeAndUint16AndNullableUint16 callback);
  //
  std::tuple<bool, std::time_t, BaselineSGP30T>
  raw_get_latest_baseline(std::string_view query, uint64_t sensor_id);
};
