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

const sqlite3_mem_methods Database::_custom_mem_methods{
    /* Memory allocation function */
    .xMalloc = [](int size) -> void * {
      return heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    },
    /* Free a prior allocation */
    .xFree = free,
    /* Resize an allocation */
    .xRealloc = [](void *ptr, int size) -> void * {
      return heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM);
    },
    /* Return the size of an allocation */
    .xSize = [](void *ptr) -> int { return heap_caps_get_allocated_size(ptr); },
    /* Round up request size to allocation size */
    .xRoundup = [](int size) -> int { return (size + 7) & ~7; },
    /* Initialize the memory allocator */
    .xInit = [](void *app_data) -> int {
      return 0; // nothing to do
    },
    /* Deinitialize the memory allocator */
    .xShutdown = [](void *app_data) -> void {
      // nothing to do
    },
    /* Argument to xInit() and xShutdown() */
    .pAppData = nullptr,
};

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
bool Database::insert_temperature(SensorId sensor_id,
                                  system_clock::time_point at, DegC degc) {
  constexpr static std::string_view query{
      "INSERT INTO"
      " temperature(sensor_id,at,degc)"
      " VALUES(?,?,?);" // values#1, values#2, values#3
  };
  TimePointAndDouble values{sensor_id, at, degc.count()};
  return insert_values(query, values);
}

//
size_t Database::read_temperatures(OrderBy order,
                                   system_clock::time_point at_begin,
                                   ReadCallback<TimePointAndDouble> callback) {
  constexpr static std::string_view query{
      "SELECT"
      " sensor_id,at,degc"
      " FROM temperature"
      " WHERE at >= :at_begin" // placeholder
      " ORDER BY :order_by;"   // placeholder
  };
  if (auto count =
          read_values(query, std::make_tuple(at_begin, order), callback);
      count) {
    return *count;
  } else {
    return 0;
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
      " WHERE sensor_id=:sensor_id" // placeholder
      " ORDER BY :order_by"         // placeholder
      " LIMIT :limit;"              // placeholder
  };
  if (auto count = read_values(query, std::make_tuple(sensor_id, order, limit),
                               callback);
      count) {
    return *count;
  } else {
    return 0;
  }
}

