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
  bool _available{false};
  constexpr static size_t SIZE_OF_HEAP_MEMORY = 2 * 1024 * 1024;
  constexpr static size_t SIZE_OF_MINIMUM_HEAP_REQUEST = 256;
  void *_heap_memory_for_database{nullptr};
  sqlite3 *sqlite3_db{nullptr};

public:
  using system_clock = std::chrono::system_clock;
  using RowId = int64_t;
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
  std::optional<RowId> rowid_temperature{};
  std::optional<RowId> rowid_relative_humidity{};
  std::optional<RowId> rowid_pressure{};
  std::optional<RowId> rowid_carbon_dioxide{};
  std::optional<RowId> rowid_total_voc{};
  //
  //  Table::Temperature table_temperature;
  //
  Database() noexcept {}
  ~Database() noexcept {
    terminate();
    free(_heap_memory_for_database);
  }
  //
  bool available() const noexcept { return _available; }
  //
  bool begin() noexcept;
  //
  void terminate() noexcept;
  //
  bool insert(const Sensor::MeasurementBme280 &m);
  bool insert(const Sensor::MeasurementSgp30 &m);
  bool insert(const Sensor::MeasurementScd30 &m);
  bool insert(const Sensor::MeasurementScd41 &m);
  bool insert(const Sensor::MeasurementM5Env3 &m);

public:
  //
  std::optional<RowId> insert_temperature(SensorId sensor_id,
                                          system_clock::time_point at,
                                          DegC degc);
  std::optional<RowId> insert_relative_humidity(SensorId sensor_id,
                                                system_clock::time_point at,
                                                PctRH rh);
  std::optional<RowId>
  insert_pressure(SensorId sensor_id, system_clock::time_point at, HectoPa hpa);
  std::optional<RowId> insert_carbon_dioxide(SensorId sensor_id,
                                             system_clock::time_point at,
                                             Ppm ppm,
                                             std::optional<uint16_t> baseline);
  std::optional<RowId> insert_total_voc(SensorId sensor_id,
                                        system_clock::time_point at, Ppb ppb,
                                        std::optional<uint16_t> baseline);
  //
  size_t read_temperatures(OrderBy order, SensorId sensor_id, size_t limit,
                           ReadCallback<TimePointAndDouble> callback);
  size_t read_relative_humidities(OrderBy order, SensorId sensor_id,
                                  size_t limit,
                                  ReadCallback<TimePointAndDouble> callback);
  size_t read_pressures(OrderBy order, SensorId sensor_id, size_t limit,
                        ReadCallback<TimePointAndDouble> callback);
  size_t read_carbon_deoxides(OrderBy order, SensorId sensor_id, size_t limit,
                              ReadCallback<TimePointAndIntAndOptInt> callback);
  size_t read_total_vocs(OrderBy order, SensorId sensor_id, size_t limit,
                         ReadCallback<TimePointAndIntAndOptInt> callback);
  //
  std::vector<TimePointAndDouble>
  read_temperatures(OrderBy order, SensorId sensor_id, size_t limit);
  std::vector<TimePointAndDouble>
  read_relative_humidities(OrderBy order, SensorId sensor_id, size_t limit);
  std::vector<TimePointAndDouble>
  read_pressures(OrderBy order, SensorId sensor_id, size_t limit);
  std::vector<TimePointAndIntAndOptInt>
  read_carbon_deoxides(OrderBy order, SensorId sensor_id, size_t limit);
  std::vector<TimePointAndIntAndOptInt>
  read_total_vocs(OrderBy order, SensorId sensor_id, size_t limit);

private:
  void retry_failed() { _available = false; }
  //
  std::optional<RowId> insert_values(std::string_view query,
                                     TimePointAndDouble values_to_insert);
  //
  std::optional<RowId> insert_values(std::string_view query,
                                     TimePointAndIntAndOptInt values_to_insert);
  //
  size_t read_values(std::string_view query,
                     std::tuple<SensorId, OrderBy, size_t> placeholder,
                     ReadCallback<TimePointAndDouble> callback);
  //
  size_t read_values(std::string_view query,
                     std::tuple<SensorId, OrderBy, size_t> placeholder,
                     ReadCallback<TimePointAndIntAndOptInt> callback);
};
