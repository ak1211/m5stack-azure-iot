// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "Application.hpp"
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
  std::string_view sqlite3_filename{};
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

  using Order = enum { OrderAsc = 0, OrderDesc = 1 };

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
  Database(std::string_view filename) noexcept : sqlite3_filename{filename} {}
  ~Database() noexcept { terminate(); }
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
  size_t read_temperatures(Order order, SensorId sensor_id, size_t limit,
                           ReadCallback<TimePointAndDouble> callback);
  size_t read_relative_humidities(Order order, SensorId sensor_id, size_t limit,
                                  ReadCallback<TimePointAndDouble> callback);
  size_t read_pressures(Order order, SensorId sensor_id, size_t limit,
                        ReadCallback<TimePointAndDouble> callback);
  size_t read_carbon_deoxides(Order order, SensorId sensor_id, size_t limit,
                              ReadCallback<TimePointAndIntAndOptInt> callback);
  size_t read_total_vocs(Order order, SensorId sensor_id, size_t limit,
                         ReadCallback<TimePointAndIntAndOptInt> callback);
  //
  std::vector<TimePointAndDouble> read_temperatures(Order order, SensorId sensor_id, size_t limit);
  std::vector<TimePointAndDouble>
  read_relative_humidities(Order order, SensorId sensor_id, size_t limit);
  std::vector<TimePointAndDouble>
  read_pressures(Order order, SensorId sensor_id, size_t limit);
  std::vector<TimePointAndIntAndOptInt>
  read_carbon_deoxides(Order order, SensorId sensor_id, size_t limit);
  std::vector<TimePointAndIntAndOptInt>
  read_total_vocs(Order order, SensorId sensor_id, size_t limit);
  //
  std::optional<std::pair<std::time_t, BaselineECo2>>
  get_latest_baseline_eco2(SensorId sensor_id);
  //
  std::optional<std::pair<std::time_t, BaselineTotalVoc>>
  get_latest_baseline_total_voc(SensorId sensor_id);

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
                     std::tuple<std::nullopt_t, SensorId, size_t> placeholder,
                     ReadCallback<TimePointAndDouble> callback);
  //
  size_t read_values(std::string_view query,
                     std::tuple<std::nullopt_t, SensorId, size_t> placeholder,
                     ReadCallback<TimePointAndIntAndOptInt> callback);
  //
  std::optional<std::pair<std::time_t, BaselineSGP30T>>
  raw_get_latest_baseline(std::string_view query, uint64_t sensor_id);
};