//
std::vector<Database::TimePointAndDouble>
Database::read_temperatures(OrderBy order, SensorId sensor_id, size_t limit) {
  std::vector<TimePointAndDouble> vect{};
  vect.reserve(limit);
  read_temperatures(order, sensor_id, limit,
                    [&vect](size_t counter, TimePointAndDouble item) -> bool {
                      // データー表示(デバッグ用)
                      if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_VERBOSE) {
                        auto &[sensorid, tp, fp_value] = item;
                        //
                        std::time_t time = system_clock::to_time_t(tp);
                        std::tm local_time;
                        localtime_r(&time, &local_time);
                        std::ostringstream oss;
                        //
                        oss << "(" << +counter << ") ";
                        oss << SensorDescriptor(sensorid).str() << ", ";
                        oss << std::put_time(&local_time, "%F %T %Z");
                        oss << ", " << +fp_value;
                        M5_LOGV("%s", oss.str().c_str());
                      }
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
bool Database::insert_relative_humidity(SensorId sensor_id,
                                        system_clock::time_point at, PctRH rh) {
  constexpr static std::string_view query{
      "INSERT INTO"
      " relative_humidity(sensor_id,at,rh)"
      " VALUES(?,?,?);" // values#1, values#2, values#3
  };
  TimePointAndDouble values{sensor_id, at, rh.count()};
  return insert_values(query, values);
}

//
size_t
Database::read_relative_humidities(OrderBy order,
                                   system_clock::time_point at_begin,
                                   ReadCallback<TimePointAndDouble> callback) {
  constexpr static std::string_view query{
      "SELECT"
      " sensor_id,at,rh"
      " FROM relative_humidity"
      " WHERE at >= :at_begin" // placeholder
      " ORDER BY :order_by;"   // placeholder
  };
  if (auto count =
          read_values(query, std::make_tuple(at_begin, order), callback);
      count) {
    return *count;
  } else {
    return 0;
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
      " WHERE sensor_id=:sensor_id" // placeholder
      " ORDER BY :order_by"         // placeholder
      " LIMIT :limit;"              // placeholder
  };
  if (auto count = read_values(query, std::make_tuple(sensor_id, order, limit),
                               callback);
      count) {
    return *count;
  } else {
    return 0;
  }
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
        // データー表示(デバッグ用)
        if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_VERBOSE) {
          auto &[sensorid, tp, fp_value] = item;
          //
          std::time_t time = system_clock::to_time_t(tp);
          std::tm local_time;
          localtime_r(&time, &local_time);
          std::ostringstream oss;
          //
          oss << "(" << +counter << ") ";
          oss << SensorDescriptor(sensorid).str() << ", ";
          oss << std::put_time(&local_time, "%F %T %Z");
          oss << ", " << +fp_value;
          M5_LOGV("%s", oss.str().c_str());
        }
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
bool Database::insert_pressure(SensorId sensor_id, system_clock::time_point at,
                               HectoPa hpa) {
  static const std::string_view query{
      "INSERT INTO"
      " pressure(sensor_id,at,hpa)"
      " VALUES(?,?,?);" // values#1, values#2, values#3
  };
  TimePointAndDouble values{sensor_id, at, hpa.count()};
  return insert_values(query, values);
}

//
size_t Database::read_pressures(OrderBy order,
                                system_clock::time_point at_begin,
                                ReadCallback<TimePointAndDouble> callback) {
  constexpr static std::string_view query{
      "SELECT"
      " sensor_id,at,hpa"
      " FROM pressure"
      " WHERE at >= :at_begin" // placeholder
      " ORDER BY :order_by;"   // placeholder
  };
  if (auto count =
          read_values(query, std::make_tuple(at_begin, order), callback);
      count) {
    return *count;
  } else {
    return 0;
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
      " WHERE sensor_id=:sensor_id" // placeholder
      " ORDER BY :order_by"         // placeholder
      " LIMIT :limit;"              // placeholder
  };
  if (auto count = read_values(query, std::make_tuple(sensor_id, order, limit),
                               callback);
      count) {
    return *count;
  } else {
    return 0;
  }
}

//
std::vector<Database::TimePointAndDouble>
Database::read_pressures(OrderBy order, SensorId sensor_id, size_t limit) {
  std::vector<TimePointAndDouble> vect{};
  vect.reserve(limit);
  read_pressures(order, sensor_id, limit,
                 [&vect](size_t counter, TimePointAndDouble item) -> bool {
                   // データー表示(デバッグ用)
                   if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_VERBOSE) {
                     auto &[sensorid, tp, fp_value] = item;
                     //
                     std::time_t time = system_clock::to_time_t(tp);
                     std::tm local_time;
                     localtime_r(&time, &local_time);
                     std::ostringstream oss;
                     //
                     oss << "(" << +counter << ") ";
                     oss << SensorDescriptor(sensorid).str() << ", ";
                     oss << std::put_time(&local_time, "%F %T %Z");
                     oss << ", " << +fp_value;
                     M5_LOGV("%s", oss.str().c_str());
                   }
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
bool Database::insert_carbon_dioxide(SensorId sensor_id,
                                     system_clock::time_point at, Ppm ppm,
                                     std::optional<uint16_t> baseline) {
  constexpr static std::string_view query{
      "INSERT INTO"
      " carbon_dioxide(sensor_id,at,ppm,baseline)"
      " VALUES(?,?,?,?);" // values#1, values#2, values#3, values#4
  };
  TimePointAndIntAndOptInt values{sensor_id, at, ppm.value, baseline};
  return insert_values(query, values);
}

//
size_t
Database::read_carbon_deoxides(OrderBy order, system_clock::time_point at_begin,
                               ReadCallback<TimePointAndUInt16> callback) {
  constexpr static std::string_view query{
      "SELECT"
      " sensor_id,at,ppm"
      " FROM carbon_dioxide"
      " WHERE at >= :at_begin" // placeholder
      " ORDER BY :order_by;"   // placeholder
  };
  if (auto count =
          read_values(query, std::make_tuple(at_begin, order), callback);
      count) {
    return *count;
  } else {
    return 0;
  }
}

//
size_t Database::read_carbon_deoxides(
    OrderBy order, system_clock::time_point at_begin,
    ReadCallback<TimePointAndIntAndOptInt> callback) {
  constexpr static std::string_view query{
      "SELECT"
      " sensor_id,at,ppm,baseline"
      " FROM carbon_dioxide"
      " WHERE at >= :at_begin" // placeholder
      " ORDER BY :order_by;"   // placeholder
  };
  if (auto count =
          read_values(query, std::make_tuple(at_begin, order), callback);
      count) {
    return *count;
  } else {
    return 0;
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
      " WHERE sensor_id=:sensor_id" // placeholder
      " ORDER BY :order_by"         // placeholder
      " LIMIT :limit;"              // placeholder
  };
  if (auto count = read_values(query, std::make_tuple(sensor_id, order, limit),
                               callback);
      count) {
    return *count;
  } else {
    return 0;
  }
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
        // データー表示(デバッグ用)
        if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_VERBOSE) {
          auto &[sensorid, tp, int_value, opt_int_value] = item;
          //
          std::time_t time = system_clock::to_time_t(tp);
          std::tm local_time;
          localtime_r(&time, &local_time);
          std::ostringstream oss;
          //
          oss << "(" << +counter << ") ";
          oss << SensorDescriptor(sensorid).str() << ", ";
          oss << std::put_time(&local_time, "%F %T %Z");
          oss << ", " << +int_value;
          oss << ", ";
          if (opt_int_value) {
            oss << opt_int_value.value();
          } else {
            oss << "nullopt";
          }
          M5_LOGV("%s", oss.str().c_str());
        }
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
bool Database::insert_total_voc(SensorId sensor_id, system_clock::time_point at,
                                Ppb ppb, std::optional<uint16_t> baseline) {
  constexpr static std::string_view query{
      "INSERT INTO"
      " total_voc(sensor_id,at,ppb,baseline)"
      " VALUES(?,?,?,?);" // values#1, values#2, values#3, values#4
  };
  TimePointAndIntAndOptInt values{sensor_id, at, ppb.value, baseline};
  return insert_values(query, values);
}

//
size_t Database::read_total_vocs(OrderBy order,
                                 system_clock::time_point at_begin,
                                 ReadCallback<TimePointAndUInt16> callback) {
  constexpr static std::string_view query{
      "SELECT"
      " sensor_id,at,ppb"
      " FROM total_voc"
      " WHERE at >= :at_begin" // placeholder
      " ORDER BY :order_by;"   // placeholder
  };
  if (auto count =
          read_values(query, std::make_tuple(at_begin, order), callback);
      count) {
    return *count;
  } else {
    return 0;
  }
}

//
size_t
Database::read_total_vocs(OrderBy order, system_clock::time_point at_begin,
                          ReadCallback<TimePointAndIntAndOptInt> callback) {
  constexpr static std::string_view query{
      "SELECT"
      " sensor_id,at,ppb,baseline"
      " FROM total_voc"
      " WHERE at >= :at_begin" // placeholder
      " ORDER BY :order_by;"   // placeholder
  };
  if (auto count =
          read_values(query, std::make_tuple(at_begin, order), callback);
      count) {
    return *count;
  } else {
    return 0;
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
      " WHERE sensor_id=:sensor_id" // placeholder
      " ORDER BY :order_by"         // placeholder
      " LIMIT :limit;"              // placeholder
  };
  if (auto count = read_values(query, std::make_tuple(sensor_id, order, limit),
                               callback);
      count) {
    return *count;
  } else {
    return 0;
  }
}

//
std::vector<Database::TimePointAndIntAndOptInt>
Database::read_total_vocs(OrderBy order, SensorId sensor_id, size_t limit) {
  std::vector<TimePointAndIntAndOptInt> vect{};
  vect.reserve(limit);
  read_total_vocs(
      order, sensor_id, limit,
      [&vect](size_t counter, TimePointAndIntAndOptInt item) -> bool {
        // データー表示(デバッグ用)
        if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_VERBOSE) {
          auto &[sensorid, tp, int_value, opt_int_value] = item;
          //
          std::time_t time = system_clock::to_time_t(tp);
          std::tm local_time;
          localtime_r(&time, &local_time);
          std::ostringstream oss;
          //
          oss << "(" << +counter << ") ";
          oss << SensorDescriptor(sensorid).str() << ", ";
          oss << std::put_time(&local_time, "%F %T %Z");
          oss << ", " << +int_value;
          oss << ", ";
          if (opt_int_value) {
            oss << opt_int_value.value();
          } else {
            oss << "nullopt";
          }
          M5_LOGV("%s", oss.str().c_str());
        }
        vect.push_back(item);
        return true;
      });
  vect.shrink_to_fit();
  return std::move(vect);
}

//
//
//
struct Database::Transaction {
  bool onTransaction{false};
  sqlite3 *db{};
  Transaction(sqlite3 *p) : db{p} {
    if (p == nullptr) {
      M5_LOGE("sqlite3 handle is null");
    }
  }
  bool begin() {
    if (db) {
      char *errmsg{nullptr};
      if (sqlite3_exec(db, "BEGIN;", nullptr, nullptr, &errmsg) != SQLITE_OK) {
        if (errmsg) {
          M5_LOGE("%s", errmsg);
        }
        sqlite3_free(errmsg);
        return (onTransaction = false);
      }
    }
    return (onTransaction = true);
  }
  ~Transaction() {
    if (onTransaction) {
      char *errmsg{nullptr};
      if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errmsg) != SQLITE_OK) {
        if (errmsg) {
          M5_LOGE("%s", errmsg);
        }
        sqlite3_free(errmsg);
      }
    }
  }
};

//
//
//
bool Database::begin() {
  if (sqlite3_db) {
    terminate();
  }
  //
  if (psramFound()) {
    M5_LOGI("Database uses to SPIRAM");
    //
    if (auto result =
            sqlite3_config(SQLITE_CONFIG_MALLOC, &_custom_mem_methods);
        result != SQLITE_OK) {
      M5_LOGE("sqlite3_config() failure: %d", result);
      terminate();
      return false;
    }
  }
  //
  if (auto result = sqlite3_config(SQLITE_CONFIG_URI, true);
      result != SQLITE_OK) {
    M5_LOGE("sqlite3_config() failure: %d", result);
    terminate();
    return false;
  }

  //
  if (auto result = sqlite3_initialize(); result != SQLITE_OK) {
    M5_LOGE("sqlite3_initialize() failure: %d", result);
    terminate();
    return false;
  }

  //
  M5_LOGD("sqlite3 open file : %s",
          Application::MEASUREMENTS_DATABASE_FILE_NAME.data());
  if (auto result = sqlite3_open_v2(
          Application::MEASUREMENTS_DATABASE_FILE_NAME.data(), &sqlite3_db,
          SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI,
          nullptr);
      result != SQLITE_OK) {
    M5_LOGE("sqlite3_open_v2() failure: %d", result);
    terminate();
    return false;
  }

  if (sqlite3_db == nullptr) {
    M5_LOGE("sqlite3_db is null");
  }

  //
  if (char *error_msg{};
      sqlite3_exec(sqlite3_db, "PRAGMA auto_vacuum = full;", nullptr, nullptr,
                   &error_msg) != SQLITE_OK) {
    if (error_msg) {
      M5_LOGE("%s", error_msg);
    }
    sqlite3_free(error_msg);
    terminate();
    return false;
  }

  //
  if (char *error_msg{};
      sqlite3_exec(sqlite3_db, "PRAGMA temp_store = 2;", nullptr, nullptr,
                   &error_msg) != SQLITE_OK) {
    if (error_msg) {
      M5_LOGE("%s", error_msg);
    }
    sqlite3_free(error_msg);
    terminate();
    return false;
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
    M5_LOGV("%s", query.data());
    if (char *error_msg{nullptr};
        sqlite3_exec(sqlite3_db, query.data(), nullptr, nullptr, &error_msg) !=
        SQLITE_OK) {
      M5_LOGE("create table error");
      if (error_msg) {
        M5_LOGE("%s", error_msg);
      }
      sqlite3_free(error_msg);
      terminate();
      return false;
    }
  }

  // succsessfully exit
  return (_available = true);
}

//
//
//
void Database::terminate() {
  if (sqlite3_db) {
    if (auto result = sqlite3_close(sqlite3_db); result != SQLITE_OK) {
      M5_LOGE("sqlite3_close() failure: %d", result);
    }
  }
  sqlite3_shutdown();
  sqlite3_db = nullptr;
  _available = false;
}

//
//
//
bool Database::delete_old_measurements_from_database(
    system_clock::time_point delete_of_older_than_tp) {
  constexpr static std::string_view queries[] = {
      {"DELETE FROM temperature WHERE at < ?;"},       // placeholder#1
      {"DELETE FROM relative_humidity WHERE at < ?;"}, // placeholder#1
      {"DELETE FROM carbon_dioxide WHERE at < ?;"},    // placeholder#1
      {"DELETE FROM total_voc WHERE at < ?;"},         // placeholder#1
  };

  if (!available()) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }

  Transaction transaction{sqlite3_db};
  if (!transaction.begin()) {
    return false;
  }

  // delete old rows
  for (auto &query : queries) {
    struct Finalizer {
      sqlite3 *db{nullptr};
      sqlite3_stmt *stmt{nullptr};
      Finalizer(sqlite3 *p) : db{p} {}
      ~Finalizer() {
        sqlite3_finalize(stmt);
        switch (auto code = sqlite3_errcode(db); code) {
        case SQLITE_DONE:
          [[fallthrough]];
        case SQLITE_OK:
          /* No error exit */
          break;
        default:
          M5_LOGE("%s", sqlite3_errstr(code));
          break;
        }
      }
    } fin{sqlite3_db};
    //
    if (sqlite3_prepare_v2(sqlite3_db, query.data(), -1, &fin.stmt, nullptr) !=
        SQLITE_OK) {
      M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
      M5_LOGE("%s", query.data());
      return false;
    }
    if (sqlite3_bind_int64(fin.stmt, 1,
                           static_cast<int64_t>(system_clock::to_time_t(
                               delete_of_older_than_tp))) != SQLITE_OK) {
      M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
      return false;
    }
    // SQL表示(デバッグ用)
    if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_VERBOSE) {
      if (auto p = sqlite3_expanded_sql(fin.stmt); p) {
        M5_LOGV("%s", p);
        sqlite3_free(p);
      }
    }
    //
    auto timeover{steady_clock::now() + RETRY_TIMEOUT};
    while (steady_clock::now() < timeover &&
           sqlite3_step(fin.stmt) != SQLITE_DONE) {
      /**/
    }
    if (steady_clock::now() > timeover) {
      M5_LOGE("sqlite3_step() timeover");
      M5_LOGE("query is \"%s\"", query.data());
      M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
      return false;
    }
  }

  return true;
}

