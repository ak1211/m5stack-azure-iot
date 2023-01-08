// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once

#include "sensor.hpp"
#include "value_types.hpp"
#include <functional>
#include <sqlite3.h>
#include <string>

//
//
//
class LocalDatabase {
public:
  constexpr static const uint_fast8_t RETRY_COUNT = 100;
  //
  using RowId = int64_t;
  RowId rowid_temperature;
  RowId rowid_relative_humidity;
  RowId rowid_pressure;
  RowId rowid_carbon_dioxide;
  RowId rowid_total_voc;
  //
  LocalDatabase(std::string_view filename);
  //
  ~LocalDatabase();
  //
  bool available() { return _available; }
  //
  bool begin();
  //
  void terminate();
  //
  bool insert(const TempHumiPres &);
  bool insert(const TvocEco2 &);
  bool insert(const Co2TempHumi &);
  //
  RowId insert_temperature(SensorId sensor_id, std::time_t at, DegC degc);
  RowId insert_relative_humidity(SensorId sensor_id, std::time_t at,
                                 RelHumidity rh);
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
  bool _available;
  std::string_view sqlite3_filename;
  sqlite3 *database;
  //
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
