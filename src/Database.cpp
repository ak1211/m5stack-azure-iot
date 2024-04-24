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
      " VALUES(?,?,?);" // values#1, values#2, values#3
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
    OrderBy order, SensorId sensor_id, size_t limit,
    Database::ReadCallback<TimePointAndDouble> callback) {
  constexpr static std::string_view query{
      "SELECT"
      " sensor_id,at,degc"
      " FROM temperature"
      " WHERE sensor_id=?" // placeholder#1
      " ORDER BY ?"        // placeholder#2
      " LIMIT ?;"          // placeholder#3
  };
  return read_values(query, std::make_tuple(sensor_id, order, limit), callback);
}

//
std::vector<Database::TimePointAndDouble>
Database::read_temperatures(OrderBy order, SensorId sensor_id, size_t limit) {
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
      " VALUES(?,?,?);" // values#1, values#2, values#3
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
    OrderBy order, SensorId sensor_id, size_t limit,
    Database::ReadCallback<TimePointAndDouble> callback) {
  constexpr static std::string_view query{
      "SELECT"
      " sensor_id,at,rh"
      " FROM relative_humidity"
      " WHERE sensor_id=?" // placeholder#1
      " ORDER BY ?"        // placeholder#2
      " LIMIT ?;"          // placeholder#3
  };
  return read_values(query, std::make_tuple(sensor_id, order, limit), callback);
}

