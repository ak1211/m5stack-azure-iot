// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "Database.hpp"
#include "Application.hpp"
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include <M5Unified.h>

using namespace std::chrono;

//
//
//
constexpr static std::string_view schema_temperature{
    "CREATE TABLE IF NOT EXISTS temperature"
    "(id INTEGER PRIMARY KEY AUTOINCREMENT"
    ",sensor_id INTEGER NOT NULL"
    ",at INTEGER NOT NULL"
    ",degc REAL NOT NULL"
    ");"};

//
std::optional<Database::RowId>
Database::insert_temperature(SensorId sensor_id, system_clock::time_point at,
                             DegC degc) {
  constexpr static std::string_view query{
      "INSERT INTO"
      " temperature(sensor_id,at,degc)"
      " VALUES(?,?,?);" // values#0, values#1, values#2
  };
  TimePointAndDouble values{sensor_id, at, degc.count()};
  if (auto rowid = insert_values(query, values); rowid) {
    return (rowid_temperature = rowid);
  } else {
    return std::nullopt;
  }
}

//
size_t Database::read_temperatures(
    Order order, SensorId sensor_id, size_t limit,
    Database::ReadCallback<TimePointAndDouble> callback) {
  constexpr static std::string_view query_part1{
      "SELECT"
      " sensor_id,at,degc"
      " FROM temperature"
      " WHERE sensor_id=?" // placeholder#1
  };
  constexpr static std::string_view query_part2[2]{" ORDER BY at ASC",
                                                   " ORDER BY at DESC"};
  constexpr static std::string_view query_part3{
      " LIMIT ?;" // placeholder#2
  };
  static_assert(OrderAsc == 0);
  static_assert(OrderDesc == 1);
  std::string query{};
  query += query_part1;
  query += query_part2[order];
  query += query_part3;
  return read_values(query, std::make_tuple(std::nullopt, sensor_id, limit),
                     callback);
}

//
std::vector<Database::TimePointAndDouble>
Database::read_temperatures(Order order, SensorId sensor_id, size_t limit) {
  std::vector<TimePointAndDouble> vect{};
  vect.reserve(limit);
  read_temperatures(order, sensor_id, limit,
                    [&vect](size_t counter, TimePointAndDouble item) -> bool {
                      vect.push_back(item);
                      return true;
                    });
  vect.shrink_to_fit();
  return std::move(vect);
}

//
//
//
constexpr static std::string_view schema_relative_humidity{
    "CREATE TABLE IF NOT EXISTS relative_humidity"
    "(id INTEGER PRIMARY KEY AUTOINCREMENT"
    ",sensor_id INTEGER NOT NULL"
    ",at INTEGER NOT NULL"
    ",rh REAL NOT NULL"
    ");"};

//
std::optional<Database::RowId>
Database::insert_relative_humidity(SensorId sensor_id,
                                   system_clock::time_point at, PctRH rh) {
  constexpr static std::string_view query{
      "INSERT INTO"
      " relative_humidity(sensor_id,at,rh)"
      " VALUES(?,?,?);" // values#0, values#1, values#2
  };
  TimePointAndDouble values{sensor_id, at, rh.count()};
  if (auto rowid = insert_values(query, values); rowid) {
    return (rowid_relative_humidity = rowid);
  } else {
    return std::nullopt;
  }
}

//
size_t Database::read_relative_humidities(
    Order order, SensorId sensor_id, size_t limit,
    Database::ReadCallback<TimePointAndDouble> callback) {
  constexpr static std::string_view query_part1{
      "SELECT"
      " sensor_id,at,rh"
      " FROM relative_humidity"
      " WHERE sensor_id=?" // placeholder#1
  };
  constexpr static std::string_view query_part2[2]{" ORDER BY at ASC",
                                                   " ORDER BY at DESC"};
  constexpr static std::string_view query_part3{
      " LIMIT ?;" // placeholder#2
  };
  static_assert(OrderAsc == 0);
  static_assert(OrderDesc == 1);
  std::string query{};
  query += query_part1;
  query += query_part2[order];
  query += query_part3;
  return read_values(query, std::make_tuple(std::nullopt, sensor_id, limit),
                     callback);
}