//
//
//
bool Database::insert(const Sensor::MeasurementBme280 &m) {
  if (!available()) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }

  Transaction transaction{sqlite3_db};
  if (!transaction.begin()) {
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

  _latestMeasurementBme280 = m;
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

  Transaction transaction{sqlite3_db};
  if (!transaction.begin()) {
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

  _latestMeasurementSgp30 = m;
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

  Transaction transaction{sqlite3_db};
  if (!transaction.begin()) {
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

  _latestMeasurementScd30 = m;
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

  Transaction transaction{sqlite3_db};
  if (!transaction.begin()) {
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

  _latestMeasurementScd41 = m;
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

  Transaction transaction{sqlite3_db};
  if (!transaction.begin()) {
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

  _latestMeasurementM5Env3 = m;
  M5_LOGD("insert M5Env3 is success.");
  return true;
}

//
bool Database::insert_values(std::string_view query,
                             TimePointAndDouble values_to_insert) {
  auto [sensor_id_to_insert, tp_to_insert, fp_value_to_insert] =
      values_to_insert;

  if (sqlite3_db == nullptr) {
    M5_LOGE("sqlite3_db is null");
    return false;
  }

  struct Finalizer {
    sqlite3 *db{nullptr};
    sqlite3_stmt *stmt{nullptr};
    Finalizer(sqlite3 *p) : db{p} {}
    ~Finalizer() {
      sqlite3_finalize(stmt);
      switch (auto code = sqlite3_errcode(db); code) {
      case SQLITE_DONE:
        [[fallthrough]];
      case SQLITE_OK:
        /* No error exit */
        break;
      default:
        M5_LOGE("%s", sqlite3_errstr(code));
        break;
      }
    }
  } fin{sqlite3_db};

  //
  if (sqlite3_prepare_v2(sqlite3_db, query.data(), -1, &fin.stmt, nullptr) !=
      SQLITE_OK) {
    M5_LOGE("query");
    M5_LOGE("%s", query.data());
    return false;
  }

  //
  if (sqlite3_bind_int64(fin.stmt, 1, sensor_id_to_insert) != SQLITE_OK) {
    return false;
  }
  if (sqlite3_bind_int64(fin.stmt, 2,
                         static_cast<int64_t>(system_clock::to_time_t(
                             tp_to_insert))) != SQLITE_OK) {
    return false;
  }
  if (sqlite3_bind_double(fin.stmt, 3, fp_value_to_insert) != SQLITE_OK) {
    return false;
  }

  // SQL表示(デバッグ用)
  if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_VERBOSE) {
    if (auto p = sqlite3_expanded_sql(fin.stmt); p) {
      M5_LOGV("%s", p);
      sqlite3_free(p);
    }
  }

  //
  auto timeover{steady_clock::now() + RETRY_TIMEOUT};
  while (steady_clock::now() < timeover &&
         sqlite3_step(fin.stmt) != SQLITE_DONE) {
    /**/
  }
  if (steady_clock::now() > timeover) {
    M5_LOGE("sqlite3_step() timeover");
    M5_LOGE("query is \"%s\"", query.data());
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    return false;
  }

  // sqlite3_last_insert_rowid(sqlite3_db);
  return true;
}

//
bool Database::insert_values(std::string_view query,
                             TimePointAndIntAndOptInt values_to_insert) {
  auto [sensor_id_to_insert, tp_to_insert, u16_value_to_insert,
        optional_u16_value_to_insert] = values_to_insert;

  if (sqlite3_db == nullptr) {
    M5_LOGE("sqlite3_db is null");
    return false;
  }

  struct Finalizer {
    sqlite3 *db{nullptr};
    sqlite3_stmt *stmt{nullptr};
    Finalizer(sqlite3 *p) : db{p} {}
    ~Finalizer() {
      sqlite3_finalize(stmt);
      switch (auto code = sqlite3_errcode(db); code) {
      case SQLITE_DONE:
        [[fallthrough]];
      case SQLITE_OK:
        /* No error exit */
        break;
      default:
        M5_LOGE("%s", sqlite3_errstr(code));
        break;
      }
    }
  } fin{sqlite3_db};

  //
  if (sqlite3_prepare_v2(sqlite3_db, query.data(), -1, &fin.stmt, nullptr) !=
      SQLITE_OK) {
    M5_LOGE("query");
    M5_LOGE("%s", query.data());
    return false;
  }
  //
  if (sqlite3_bind_int64(fin.stmt, 1, sensor_id_to_insert) != SQLITE_OK) {
    return false;
  }
  if (sqlite3_bind_int64(fin.stmt, 2,
                         static_cast<int64_t>(system_clock::to_time_t(
                             tp_to_insert))) != SQLITE_OK) {
    return false;
  }
  if (sqlite3_bind_int(fin.stmt, 3, u16_value_to_insert) != SQLITE_OK) {
    return false;
  }
  if (optional_u16_value_to_insert.has_value()) {
    if (sqlite3_bind_int(fin.stmt, 4, *optional_u16_value_to_insert) !=
        SQLITE_OK) {
      return false;
    }
  } else {
    if (sqlite3_bind_null(fin.stmt, 4) != SQLITE_OK) {
      return false;
    }
  }
  // SQL表示(デバッグ用)
  if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_VERBOSE) {
    if (auto p = sqlite3_expanded_sql(fin.stmt); p) {
      M5_LOGV("%s", p);
      sqlite3_free(p);
    }
  }

  //
  auto timeover{steady_clock::now() + RETRY_TIMEOUT};
  while (steady_clock::now() < timeover &&
         sqlite3_step(fin.stmt) != SQLITE_DONE) {
    /**/
  }
  if (steady_clock::now() > timeover) {
    M5_LOGE("sqlite3_step() timeover");
    M5_LOGE("query is \"%s\"", query.data());
    M5_LOGE("%s", sqlite3_errmsg(sqlite3_db));
    return false;
  }

  // sqlite3_last_insert_rowid(sqlite3_db);
  return true;
}

//
std::optional<size_t>
Database::read_values(std::string_view query,
                      std::tuple<system_clock::time_point, OrderBy> placeholder,
                      ReadCallback<TimePointAndDouble> callback) {
  auto [at_begin, orderby] = placeholder;

  if (sqlite3_db == nullptr) {
    M5_LOGE("sqlite3_db is null");
    return std::nullopt;
  }

  struct Finalizer {
    sqlite3 *db{nullptr};
    sqlite3_stmt *stmt{nullptr};
    Finalizer(sqlite3 *p) : db{p} {}
    ~Finalizer() {
      sqlite3_finalize(stmt);
      switch (auto code = sqlite3_errcode(db); code) {
      case SQLITE_DONE:
        [[fallthrough]];
      case SQLITE_OK:
        /* No error exit */
        break;
      default:
        M5_LOGE("%s", sqlite3_errstr(code));
        break;
      }
    }
  } fin{sqlite3_db};

  //
  if (sqlite3_prepare_v2(sqlite3_db, query.data(), -1, &fin.stmt, nullptr) !=
      SQLITE_OK) {
    M5_LOGE("query");
    M5_LOGE("%s", query.data());
    return std::nullopt;
  }
  //
  if (sqlite3_bind_int64(
          fin.stmt, sqlite3_bind_parameter_index(fin.stmt, ":at_begin"),
          static_cast<int64_t>(system_clock::to_time_t(at_begin))) !=
      SQLITE_OK) {
    return std::nullopt;
  }
  {
    std::string_view order_text{orderby == OrderByAtDesc ? "at DESC"
                                                         : "at ASC"};
    if (sqlite3_bind_text(fin.stmt,
                          sqlite3_bind_parameter_index(fin.stmt, ":order_by"),
                          order_text.data(), -1, SQLITE_STATIC) != SQLITE_OK) {
      return std::nullopt;
    }
  }
  // SQL表示(デバッグ用)
  if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_VERBOSE) {
    if (auto p = sqlite3_expanded_sql(fin.stmt); p) {
      M5_LOGV("%s", p);
      sqlite3_free(p);
    }
  }
  //
  size_t counter{1}; // 1 start
  while (sqlite3_step(fin.stmt) == SQLITE_ROW) {
    // number 0 is sensor_id
    int64_t sensor_id = sqlite3_column_int64(fin.stmt, 0);
    int64_t at = sqlite3_column_int64(fin.stmt, 1);
    double fp_value = sqlite3_column_double(fin.stmt, 2);
    TimePointAndDouble values{sensor_id, system_clock::from_time_t(at),
                              fp_value};
    if (bool _continue = callback(counter, values); _continue == false) {
      break;
    }
    counter++;
  }

  return std::make_optional(counter);
}

//
std::optional<size_t>
Database::read_values(std::string_view query,
                      std::tuple<SensorId, OrderBy, size_t> placeholder,
                      ReadCallback<TimePointAndDouble> callback) {
  auto [sensorid, orderby, limit] = placeholder;

  if (sqlite3_db == nullptr) {
    M5_LOGE("sqlite3_db is null");
    return std::nullopt;
  }

  struct Finalizer {
    sqlite3 *db{nullptr};
    sqlite3_stmt *stmt{nullptr};
    Finalizer(sqlite3 *p) : db{p} {}
    ~Finalizer() {
      sqlite3_finalize(stmt);
      switch (auto code = sqlite3_errcode(db); code) {
      case SQLITE_DONE:
        [[fallthrough]];
      case SQLITE_OK:
        /* No error exit */
        break;
      default:
        M5_LOGE("%s", sqlite3_errstr(code));
        break;
      }
    }
  } fin{sqlite3_db};

  if (sqlite3_prepare_v2(sqlite3_db, query.data(), -1, &fin.stmt, nullptr) !=
      SQLITE_OK) {
    M5_LOGE("query");
    M5_LOGE("%s", query.data());
    return std::nullopt;
  }
  //
  if (sqlite3_bind_int64(fin.stmt,
                         sqlite3_bind_parameter_index(fin.stmt, ":sensor_id"),
                         sensorid) != SQLITE_OK) {
    return std::nullopt;
  }
  {
    std::string_view order_text{orderby == OrderByAtDesc ? "at DESC"
                                                         : "at ASC"};
    if (sqlite3_bind_text(fin.stmt,
                          sqlite3_bind_parameter_index(fin.stmt, ":order_by"),
                          order_text.data(), -1, SQLITE_STATIC) != SQLITE_OK) {
      return std::nullopt;
    }
  }
  if (sqlite3_bind_int(fin.stmt,
                       sqlite3_bind_parameter_index(fin.stmt, ":limit"),
                       limit) != SQLITE_OK) {
    return std::nullopt;
  }
  // SQL表示(デバッグ用)
  if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_VERBOSE) {
    if (auto p = sqlite3_expanded_sql(fin.stmt); p) {
      M5_LOGV("%s", p);
      sqlite3_free(p);
    }
  }
  //
  size_t counter{1}; // 1 start
  while (sqlite3_step(fin.stmt) == SQLITE_ROW) {
    // number 0 is sensor_id
    int64_t sensor_id = sqlite3_column_int64(fin.stmt, 0);
    int64_t at = sqlite3_column_int64(fin.stmt, 1);
    double fp_value = sqlite3_column_double(fin.stmt, 2);
    TimePointAndDouble values{sensor_id, system_clock::from_time_t(at),
                              fp_value};
    if (bool _continue = callback(counter, values); _continue == false) {
      break;
    }
    counter++;
  }

  return std::make_optional(counter);
}

