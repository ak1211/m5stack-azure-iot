// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef LOCAL_DATABASE_HPP
#define LOCAL_DATABASE_HPP

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
  using RawId = int64_t;
  RawId rawid_temperature;
  RawId rawid_relative_humidity;
  RawId rawid_pressure;
  RawId rawid_carbon_dioxide;
  RawId rawid_total_voc;
  //
  LocalDatabase(const std::string &filename);
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
  RawId insert_temperature(uint64_t sensor_id, std::time_t at, DegC degc);
  RawId insert_relative_humidity(uint64_t sensor_id, std::time_t at, PcRH rh);
  RawId insert_pressure(uint64_t sensor_id, std::time_t at, HPa hpa);
  RawId insert_carbon_dioxide(uint64_t sensor_id, std::time_t at, Ppm ppm,
                              const uint16_t *baseline);
  RawId insert_total_voc(uint64_t sensor_id, std::time_t at, Ppb ppb,
                         const uint16_t *baseline);
  //
  using CallbackRowTimeAndFloat =
      std::function<bool(size_t counter, std::time_t at, float v)>;
  //
  size_t get_temperatures_desc(uint64_t sensor_id, size_t limit,
                               CallbackRowTimeAndFloat callback);
  //
  size_t get_relative_humidities_desc(uint64_t sensor_id, size_t limit,
                                      CallbackRowTimeAndFloat callback);
  //
  size_t get_pressures_desc(uint64_t sensor_id, size_t limit,
                            CallbackRowTimeAndFloat callback);
  //
  using CallbackRowTimeAndUint16AndNullableUint16 = std::function<bool(
      size_t counter, std::time_t at, uint16_t v1, uint16_t v2, bool has_v2)>;
  //
  size_t
  get_carbon_deoxides_desc(uint64_t sensor_id, size_t limit,
                           CallbackRowTimeAndUint16AndNullableUint16 callback);
  //
  size_t
  get_total_vocs_desc(uint64_t sensor_id, size_t limit,
                      CallbackRowTimeAndUint16AndNullableUint16 callback);
  //
  std::tuple<bool, std::time_t, BaselineECo2>
  get_latest_baseline_eco2(uint64_t sensor_id);
  //
  std::tuple<bool, std::time_t, BaselineTotalVoc>
  get_latest_baseline_total_voc(uint64_t sensor_id);

private:
  bool _available;
  const std::string sqlite3_filename;
  sqlite3 *database;
  //
  static int64_t raw_insert_time_and_float(sqlite3 *database, const char *query,
                                           uint64_t sensor_id, std::time_t time,
                                           float float_value);
  //
  static int64_t raw_insert_time_and_uint16_and_nullable_uint16(
      sqlite3 *database, const char *query, uint64_t sensor_id,
      std::time_t time, uint16_t uint16_value,
      const uint16_t *nullable_uint16_value);
  //
  static size_t raw_get_n_desc_time_and_float(sqlite3 *database,
                                              const char *query,
                                              uint64_t sensor_id, size_t limit,
                                              CallbackRowTimeAndFloat callback);
  //
  static size_t raw_get_n_time_and_uint16_and_nullable_uint16(
      sqlite3 *database, const char *query, uint64_t sensor_id, size_t limit,
      CallbackRowTimeAndUint16AndNullableUint16 callback);
  //
  static std::tuple<bool, std::time_t, BaselineSGP30T>
  raw_get_latest_baseline(sqlite3 *database, const char *query,
                          uint64_t sensor_id);
};

#endif // LOCAL_DATABASE_HPP