//
std::vector<Database::TimePointAndDouble>
Database::read_relative_humidities(Order order, SensorId sensor_id,
                                   size_t limit) {
  std::vector<TimePointAndDouble> vect{};
  vect.reserve(limit);
  read_relative_humidities(
      order, sensor_id, limit,
      [&vect](size_t counter, TimePointAndDouble item) -> bool {
        vect.push_back(item);
        return true;
      });
  vect.shrink_to_fit();
  return std::move(vect);
}

//
//
//
constexpr static std::string_view schema_pressure{
    "CREATE TABLE IF NOT EXISTS pressure"
    "(id INTEGER PRIMARY KEY AUTOINCREMENT"
    ",sensor_id INTEGER NOT NULL"
    ",at INTEGER NOT NULL"
    ",hpa REAL NOT NULL"
    ");"};

//
std::optional<Database::RowId>
Database::insert_pressure(SensorId sensor_id, system_clock::time_point at,
                          HectoPa hpa) {
  static const std::string_view query{
      "INSERT INTO"
      " pressure(sensor_id,at,hpa)"
      " VALUES(?,?,?);" // values#0, values#1, values#2
  };
  TimePointAndDouble values{sensor_id, at, hpa.count()};
  if (auto rowid = insert_values(query, values); rowid) {
    return (rowid_pressure = rowid);
  } else {
    return std::nullopt;
  }
}

//
size_t
Database::read_pressures(Order order, SensorId sensor_id, size_t limit,
                         Database::ReadCallback<TimePointAndDouble> callback) {
  constexpr static std::string_view query_part1{
      "SELECT"
      " sensor_id,at,hpa"
      " FROM pressure"
      " WHERE sensor_id=?" // placeholder#1
  };
  constexpr static std::string_view query_part2[2]{" ORDER BY at ASC",
                                                   " ORDER BY at DESC"};
  constexpr static std::string_view query_part3{
      " LIMIT ?;" // placeholder#2
  };
  static_assert(OrderAsc == 0);
  static_assert(OrderDesc == 1);
  std::string query{};
  query += query_part1;
  query += query_part2[order];
  query += query_part3;
  return read_values(query, std::make_tuple(std::nullopt, sensor_id, limit),
                     callback);
}

//
std::vector<Database::TimePointAndDouble>
Database::read_pressures(Order order, SensorId sensor_id, size_t limit) {
  std::vector<TimePointAndDouble> vect{};
  vect.reserve(limit);
  read_pressures(order, sensor_id, limit,
                 [&vect](size_t counter, TimePointAndDouble item) -> bool {
                   vect.push_back(item);
                   return true;
                 });
  vect.shrink_to_fit();
  return std::move(vect);
}

//
//
//
constexpr static std::string_view schema_carbon_dioxide{
    "CREATE TABLE IF NOT EXISTS carbon_dioxide"
    "(id INTEGER PRIMARY KEY AUTOINCREMENT"
    ",sensor_id INTEGER NOT NULL"
    ",at INTEGER NOT NULL"
    ",ppm REAL NOT NULL"
    ",baseline INTEGER"
    ");"};

//
std::optional<Database::RowId>
Database::insert_carbon_dioxide(SensorId sensor_id, system_clock::time_point at,
                                Ppm ppm, std::optional<uint16_t> baseline) {
  constexpr static std::string_view query{
      "INSERT INTO"
      " carbon_dioxide(sensor_id,at,ppm,baseline)"
      " VALUES(?,?,?,?);" // values#0, values#1, values#2, values#3
  };
  TimePointAndIntAndOptInt values{sensor_id, at, ppm.value, baseline};
  if (auto rowid = insert_values(query, values); rowid) {
    return (rowid_carbon_dioxide = rowid);
  } else {
    return std::nullopt;
  }
}

