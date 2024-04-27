// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "Sensor.hpp"
#include "value_types.hpp"
#include <chrono>
#include <ctime>
#include <functional>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <string>

//
//
//
class Database final {
#if defined(SQLITE_ENABLE_MEMSYS5)
  constexpr static size_t ALLOCATE_DATABASE_HEAP_OF_BYTES = 2 * 1024 * 1024;
  constexpr static size_t SIZE_OF_MINIMUM_HEAP_REQUEST = 32;
  void *_heap_memory_for_database{nullptr};
#endif
  bool _available{false};
  sqlite3 *sqlite3_db{nullptr};
  //
  std::optional<Sensor::MeasurementBme280> _latestMeasurementBme280{};
  std::optional<Sensor::MeasurementSgp30> _latestMeasurementSgp30{};
  std::optional<Sensor::MeasurementScd30> _latestMeasurementScd30{};
  std::optional<Sensor::MeasurementScd41> _latestMeasurementScd41{};
  std::optional<Sensor::MeasurementM5Env3> _latestMeasurementM5Env3{};

public:
  using system_clock = std::chrono::system_clock;
  //
  using TimePointAndDouble =
      std::tuple<SensorId, system_clock::time_point, double>;
  using TimePointAndIntAndOptInt =
      std::tuple<SensorId, system_clock::time_point, uint16_t,
                 std::optional<uint16_t>>;
  using CallbackRowTimeAndDouble =
      std::function<bool(size_t counter, std::time_t at, double v)>;
  template <typename T>
  using ReadCallback = std::function<bool(size_t counter, T)>;

  using OrderBy = enum { OrderByAtAsc = 0, OrderByAtDesc = 1 };

  constexpr static const uint_fast8_t RETRY_COUNT = 100;
  //
  Database() noexcept {}
  ~Database() noexcept {
    terminate();
#if defined(SQLITE_ENABLE_MEMSYS5)
    free(_heap_memory_for_database);
#endif
  }
  //
  bool available() const noexcept { return _available; }
  //
  bool begin() noexcept;
  //
  void terminate() noexcept;
  //
  bool delete_old_measurements_from_database(
      system_clock::time_point delete_of_older_than_tp);
  //
  bool insert(const Sensor::MeasurementBme280 &m);
  bool insert(const Sensor::MeasurementSgp30 &m);
  bool insert(const Sensor::MeasurementScd30 &m);
  bool insert(const Sensor::MeasurementScd41 &m);
  bool insert(const Sensor::MeasurementM5Env3 &m);
  //
  std::optional<Sensor::MeasurementBme280> getLatestMeasurementBme280() {
    return _latestMeasurementBme280;
  }
  std::optional<Sensor::MeasurementSgp30> getLatestMeasurementSgp30() {
    return _latestMeasurementSgp30;
  }
  std::optional<Sensor::MeasurementScd30> getLatestMeasurementScd30() {
    return _latestMeasurementScd30;
  }
  std::optional<Sensor::MeasurementScd41> getLatestMeasurementScd41() {
    return _latestMeasurementScd41;
  }
  std::optional<Sensor::MeasurementM5Env3> getLatestMeasurementM5Env3() {
    return _latestMeasurementM5Env3;
  }

public:
  //
  //
  //
  bool insert_temperature(SensorId sensor_id, system_clock::time_point at,
                          DegC degc);
  bool insert_relative_humidity(SensorId sensor_id, system_clock::time_point at,
                                PctRH rh);
  bool insert_pressure(SensorId sensor_id, system_clock::time_point at,
                       HectoPa hpa);
  bool insert_carbon_dioxide(SensorId sensor_id, system_clock::time_point at,
                             Ppm ppm, std::optional<uint16_t> baseline);
  bool insert_total_voc(SensorId sensor_id, system_clock::time_point at,
                        Ppb ppb, std::optional<uint16_t> baseline);
  //
  //
  //
  size_t read_temperatures(OrderBy order, system_clock::time_point at_begin,
                           ReadCallback<TimePointAndDouble> callback);
  size_t read_temperatures(OrderBy order, SensorId sensor_id, size_t limit,
                           ReadCallback<TimePointAndDouble> callback);
  std::vector<TimePointAndDouble>
  read_temperatures(OrderBy order, SensorId sensor_id, size_t limit);
  size_t read_relative_humidities(OrderBy order, SensorId sensor_id,
                                  size_t limit,
                                  ReadCallback<TimePointAndDouble> callback);
  std::vector<TimePointAndDouble>
  read_relative_humidities(OrderBy order, SensorId sensor_id, size_t limit);
  size_t read_pressures(OrderBy order, SensorId sensor_id, size_t limit,
                        ReadCallback<TimePointAndDouble> callback);
  std::vector<TimePointAndDouble>
  read_pressures(OrderBy order, SensorId sensor_id, size_t limit);
  size_t read_carbon_deoxides(OrderBy order, SensorId sensor_id, size_t limit,
                              ReadCallback<TimePointAndIntAndOptInt> callback);
  std::vector<TimePointAndIntAndOptInt>
  read_carbon_deoxides(OrderBy order, SensorId sensor_id, size_t limit);
  size_t read_total_vocs(OrderBy order, SensorId sensor_id, size_t limit,
                         ReadCallback<TimePointAndIntAndOptInt> callback);
  std::vector<TimePointAndIntAndOptInt>
  read_total_vocs(OrderBy order, SensorId sensor_id, size_t limit);

private:
  void retry_failed() { _available = false; }
  //
  bool insert_values(std::string_view query,
                     TimePointAndDouble values_to_insert);
  //
  bool insert_values(std::string_view query,
                     TimePointAndIntAndOptInt values_to_insert);
  //
  size_t read_values(std::string_view query,
                     std::tuple<system_clock::time_point, OrderBy> placeholder,
                     ReadCallback<TimePointAndDouble> callback);
  size_t read_values(std::string_view query,
                     std::tuple<SensorId, OrderBy, size_t> placeholder,
                     ReadCallback<TimePointAndDouble> callback);
  //
  size_t read_values(std::string_view query,
                     std::tuple<SensorId, OrderBy, size_t> placeholder,
                     ReadCallback<TimePointAndIntAndOptInt> callback);
};