//
std::optional<size_t>
Database::read_values(std::string_view query,
                      std::tuple<system_clock::time_point, OrderBy> placeholder,
                      ReadCallback<TimePointAndUInt16> callback) {
  auto [at_begin, orderby] = placeholder;

  if (sqlite3_db == nullptr) {
    M5_LOGE("sqlite3_db is null");
    return std::nullopt;
  }

  struct Finalizer {
    sqlite3 *db{nullptr};
    sqlite3_stmt *stmt{nullptr};
    Finalizer(sqlite3 *p) : db{p} {}
    ~Finalizer() {
      sqlite3_finalize(stmt);
      switch (auto code = sqlite3_errcode(db); code) {
      case SQLITE_DONE:
        [[fallthrough]];
      case SQLITE_OK:
        /* No error exit */
        break;
      default:
        M5_LOGE("%s", sqlite3_errstr(code));
        break;
      }
    }
  } fin{sqlite3_db};

  //
  if (sqlite3_prepare_v2(sqlite3_db, query.data(), -1, &fin.stmt, nullptr) !=
      SQLITE_OK) {
    M5_LOGE("query");
    M5_LOGE("%s", query.data());
    return std::nullopt;
  }
  //
  if (sqlite3_bind_int64(
          fin.stmt, sqlite3_bind_parameter_index(fin.stmt, ":at_begin"),
          static_cast<int64_t>(system_clock::to_time_t(at_begin))) !=
      SQLITE_OK) {
    return std::nullopt;
  }
  {
    std::string_view order_text{orderby == OrderByAtDesc ? "at DESC"
                                                         : "at ASC"};
    if (sqlite3_bind_text(fin.stmt,
                          sqlite3_bind_parameter_index(fin.stmt, ":order_by"),
                          order_text.data(), -1, SQLITE_STATIC) != SQLITE_OK) {
      return std::nullopt;
    }
  }
  // SQL表示(デバッグ用)
  if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_VERBOSE) {
    if (auto p = sqlite3_expanded_sql(fin.stmt); p) {
      M5_LOGV("%s", p);
      sqlite3_free(p);
    }
  }
  //
  size_t counter{1}; // 1 start
  while (sqlite3_step(fin.stmt) == SQLITE_ROW) {
    // number 0 is sensor_id
    int64_t sensor_id = sqlite3_column_int64(fin.stmt, 0);
    int64_t at = sqlite3_column_int64(fin.stmt, 1);
    int int_value = sqlite3_column_int(fin.stmt, 2);
    TimePointAndUInt16 values{sensor_id, system_clock::from_time_t(at),
                              int_value};
    if (bool _continue = callback(counter, values); _continue == false) {
      break;
    }
    counter++;
  }

  return std::make_optional(counter);
}