//
size_t Database::read_carbon_deoxides(
    Order order, SensorId sensor_id, size_t limit,
    Database::ReadCallback<TimePointAndIntAndOptInt> callback) {
  constexpr static std::string_view query_part1{
      "SELECT"
      " sensor_id,at,ppm,baseline"
      " FROM carbon_dioxide"
      " WHERE sensor_id=?" // placeholder#1
  };
  constexpr static std::string_view query_part2[2]{" ORDER BY at ASC",
                                                   " ORDER BY at DESC"};
  constexpr static std::string_view query_part3{
      " LIMIT ?;" // placeholder#2
  };
  static_assert(OrderAsc == 0);
  static_assert(OrderDesc == 1);
  std::string query{};
  query += query_part1;
  query += query_part2[order];
  query += query_part3;
  return read_values(query, std::make_tuple(std::nullopt, sensor_id, limit),
                     callback);
}

//
std::vector<Database::TimePointAndIntAndOptInt>
Database::read_carbon_deoxides(Order order, SensorId sensor_id, size_t limit) {
  std::vector<TimePointAndIntAndOptInt> vect{};
  vect.reserve(limit);
  read_carbon_deoxides(
      order, sensor_id, limit,
      [&vect](size_t counter, TimePointAndIntAndOptInt item) -> bool {
        vect.push_back(item);
        return true;
      });
  vect.shrink_to_fit();
  return std::move(vect);
}

//
//
//
constexpr static std::string_view schema_total_voc{
    "CREATE TABLE IF NOT EXISTS total_voc"
    "(id INTEGER PRIMARY KEY AUTOINCREMENT"
    ",sensor_id INTEGER NOT NULL"
    ",at INTEGER NOT NULL"
    ",ppb REAL NOT NULL"
    ",baseline INTEGER"
    ");"};

//
std::optional<Database::RowId>
Database::insert_total_voc(SensorId sensor_id, system_clock::time_point at,
                           Ppb ppb, std::optional<uint16_t> baseline) {
  constexpr static std::string_view query{
      "INSERT INTO"
      " total_voc(sensor_id,at,ppb,baseline)"
      " VALUES(?,?,?,?);" // values#0, values#1, values#2, values#3
  };
  TimePointAndIntAndOptInt values{sensor_id, at, ppb.value, baseline};
  if (auto rowid = insert_values(query, values); rowid) {
    return (rowid_total_voc = rowid);
  } else {
    return std::nullopt;
  }
}

//
size_t Database::read_total_vocs(
    Order order, SensorId sensor_id, size_t limit,
    Database::ReadCallback<TimePointAndIntAndOptInt> callback) {
  constexpr static std::string_view query_part1{
      "SELECT"
      " sensor_id,at,ppb,baseline"
      " FROM total_voc"
      " WHERE sensor_id=?" // placeholder#1
  };
  constexpr static std::string_view query_part2[2]{" ORDER BY at ASC",
                                                   " ORDER BY at DESC"};
  constexpr static std::string_view query_part3{
      " LIMIT ?;" // placeholder#2
  };
  static_assert(OrderAsc == 0);
  static_assert(OrderDesc == 1);
  std::string query{};
  query += query_part1;
  query += query_part2[order];
  query += query_part3;
  return read_values(query, std::make_tuple(std::nullopt, sensor_id, limit),
                     callback);
}

//
std::vector<Database::TimePointAndIntAndOptInt>
Database::read_total_vocs(Order order, SensorId sensor_id, size_t limit) {
  std::vector<TimePointAndIntAndOptInt> vect{};
  vect.reserve(limit);
  read_total_vocs(
      order, sensor_id, limit,
      [&vect](size_t counter, TimePointAndIntAndOptInt item) -> bool {
        vect.push_back(item);
        return true;
      });
  vect.shrink_to_fit();
  return std::move(vect);
}

//
std::optional<std::pair<std::time_t, BaselineECo2>>
Database::get_latest_baseline_eco2(SensorId sensor_id) {
  constexpr static std::string_view query{"SELECT"
                                          " sensor_id" // 0
                                          ",at"        // 1
                                          ",baseline"  // 2
                                          " FROM carbon_dioxide"
                                          " WHERE sensor_id=?"
                                          " AND baseline NOTNULL"
                                          " ORDER BY at DESC"
                                          " LIMIT 1;"};
  if (auto baseline = raw_get_latest_baseline(query, sensor_id);
      baseline.has_value()) {
    auto [time, value] = *baseline;
    return std::make_pair(time, BaselineECo2(value));
  } else {
    return std::nullopt;
  }
}