//
std::vector<Database::TimePointAndDouble>
Database::read_relative_humidities(OrderBy order, SensorId sensor_id,
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
      " VALUES(?,?,?);" // values#1, values#2, values#3
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
Database::read_pressures(OrderBy order, SensorId sensor_id, size_t limit,
                         Database::ReadCallback<TimePointAndDouble> callback) {
  constexpr static std::string_view query{
      "SELECT"
      " sensor_id,at,hpa"
      " FROM pressure"
      " WHERE sensor_id=?" // placeholder#1
      " ORDER BY ?"        // placeholder#2
      " LIMIT ?;"          // placeholder#3
  };
  return read_values(query, std::make_tuple(sensor_id, order, limit), callback);
}

//
std::vector<Database::TimePointAndDouble>
Database::read_pressures(OrderBy order, SensorId sensor_id, size_t limit) {
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
      " VALUES(?,?,?,?);" // values#1, values#2, values#3, values#4
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
    OrderBy order, SensorId sensor_id, size_t limit,
    Database::ReadCallback<TimePointAndIntAndOptInt> callback) {
  constexpr static std::string_view query{
      "SELECT"
      " sensor_id,at,ppm,baseline"
      " FROM carbon_dioxide"
      " WHERE sensor_id=?" // placeholder#1
      " ORDER BY ?"        // placeholder#2
      " LIMIT ?;"          // placeholder#3
  };
  return read_values(query, std::make_tuple(sensor_id, order, limit), callback);
}

//
std::vector<Database::TimePointAndIntAndOptInt>
Database::read_carbon_deoxides(OrderBy order, SensorId sensor_id,
                               size_t limit) {
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
      " VALUES(?,?,?,?);" // values#1, values#2, values#3, values#4
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
    OrderBy order, SensorId sensor_id, size_t limit,
    Database::ReadCallback<TimePointAndIntAndOptInt> callback) {
  constexpr static std::string_view query{
      "SELECT"
      " sensor_id,at,ppb,baseline"
      " FROM total_voc"
      " WHERE sensor_id=?" // placeholder#1
      " ORDER BY ?"        // placeholder#2
      " LIMIT ?;"          // placeholder#3
  };
  return read_values(query, std::make_tuple(sensor_id, order, limit), callback);
}

//
std::vector<Database::TimePointAndIntAndOptInt>
Database::read_total_vocs(OrderBy order, SensorId sensor_id, size_t limit) {
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
    if (auto result = sqlite3_config(SQLITE_CONFIG_MALLOC, &psram_mem_methods);
        result != SQLITE_OK) {
      M5_LOGE("sqlite3_config() failure: %d", result);
      goto error_exit;
    }
    // データーベース用メモリは事前にヒープから確保しておく
    free(_heap_memory_for_database);
    _heap_memory_for_database = ps_malloc(SIZE_OF_HEAP_MEMORY);
    if (_heap_memory_for_database) {
      if (auto result =
              sqlite3_config(SQLITE_CONFIG_HEAP, _heap_memory_for_database,
                             SIZE_OF_HEAP_MEMORY, SIZE_OF_MINIMUM_HEAP_REQUEST);
          result != SQLITE_OK) {
        M5_LOGE("sqlite3_config() failure: %d", result);
        goto error_exit;
      }
    } else {
      M5_LOGE("sqlite3 memory allocation failure");
      goto error_exit;
    }
  }
  //
  if (auto result = sqlite3_initialize(); result != SQLITE_OK) {
    M5_LOGE("sqlite3_initialize() failure: %d", result);
    goto error_exit;
  }
  //
  constexpr static std::string_view SQLITE3_FILE_NAME{":memory:"};
  M5_LOGD("sqlite3 open file : %s", SQLITE3_FILE_NAME.data());
  if (auto result = sqlite3_open_v2(SQLITE3_FILE_NAME.data(), &sqlite3_db,
                                    SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE |
                                        SQLITE_OPEN_MEMORY,
                                    nullptr);
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
    M5_LOGV("exec query");
    M5_LOGV("%s", query.data());
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
bool Database::insert(const Sensor::MeasurementBme280 &m) {
  if (!available()) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }
  const auto &[timepoint, it] = m;
  const auto sensorid = SensorId{it.sensor_descriptor};

  if (!insert_temperature(sensorid, timepoint, it.temperature)) {
    M5_LOGE("insert_temperature() failure.");
    return false;
  }

  if (!insert_pressure(sensorid, timepoint, it.pressure)) {
    M5_LOGE("insert_pressure() failure.");
    return false;
  }

  if (!insert_relative_humidity(sensorid, timepoint, it.relative_humidity)) {
    M5_LOGE("insert_relative_humidity() failure.");
    return false;
  }

  M5_LOGD("insert Bme280 is success.");
  return true;
}

//
//
//
bool Database::insert(const Sensor::MeasurementSgp30 &m) {
  if (!available()) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }
  const auto &[timepoint, it] = m;
  const auto sensorid = SensorId{it.sensor_descriptor};

  {
    std::optional<uint16_t> tvoc_base =
        it.tvoc_baseline ? std::make_optional(it.tvoc_baseline->value)
                         : std::nullopt;
    if (!insert_total_voc(sensorid, timepoint, it.tvoc, tvoc_base)) {
      M5_LOGE("insert_total_voc() failure.");
      return false;
    }
  }
  {
    std::optional<uint16_t> eco2_base =
        it.eCo2_baseline ? std::make_optional(it.eCo2_baseline->value)
                         : std::nullopt;
    if (!insert_carbon_dioxide(sensorid, timepoint, it.eCo2, eco2_base)) {
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
bool Database::insert(const Sensor::MeasurementScd30 &m) {
  if (!available()) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }
  const auto &[timepoint, it] = m;
  const auto sensorid = SensorId{it.sensor_descriptor};

  if (!insert_temperature(sensorid, timepoint, it.temperature)) {
    M5_LOGE("insert_temperature() failure.");
    return false;
  }

  if (!insert_relative_humidity(sensorid, timepoint, it.relative_humidity)) {
    M5_LOGE("insert_relative_humidity() failure.");
    return false;
  }

  if (!insert_carbon_dioxide(sensorid, timepoint, it.co2, std::nullopt)) {
    M5_LOGE("insert_carbon_dioxide() failure.");
    return false;
  }

  M5_LOGI("insert Scd30 is success.");
  return true;
}

//
//
//
bool Database::insert(const Sensor::MeasurementScd41 &m) {
  if (!available()) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }
  const auto &[timepoint, it] = m;
  const auto sensorid = SensorId{it.sensor_descriptor};

  if (!insert_temperature(sensorid, timepoint, it.temperature)) {
    M5_LOGE("insert_temperature() failure.");
    return false;
  }

  if (!insert_relative_humidity(sensorid, timepoint, it.relative_humidity)) {
    M5_LOGE("insert_relative_humidity() failure.");
    return false;
  }

  if (!insert_carbon_dioxide(sensorid, timepoint, it.co2, std::nullopt)) {
    M5_LOGE("insert_carbon_dioxide() failure.");
    return false;
  }

  M5_LOGI("insert Scd41 is success.");
  return true;
}

//
//
//
bool Database::insert(const Sensor::MeasurementM5Env3 &m) {
  if (!available()) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }
  const auto &[timepoint, it] = m;
  const auto sensorid = SensorId{it.sensor_descriptor};

  if (!insert_temperature(sensorid, timepoint, it.temperature)) {
    M5_LOGE("insert_temperature() failure.");
    return false;
  }

  if (!insert_pressure(sensorid, timepoint, it.pressure)) {
    M5_LOGE("insert_pressure() failure.");
    return false;
  }

  if (!insert_relative_humidity(sensorid, timepoint, it.relative_humidity)) {
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
    M5_LOGE("query");
    M5_LOGE("%s", query.data());
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
  if (auto p = sqlite3_expanded_sql(stmt); p) {
    M5_LOGV("%s", p);
    sqlite3_free(p);
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
    M5_LOGE("query");
    M5_LOGE("%s", query.data());
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
  if (auto p = sqlite3_expanded_sql(stmt); p) {
    M5_LOGV("%s", p);
    sqlite3_free(p);
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
size_t Database::read_values(std::string_view query,
                             std::tuple<SensorId, OrderBy, size_t> placeholder,
                             ReadCallback<TimePointAndDouble> callback) {
  sqlite3_stmt *stmt{nullptr};
  auto [sensorid, orderby, limits] = placeholder;

  if (!sqlite3_db) {
    M5_LOGE("sqlite3_db is null");
    goto error_exit;
  }

  if (auto result =
          sqlite3_prepare_v2(sqlite3_db, query.data(), -1, &stmt, nullptr);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    M5_LOGE("query");
    M5_LOGE("%s", query.data());
    goto error_exit;
  }
  //
  if (auto result = sqlite3_bind_int64(stmt, 1, sensorid);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  {
    std::string_view order_text{orderby == OrderByAtDesc ? "at DESC"
                                                         : "at ASC"};
    if (auto result =
            sqlite3_bind_text(stmt, 2, order_text.data(), -1, SQLITE_STATIC);
        result != SQLITE_OK) {
      M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
      goto error_exit;
    }
  }
  if (auto result = sqlite3_bind_int(stmt, 3, limits); result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  if (auto p = sqlite3_expanded_sql(stmt); p) {
    M5_LOGV("%s", p);
    sqlite3_free(p);
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
size_t Database::read_values(std::string_view query,
                             std::tuple<SensorId, OrderBy, size_t> placeholder,
                             ReadCallback<TimePointAndIntAndOptInt> callback) {
  sqlite3_stmt *stmt{nullptr};
  auto [sensorid, orderby, limits] = placeholder;

  if (!sqlite3_db) {
    M5_LOGE("sqlite3 sqlite3_db is null");
    goto error_exit;
  }

  if (auto result =
          sqlite3_prepare_v2(sqlite3_db, query.data(), -1, &stmt, nullptr);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    M5_LOGE("query");
    M5_LOGE("%s", query.data());
    goto error_exit;
  }
  //
  if (auto result = sqlite3_bind_int64(stmt, 1, sensorid);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  {
    std::string_view order_text{orderby == OrderByAtDesc ? "at DESC"
                                                         : "at ASC"};
    if (auto result =
            sqlite3_bind_text(stmt, 2, order_text.data(), -1, SQLITE_STATIC);
        result != SQLITE_OK) {
      M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
      goto error_exit;
    }
  }
  if (auto result = sqlite3_bind_int(stmt, 3, limits); result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    goto error_exit;
  }
  if (auto p = sqlite3_expanded_sql(stmt); p) {
    M5_LOGV("%s", p);
    sqlite3_free(p);
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