//
std::optional<size_t>
Database::read_values(std::string_view query,
                      std::tuple<system_clock::time_point, OrderBy> placeholder,
                      ReadCallback<TimePointAndIntAndOptInt> callback) {
  auto [at_begin, orderby] = placeholder;

  if (sqlite3_db == nullptr) {
    M5_LOGE("sqlite3_db is null");
    return std::nullopt;
  }

  struct Finalizer {
    sqlite3 *db{nullptr};
    sqlite3_stmt *stmt{nullptr};
    Finalizer(sqlite3 *p) : db{p} {}
    ~Finalizer() {
      sqlite3_finalize(stmt);
      switch (auto code = sqlite3_errcode(db); code) {
      case SQLITE_DONE:
        [[fallthrough]];
      case SQLITE_OK:
        /* No error exit */
        break;
      default:
        M5_LOGE("%s", sqlite3_errstr(code));
        break;
      }
    }
  } fin{sqlite3_db};

  //
  if (sqlite3_prepare_v2(sqlite3_db, query.data(), -1, &fin.stmt, nullptr) !=
      SQLITE_OK) {
    M5_LOGE("query");
    M5_LOGE("%s", query.data());
    return std::nullopt;
  }
  //
  if (sqlite3_bind_int64(
          fin.stmt, sqlite3_bind_parameter_index(fin.stmt, ":at_begin"),
          static_cast<int64_t>(system_clock::to_time_t(at_begin))) !=
      SQLITE_OK) {
    return std::nullopt;
  }
  {
    std::string_view order_text{orderby == OrderByAtDesc ? "at DESC"
                                                         : "at ASC"};
    if (sqlite3_bind_text(fin.stmt,
                          sqlite3_bind_parameter_index(fin.stmt, ":order_by"),
                          order_text.data(), -1, SQLITE_STATIC) != SQLITE_OK) {
      return std::nullopt;
    }
  }
  // SQL表示(デバッグ用)
  if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_VERBOSE) {
    if (auto p = sqlite3_expanded_sql(fin.stmt); p) {
      M5_LOGV("%s", p);
      sqlite3_free(p);
    }
  }
  //
  size_t counter{1}; // 1 start
  while (sqlite3_step(fin.stmt) == SQLITE_ROW) {
    // number 0 is sensor_id
    int64_t sensor_id = sqlite3_column_int64(fin.stmt, 0);
    int64_t at = sqlite3_column_int64(fin.stmt, 1);
    int int_value = sqlite3_column_int(fin.stmt, 2);
    std::optional<uint16_t> optional_int_value{
        sqlite3_column_type(fin.stmt, 3) == SQLITE_INTEGER
            ? std::make_optional(sqlite3_column_int(fin.stmt, 3))
            : std::nullopt};
    TimePointAndIntAndOptInt values{sensor_id, system_clock::from_time_t(at),
                                    int_value, optional_int_value};
    if (bool _continue = callback(counter, values); _continue == false) {
      break;
    }
    counter++;
  }

  return std::make_optional(counter);
}