//
std::optional<std::pair<std::time_t, BaselineTotalVoc>>
Database::get_latest_baseline_total_voc(SensorId sensor_id) {
  constexpr static std::string_view query{"SELECT"
                                          " sensor_id" // 0
                                          ",at"        // 1
                                          ",baseline"  // 2
                                          " FROM total_voc"
                                          " WHERE sensor_id=?"
                                          " AND baseline NOTNULL"
                                          " ORDER BY at DESC"
                                          " LIMIT 1;"};
  if (auto baseline = raw_get_latest_baseline(query, sensor_id);
      baseline.has_value()) {
    auto [time, value] = *baseline;
    return std::make_pair(time, BaselineTotalVoc(value));
  } else {
    return std::nullopt;
  }
}

//
//
//
bool Database::begin() noexcept {
  if (sqlite3_db) {
    terminate();
  }
  if (psramFound()) {
    M5_LOGI("Database is used on PSRAM");
    // PSRAMメモリアロケータ
    sqlite3_mem_methods psram_mem_methods{
        .xMalloc = [](int size) -> void * { return ps_malloc(size); },
        .xFree = free,
        .xRealloc = [](void *p, int size) -> void * {
          return ps_realloc(p, size);
        },
        .xSize = [](void *ptr) -> int {
          return heap_caps_get_allocated_size(ptr);
        },
        .xRoundup = [](int n) -> int {
          return (n + 7) & ~7; // 8の倍数に切り上げる
        },
        .xInit = [](void *appData) -> int {
          // return psramInit() == true ? 0 : -1;
          return 0; // always OK
        },
        .xShutdown = [](void *appData) {},
        .pAppData = nullptr,
    };
    sqlite3_config(SQLITE_CONFIG_MALLOC, &psram_mem_methods);
  }
  //
  if (auto result = sqlite3_initialize(); result != SQLITE_OK) {
    M5_LOGE("sqlite3_initialize() failure: %d", result);
    goto error_exit;
  }
  //
  M5_LOGV("sqlite3 open file : %s", sqlite3_filename.data());
  if (auto result =
          sqlite3_open_v2(sqlite3_filename.data(), &sqlite3_db,
                          SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, nullptr);
      result != SQLITE_OK) {
    M5_LOGE("sqlite3_open_v2() failure: %d", result);
    goto error_exit;
  }

  // create tables
  // temperature
  // relative humidity
  // pressure
  // carbon dioxide
  // total voc
  for (auto &query :
       {schema_temperature, schema_relative_humidity, schema_pressure,
        schema_carbon_dioxide, schema_total_voc}) {
    char *error_msg{nullptr};
    if (auto result = sqlite3_exec(sqlite3_db, query.data(), nullptr, nullptr,
                                   &error_msg);
        result != SQLITE_OK) {
      M5_LOGE("create table error");
      if (error_msg) {
        M5_LOGE("%s", error_msg);
      }
      sqlite3_free(error_msg);
      goto error_exit;
    }
  }

  // succsessfully exit
  _available = true;
  return _available;

error_exit:
  _available = false;
  return _available;
}

//
//
//
void Database::terminate() noexcept {
  if (sqlite3_db) {
    if (auto result = sqlite3_close(sqlite3_db); result != SQLITE_OK) {
      M5_LOGE("sqlite3_close() failure: %d", result);
    }
  }
  sqlite3_db = nullptr;
  _available = false;
}

//
//
//
bool Database::insert(const MeasurementBme280 &in) {
  if (!available()) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }

  if (auto rawid = insert_temperature(SensorId(in.second.sensor_descriptor),
                                      in.first, in.second.temperature);
      rawid < 0) {
    M5_LOGE("insert_temperature() failure.");
    return false;
  }

  if (auto rawid = insert_pressure(SensorId(in.second.sensor_descriptor),
                                   in.first, in.second.pressure);
      rawid < 0) {
    M5_LOGE("insert_pressure() failure.");
    return false;
  }

  if (auto rawid =
          insert_relative_humidity(SensorId(in.second.sensor_descriptor),
                                   in.first, in.second.relative_humidity);
      rawid < 0) {
    M5_LOGE("insert_relative_humidity() failure.");
    return false;
  }

  M5_LOGD("insert Bme280 is success.");
  return true;
}