//
std::optional<size_t>
Database::read_values(std::string_view query,
                      std::tuple<SensorId, OrderBy, size_t> placeholder,
                      ReadCallback<TimePointAndIntAndOptInt> callback) {
  auto [sensorid, orderby, limit] = placeholder;

  if (sqlite3_db == nullptr) {
    M5_LOGE("sqlite3 sqlite3_db is null");
    return std::nullopt;
  }

  struct Finalizer {
    sqlite3 *db{nullptr};
    sqlite3_stmt *stmt{nullptr};
    Finalizer(sqlite3 *p) : db{p} {}
    ~Finalizer() {
      sqlite3_finalize(stmt);
      switch (auto code = sqlite3_errcode(db); code) {
      case SQLITE_DONE:
        [[fallthrough]];
      case SQLITE_OK:
        /* No error exit */
        break;
      default:
        M5_LOGE("%s", sqlite3_errstr(code));
        break;
      }
    }
  } fin{sqlite3_db};

  if (sqlite3_prepare_v2(sqlite3_db, query.data(), -1, &fin.stmt, nullptr) !=
      SQLITE_OK) {
    M5_LOGE("query");
    M5_LOGE("%s", query.data());
    return std::nullopt;
  }
  //
  if (sqlite3_bind_int64(fin.stmt,
                         sqlite3_bind_parameter_index(fin.stmt, ":sensor_id"),
                         sensorid) != SQLITE_OK) {
    return std::nullopt;
  }
  if (std::string_view order_text{orderby == OrderByAtDesc ? "at DESC"
                                                           : "at ASC"};
      sqlite3_bind_text(fin.stmt,
                        sqlite3_bind_parameter_index(fin.stmt, ":order_by"),
                        order_text.data(), -1, SQLITE_STATIC) != SQLITE_OK) {
    return std::nullopt;
  }
  if (sqlite3_bind_int(fin.stmt,
                       sqlite3_bind_parameter_index(fin.stmt, ":limit"),
                       limit) != SQLITE_OK) {
    return std::nullopt;
  }
  // SQL表示(デバッグ用)
  if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_VERBOSE) {
    if (auto p = sqlite3_expanded_sql(fin.stmt); p) {
      M5_LOGV("%s", p);
      sqlite3_free(p);
    }
  }
  //
  size_t counter{1}; // 1 start
  while (sqlite3_step(fin.stmt) == SQLITE_ROW) {
    // number 0 is sensor_id
    int64_t sensor_id = sqlite3_column_int64(fin.stmt, 0);
    int64_t at = sqlite3_column_int64(fin.stmt, 1);
    int int_value = sqlite3_column_int(fin.stmt, 2);
    std::optional<uint16_t> optional_int_value{
        sqlite3_column_type(fin.stmt, 3) == SQLITE_INTEGER
            ? std::make_optional(sqlite3_column_int(fin.stmt, 3))
            : std::nullopt};
    TimePointAndIntAndOptInt values{sensor_id, system_clock::from_time_t(at),
                                    int_value, optional_int_value};
    if (bool _continue = callback(counter, values); _continue == false) {
      break;
    }
    counter++;
  }

  return std::make_optional(counter);
}