//
//
//
bool Database::insert(const MeasurementSgp30 &in) {
  if (!available()) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }
  if (in.second.tvoc_baseline.has_value()) {
    BaselineTotalVoc tvoc_base = in.second.tvoc_baseline.value();
    if (auto rawid =
            insert_total_voc(SensorId(in.second.sensor_descriptor), in.first,
                             in.second.tvoc, tvoc_base.value);
        rawid < 0) {
      M5_LOGE("insert_total_voc() failure.");
      return false;
    }
  } else {
    if (auto rawid = insert_total_voc(SensorId(in.second.sensor_descriptor),
                                      in.first, in.second.tvoc, std::nullopt);
        rawid < 0) {
      M5_LOGE("insert_total_voc() failure.");
      return false;
    }
  }

  if (in.second.eCo2_baseline.has_value()) {
    BaselineECo2 eco2_base = in.second.eCo2_baseline.value();
    if (auto rawid =
            insert_carbon_dioxide(SensorId(in.second.sensor_descriptor),
                                  in.first, in.second.eCo2, eco2_base.value);
        rawid < 0) {
      M5_LOGE("insert_carbon_dioxide() failure.");
      return false;
    }
  } else {
    if (auto rawid =
            insert_carbon_dioxide(SensorId(in.second.sensor_descriptor),
                                  in.first, in.second.eCo2, std::nullopt);
        rawid < 0) {
      M5_LOGE("insert_carbon_dioxide() failure.");
      return false;
    }
  }

  M5_LOGI("insert Sgp30 is success.");
  return true;
}

//
//
//
bool Database::insert(const MeasurementScd30 &in) {
  if (!available()) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }

  if (auto rawid = insert_temperature(SensorId(in.second.sensor_descriptor),
                                      in.first, in.second.temperature);
      rawid < 0) {
    M5_LOGE("insert_temperature() failure.");
    return false;
  }

  if (auto rawid =
          insert_relative_humidity(SensorId(in.second.sensor_descriptor),
                                   in.first, in.second.relative_humidity);
      rawid < 0) {
    M5_LOGE("insert_relative_humidity() failure.");
    return false;
  }

  if (auto rawid = insert_carbon_dioxide(SensorId(in.second.sensor_descriptor),
                                         in.first, in.second.co2, std::nullopt);
      rawid < 0) {
    M5_LOGE("insert_carbon_dioxide() failure.");
    return false;
  }

  M5_LOGI("insert Scd30 is success.");
  return true;
}

//
//
//
bool Database::insert(const MeasurementScd41 &in) {
  if (!available()) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }

  if (auto rawid = insert_temperature(SensorId(in.second.sensor_descriptor),
                                      in.first, in.second.temperature);
      rawid < 0) {
    M5_LOGE("insert_temperature() failure.");
    return false;
  }

  if (auto rawid =
          insert_relative_humidity(SensorId(in.second.sensor_descriptor),
                                   in.first, in.second.relative_humidity);
      rawid < 0) {
    M5_LOGE("insert_relative_humidity() failure.");
    return false;
  }

  if (auto rawid = insert_carbon_dioxide(SensorId(in.second.sensor_descriptor),
                                         in.first, in.second.co2, std::nullopt);
      rawid < 0) {
    M5_LOGE("insert_carbon_dioxide() failure.");
    return false;
  }

  M5_LOGI("insert Scd41 is success.");
  return true;
}

//
//
//
bool Database::insert(const MeasurementM5Env3 &in) {
  if (!available()) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }

  if (auto rawid = insert_temperature(SensorId(in.second.sensor_descriptor),
                                      in.first, in.second.temperature);
      rawid < 0) {
    M5_LOGE("insert_temperature() failure.");
    return false;
  }

  if (auto rawid = insert_pressure(SensorId(in.second.sensor_descriptor),
                                   in.first, in.second.pressure);
      rawid < 0) {
    M5_LOGE("insert_pressure() failure.");
    return false;
  }

  if (auto rawid =
          insert_relative_humidity(SensorId(in.second.sensor_descriptor),
                                   in.first, in.second.relative_humidity);
      rawid < 0) {
    M5_LOGE("insert_relative_humidity() failure.");
    return false;
  }

  M5_LOGD("insert M5Env3 is success.");
  return true;
}

//
std::optional<Database::RowId>
Database::insert_values(std::string_view query,
                        TimePointAndDouble values_to_insert) {
  auto [sensor_id_to_insert, tp_to_insert, fp_value_to_insert] =
      values_to_insert;
  sqlite3_stmt *stmt{nullptr};

  if (!sqlite3_db) {
    M5_LOGE("sqlite3_db is null");
    goto error_exit;
  }

  if (auto result =
          sqlite3_prepare_v2(sqlite3_db, query.data(), -1, &stmt, nullptr);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  //
  if (auto result = sqlite3_bind_int64(stmt, 1, sensor_id_to_insert);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  if (auto result = sqlite3_bind_int64(
          stmt, 2, static_cast<int64_t>(system_clock::to_time_t(tp_to_insert)));
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  if (auto result = sqlite3_bind_double(stmt, 3, fp_value_to_insert);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  //
  for (int32_t retry = 0; sqlite3_step(stmt) != SQLITE_DONE; ++retry) {
    M5_LOGV("sqlite3_step()");
    if (retry >= RETRY_COUNT) {
      M5_LOGE("sqlite3_step() over");
      M5_LOGE("query is \"%s\"", query.data());
      M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
      retry_failed();
      goto error_exit;
    }
  }

  if (auto result = sqlite3_finalize(stmt); result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    stmt = nullptr; // omit double finalize
    goto error_exit;
  }

  return sqlite3_last_insert_rowid(sqlite3_db);

error_exit:
  sqlite3_finalize(stmt);
  return -1;
}

//
std::optional<Database::RowId>
Database::insert_values(std::string_view query,
                        TimePointAndIntAndOptInt values_to_insert) {
  auto [sensor_id_to_insert, tp_to_insert, u16_value_to_insert,
        optional_u16_value_to_insert] = values_to_insert;
  sqlite3_stmt *stmt{nullptr};

  if (!sqlite3_db) {
    M5_LOGE("sqlite3_db is null");
    goto error_exit;
  }

  if (auto result =
          sqlite3_prepare_v2(sqlite3_db, query.data(), -1, &stmt, nullptr);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  //
  if (auto result = sqlite3_bind_int64(stmt, 1, sensor_id_to_insert);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  if (auto result = sqlite3_bind_int64(
          stmt, 2, static_cast<int64_t>(system_clock::to_time_t(tp_to_insert)));
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  if (auto result = sqlite3_bind_int(stmt, 3, u16_value_to_insert);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  if (optional_u16_value_to_insert.has_value()) {
    if (auto result = sqlite3_bind_int(stmt, 4, *optional_u16_value_to_insert);
        result != SQLITE_OK) {
      M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
      goto error_exit;
    }
  } else {
    if (auto result = sqlite3_bind_null(stmt, 4); result != SQLITE_OK) {
      M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
      goto error_exit;
    }
  }
  //
  for (int32_t retry = 0; sqlite3_step(stmt) != SQLITE_DONE; ++retry) {
    M5_LOGV("sqlite3_step()");
    if (retry >= RETRY_COUNT) {
      M5_LOGE("sqlite3_step() over");
      M5_LOGE("query is \"%s\"", query.data());
      M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
      retry_failed();
      goto error_exit;
    }
  }

  if (auto result = sqlite3_finalize(stmt); result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    stmt = nullptr; // omit double finalize
    goto error_exit;
  }

  return std::make_optional(sqlite3_last_insert_rowid(sqlite3_db));

error_exit:
  sqlite3_finalize(stmt);
  return std::nullopt;
}

//
size_t
Database::read_values(std::string_view query,
                      std::tuple<std::nullopt_t, SensorId, size_t> placeholder,
                      ReadCallback<TimePointAndDouble> callback) {
  sqlite3_stmt *stmt{nullptr};

  if (!sqlite3_db) {
    M5_LOGE("sqlite3_db is null");
    goto error_exit;
  }

  if (auto result =
          sqlite3_prepare_v2(sqlite3_db, query.data(), -1, &stmt, nullptr);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  //
  if (auto result = sqlite3_bind_int64(stmt, 1, std::get<1>(placeholder));
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  if (auto result = sqlite3_bind_int(stmt, 2, std::get<2>(placeholder));
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  //
  {
    size_t counter{0};
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      M5_LOGV("sqlite3_step()");
      // number 0 is sensor_id
      int64_t sensor_id = sqlite3_column_int64(stmt, 0);
      int64_t at = sqlite3_column_int64(stmt, 1);
      double fp_value = sqlite3_column_double(stmt, 2);
      TimePointAndDouble values{sensor_id, system_clock::from_time_t(at),
                                fp_value};
      if (bool _continue = callback(counter, values); _continue == false) {
        break;
      }
      counter++;
    }

    if (auto result = sqlite3_finalize(stmt); result != SQLITE_OK) {
      M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
      stmt = nullptr; // omit double finalize
      goto error_exit;
    }

    return counter;
  }

error_exit:
  sqlite3_finalize(stmt);
  return 0;
}

//
size_t
Database::read_values(std::string_view query,
                      std::tuple<std::nullopt_t, SensorId, size_t> placeholder,
                      ReadCallback<TimePointAndIntAndOptInt> callback) {
  sqlite3_stmt *stmt{nullptr};

  if (!sqlite3_db) {
    M5_LOGE("sqlite3 sqlite3_db is null");
    goto error_exit;
  }

  if (auto result =
          sqlite3_prepare_v2(sqlite3_db, query.data(), -1, &stmt, nullptr);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  //
  if (auto result = sqlite3_bind_int64(stmt, 1, std::get<1>(placeholder));
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  if (auto result = sqlite3_bind_int(stmt, 2, std::get<2>(placeholder));
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  //
  {
    size_t counter = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      M5_LOGV("sqlite3_step()");
      // number 0 is sensor_id
      int64_t sensor_id = sqlite3_column_int64(stmt, 0);
      int64_t at = sqlite3_column_int64(stmt, 1);
      int int_value = sqlite3_column_int(stmt, 2);
      std::optional<uint16_t> optional_int_value{
          sqlite3_column_type(stmt, 3) == SQLITE_INTEGER
              ? std::make_optional(sqlite3_column_int(stmt, 3))
              : std::nullopt};
      TimePointAndIntAndOptInt values{sensor_id, system_clock::from_time_t(at),
                                      int_value, optional_int_value};
      if (bool _continue = callback(counter, values); _continue == false) {
        break;
      }
      counter++;
    }

    if (auto result = sqlite3_finalize(stmt); result != SQLITE_OK) {
      M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
      stmt = nullptr; // omit double finalize
      goto error_exit;
    }

    return counter;
  }

error_exit:
  sqlite3_finalize(stmt);
  return 0;
}

//
//
//
std::optional<std::pair<std::time_t, BaselineSGP30T>>
Database::raw_get_latest_baseline(std::string_view query, SensorId sensor_id) {
  sqlite3_stmt *stmt{nullptr};

  if (!sqlite3_db) {
    M5_LOGE("sqlite3 sqlite3_db is null");
    goto error_exit;
  }

  if (auto result =
          sqlite3_prepare_v2(sqlite3_db, query.data(), -1, &stmt, nullptr);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  //
  if (auto result = sqlite3_bind_int64(stmt, 1, sensor_id);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  //
  {
    std::optional<std::pair<std::time_t, BaselineSGP30T>> retval{std::nullopt};
    size_t counter = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      M5_LOGV("sqlite3_step()");
      // number 0 is sensor_id
      int64_t at = sqlite3_column_int64(stmt, 1);
      if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
        uint16_t baseline = sqlite3_column_int(stmt, 2);
        retval = std::make_pair(static_cast<std::time_t>(at),
                                static_cast<BaselineSGP30T>(baseline));
      }
      counter++;
    }
    if (auto result = sqlite3_finalize(stmt); result != SQLITE_OK) {
      M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
      stmt = nullptr; // omit double finalize
      goto error_exit;
    }
    return retval;
  }

error_exit:
  sqlite3_finalize(stmt);
  return std::nullopt;
}
