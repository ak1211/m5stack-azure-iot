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
      return heap_caps_aligned_alloc(8, size, MALLOC_CAP_SPIRAM);
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
void Database::Sqlite3Deleter::operator()(sqlite3 *ptr) const {
  if (auto result = sqlite3_close(ptr); result != SQLITE_OK) {
    M5_LOGE("sqlite3_close() failure: %d", result);
  }
}

//
//
//
void Database::Sqlite3StmtDeleter::operator()(sqlite3_stmt *ptr) const {
  auto code = sqlite3_finalize(ptr);
  switch (code) {
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

//
//
//
struct Database::Transaction {
  bool onTransaction{false};
  Sqlite3PointerUnique &db;
  //
  Transaction(Sqlite3PointerUnique &in) : db{in} {}
  //
  bool begin() {
    char *errmsg{nullptr};
    if (db) {
      if (sqlite3_exec(db.get(), "BEGIN;", nullptr, nullptr, &errmsg) ==
          SQLITE_OK) {
        return (onTransaction = true);
      }
    }
    if (errmsg) {
      M5_LOGE("%s", errmsg);
    }
    sqlite3_free(errmsg);
    return (onTransaction = false);
  }
  //
  ~Transaction() {
    if (onTransaction) {
      char *errmsg{nullptr};
      if (sqlite3_exec(db.get(), "COMMIT;", nullptr, nullptr, &errmsg) !=
          SQLITE_OK) {
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
bool Database::begin(const std::string &database_filename) {
  //
  if (psramFound()) {
    //
    M5_LOGI("Database uses heap on SPIRAM");
    if (auto result =
            sqlite3_config(SQLITE_CONFIG_MALLOC, &_custom_mem_methods);
        result != SQLITE_OK) {
      M5_LOGE("sqlite3_config() failure: %d", result);
      terminate();
      return false;
    }
    //
#ifdef SQLITE_ENABLE_MEMSYS5
    if (_database_use_preallocated_memory == nullptr) {
      _database_use_preallocated_memory = heap_caps_aligned_alloc(
          8, DATABASE_USE_PREALLOCATED_MEMORY_SIZE, MALLOC_CAP_SPIRAM);
    }
    //
    if (_database_use_preallocated_memory) {
      M5_LOGI("Database uses on pre-allocated memory");
      if (auto result = sqlite3_config(
              SQLITE_CONFIG_HEAP, _database_use_preallocated_memory,
              DATABASE_USE_PREALLOCATED_MEMORY_SIZE, 64);
          result != SQLITE_OK) {
        M5_LOGE("sqlite3_config() failure: %d", result);
        return false;
      }
    }
#endif
  }
  //
  if (auto result = sqlite3_config(SQLITE_CONFIG_URI, true);
      result != SQLITE_OK) {
    M5_LOGE("sqlite3_config() failure: %d", result);
    return false;
  }

  //
  if (auto result = sqlite3_initialize(); result != SQLITE_OK) {
    M5_LOGE("sqlite3_initialize() failure: %d", result);
    return false;
  }

  M5_LOGD("sqlite3 open file : %s", database_filename.c_str());
  //
  {
    sqlite3 *pDatabase{nullptr};
    if (auto result = sqlite3_open_v2(
            database_filename.c_str(), &pDatabase,
            SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI,
            nullptr);
        result == SQLITE_OK) {
      // set db handle to smartpointer
      _sqlite3_db.reset(pDatabase);
    } else {
      M5_LOGE("sqlite3_open_v2() failure: %d", result);
      return false;
    }
  }

  // guard
  if (!_sqlite3_db) {
    M5_LOGE("sqlite3_db is null");
    return false;
  }

  //
  if (char *error_msg{};
      sqlite3_exec(_sqlite3_db.get(), "PRAGMA auto_vacuum = full;", nullptr,
                   nullptr, &error_msg) != SQLITE_OK) {
    if (error_msg) {
      M5_LOGE("%s", error_msg);
    }
    sqlite3_free(error_msg);
    return false;
  }

  //
  if (char *error_msg{};
      sqlite3_exec(_sqlite3_db.get(), "PRAGMA temp_store = 2;", nullptr,
                   nullptr, &error_msg) != SQLITE_OK) {
    if (error_msg) {
      M5_LOGE("%s", error_msg);
    }
    sqlite3_free(error_msg);
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
        sqlite3_exec(_sqlite3_db.get(), query.data(), nullptr, nullptr,
                     &error_msg) != SQLITE_OK) {
      M5_LOGE("create table error");
      if (error_msg) {
        M5_LOGE("%s", error_msg);
      }
      sqlite3_free(error_msg);
      return false;
    }
  }

  // succsessfully exit
  return true;
}

//
//
//
void Database::terminate() {
  _sqlite3_db.reset();
  sqlite3_shutdown();
#ifdef SQLITE_ENABLE_MEMSYS5
  free(_database_use_preallocated_memory);
  _database_use_preallocated_memory = nullptr;
#endif
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

  // guard
  if (!_sqlite3_db) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }

  Transaction transaction{_sqlite3_db};
  if (!transaction.begin()) {
    return false;
  }

  M5_LOGI("delete old rows.");

  // delete old rows
  for (auto &query : queries) {
    Sqlite3StmtPointerUnique stmt;
    if (sqlite3_stmt * pStmt{nullptr};
        sqlite3_prepare_v2(_sqlite3_db.get(), query.data(), -1, &pStmt,
                           nullptr) == SQLITE_OK) {
      // set statement handle to smartpointer
      if (pStmt) {
        stmt.reset(pStmt);
      }
    } else {
      M5_LOGE("%s", sqlite3_errmsg(_sqlite3_db.get()));
      M5_LOGE("%s", query.data());
      return false;
    }
    if (sqlite3_bind_int64(stmt.get(), 1,
                           static_cast<int64_t>(system_clock::to_time_t(
                               delete_of_older_than_tp))) != SQLITE_OK) {
      M5_LOGE("%s", sqlite3_errmsg(_sqlite3_db.get()));
      return false;
    }
    // SQL表示(デバッグ用)
    if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_DEBUG) {
      std::time_t time = system_clock::to_time_t(delete_of_older_than_tp);
      std::tm local_time;
      localtime_r(&time, &local_time);
      std::ostringstream oss;
      //
      if (auto p = sqlite3_expanded_sql(stmt.get()); p) {
        oss << std::string_view(p);
        sqlite3_free(p);
      }
      //
      oss << " (" << static_cast<int64_t>(time) << " is "
          << std::put_time(&local_time, "%F %T %Z") << ")";
      M5_LOGD("%s", oss.str().c_str());
    }
    //
    auto timeover{steady_clock::now() + RETRY_TIMEOUT};
    while (steady_clock::now() < timeover &&
           sqlite3_step(stmt.get()) != SQLITE_DONE) {
      /**/
    }
    if (steady_clock::now() > timeover) {
      M5_LOGE("sqlite3_step() timeover");
      M5_LOGE("query is \"%s\"", query.data());
      M5_LOGE("%s", sqlite3_errmsg(_sqlite3_db.get()));
      return false;
    }
  }

  return true;
}

//
//
//
bool Database::insert(const Sensor::MeasurementBme280 &m) {
  // guard
  if (!_sqlite3_db) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }

  Transaction transaction{_sqlite3_db};
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
  // guard
  if (!_sqlite3_db) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }

  Transaction transaction{_sqlite3_db};
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
  // guard
  if (!_sqlite3_db) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }

  Transaction transaction{_sqlite3_db};
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
  // guard
  if (!_sqlite3_db) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }

  Transaction transaction{_sqlite3_db};
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
  // guard
  if (!_sqlite3_db) {
    M5_LOGI("sqlite3_db is not available.");
    return false;
  }

  Transaction transaction{_sqlite3_db};
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

  // guard
  if (!_sqlite3_db) {
    M5_LOGE("sqlite3_db is null");
    return false;
  }

  Sqlite3StmtPointerUnique stmt;
  if (sqlite3_stmt * pStmt{nullptr};
      sqlite3_prepare_v2(_sqlite3_db.get(), query.data(), -1, &pStmt,
                         nullptr) == SQLITE_OK) {
    // set statement handle to smartpointer
    if (pStmt) {
      stmt.reset(pStmt);
    }
  } else {
    M5_LOGE("%s", sqlite3_errmsg(_sqlite3_db.get()));
    M5_LOGE("%s", query.data());
    return false;
  }

  //
  if (sqlite3_bind_int64(stmt.get(), 1, sensor_id_to_insert) != SQLITE_OK) {
    return false;
  }
  if (sqlite3_bind_int64(stmt.get(), 2,
                         static_cast<int64_t>(system_clock::to_time_t(
                             tp_to_insert))) != SQLITE_OK) {
    return false;
  }
  if (sqlite3_bind_double(stmt.get(), 3, fp_value_to_insert) != SQLITE_OK) {
    return false;
  }

  // SQL表示(デバッグ用)
  if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_DEBUG) {
    //
    std::time_t time = system_clock::to_time_t(tp_to_insert);
    std::tm local_time;
    localtime_r(&time, &local_time);
    std::ostringstream oss;
    //
    if (auto p = sqlite3_expanded_sql(stmt.get()); p) {
      oss << std::string_view(p);
      sqlite3_free(p);
    }
    //
    oss << " (" << static_cast<int64_t>(time) << " is "
        << std::put_time(&local_time, "%F %T %Z") << ")";
    M5_LOGD("%s", oss.str().c_str());
  }

  //
  auto timeover{steady_clock::now() + RETRY_TIMEOUT};
  while (steady_clock::now() < timeover &&
         sqlite3_step(stmt.get()) != SQLITE_DONE) {
    /**/
  }
  if (steady_clock::now() > timeover) {
    M5_LOGE("sqlite3_step() timeover");
    M5_LOGE("query is \"%s\"", query.data());
    M5_LOGE("%s", sqlite3_errmsg(_sqlite3_db.get()));
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

  // guard
  if (!_sqlite3_db) {
    M5_LOGE("sqlite3_db is null");
    return false;
  }

  Sqlite3StmtPointerUnique stmt;
  if (sqlite3_stmt * pStmt{nullptr};
      sqlite3_prepare_v2(_sqlite3_db.get(), query.data(), -1, &pStmt,
                         nullptr) == SQLITE_OK) {
    // set statement handle to smartpointer
    if (pStmt) {
      stmt.reset(pStmt);
    }
  } else {
    M5_LOGE("%s", sqlite3_errmsg(_sqlite3_db.get()));
    M5_LOGE("%s", query.data());
    return false;
  }

  //
  if (sqlite3_bind_int64(stmt.get(), 1, sensor_id_to_insert) != SQLITE_OK) {
    return false;
  }
  if (sqlite3_bind_int64(stmt.get(), 2,
                         static_cast<int64_t>(system_clock::to_time_t(
                             tp_to_insert))) != SQLITE_OK) {
    return false;
  }
  if (sqlite3_bind_int(stmt.get(), 3, u16_value_to_insert) != SQLITE_OK) {
    return false;
  }
  if (optional_u16_value_to_insert.has_value()) {
    if (sqlite3_bind_int(stmt.get(), 4, *optional_u16_value_to_insert) !=
        SQLITE_OK) {
      return false;
    }
  } else {
    if (sqlite3_bind_null(stmt.get(), 4) != SQLITE_OK) {
      return false;
    }
  }
  // SQL表示(デバッグ用)
  if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_DEBUG) {
    //
    std::time_t time = system_clock::to_time_t(tp_to_insert);
    std::tm local_time;
    localtime_r(&time, &local_time);
    std::ostringstream oss;
    //
    if (auto p = sqlite3_expanded_sql(stmt.get()); p) {
      oss << std::string_view(p);
      sqlite3_free(p);
    }
    //
    oss << " (" << static_cast<int64_t>(time) << " is "
        << std::put_time(&local_time, "%F %T %Z") << ")";
    M5_LOGD("%s", oss.str().c_str());
  }

  //
  auto timeover{steady_clock::now() + RETRY_TIMEOUT};
  while (steady_clock::now() < timeover &&
         sqlite3_step(stmt.get()) != SQLITE_DONE) {
    /**/
  }
  if (steady_clock::now() > timeover) {
    M5_LOGE("sqlite3_step() timeover");
    M5_LOGE("query is \"%s\"", query.data());
    M5_LOGE("%s", sqlite3_errmsg(_sqlite3_db.get()));
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

  // guard
  if (!_sqlite3_db) {
    M5_LOGE("sqlite3_db is null");
    return std::nullopt;
  }

  Sqlite3StmtPointerUnique stmt;
  if (sqlite3_stmt * pStmt{nullptr};
      sqlite3_prepare_v2(_sqlite3_db.get(), query.data(), -1, &pStmt,
                         nullptr) == SQLITE_OK) {
    // set statement handle to smartpointer
    if (pStmt) {
      stmt.reset(pStmt);
    }
  } else {
    M5_LOGE("%s", sqlite3_errmsg(_sqlite3_db.get()));
    M5_LOGE("%s", query.data());
    return std::nullopt;
  }

  //
  if (sqlite3_bind_int64(
          stmt.get(), sqlite3_bind_parameter_index(stmt.get(), ":at_begin"),
          static_cast<int64_t>(system_clock::to_time_t(at_begin))) !=
      SQLITE_OK) {
    return std::nullopt;
  }
  {
    std::string_view order_text{orderby == OrderByAtDesc ? "at DESC"
                                                         : "at ASC"};
    if (sqlite3_bind_text(stmt.get(),
                          sqlite3_bind_parameter_index(stmt.get(), ":order_by"),
                          order_text.data(), -1, SQLITE_STATIC) != SQLITE_OK) {
      return std::nullopt;
    }
  }
  // SQL表示(デバッグ用)
  if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_DEBUG) {
    if (auto p = sqlite3_expanded_sql(stmt.get()); p) {
      M5_LOGD("%s", p);
      sqlite3_free(p);
    }
  }
  //
  size_t counter{1}; // 1 start
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    // number 0 is sensor_id
    int64_t sensor_id = sqlite3_column_int64(stmt.get(), 0);
    int64_t at = sqlite3_column_int64(stmt.get(), 1);
    double fp_value = sqlite3_column_double(stmt.get(), 2);
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

  // guard
  if (!_sqlite3_db) {
    M5_LOGE("sqlite3_db is null");
    return std::nullopt;
  }

  Sqlite3StmtPointerUnique stmt;
  if (sqlite3_stmt * pStmt{nullptr};
      sqlite3_prepare_v2(_sqlite3_db.get(), query.data(), -1, &pStmt,
                         nullptr) == SQLITE_OK) {
    // set statement handle to smartpointer
    if (pStmt) {
      stmt.reset(pStmt);
    }
  } else {
    M5_LOGE("%s", sqlite3_errmsg(_sqlite3_db.get()));
    M5_LOGE("%s", query.data());
    return std::nullopt;
  }

  //
  if (sqlite3_bind_int64(stmt.get(),
                         sqlite3_bind_parameter_index(stmt.get(), ":sensor_id"),
                         sensorid) != SQLITE_OK) {
    return std::nullopt;
  }
  {
    std::string_view order_text{orderby == OrderByAtDesc ? "at DESC"
                                                         : "at ASC"};
    if (sqlite3_bind_text(stmt.get(),
                          sqlite3_bind_parameter_index(stmt.get(), ":order_by"),
                          order_text.data(), -1, SQLITE_STATIC) != SQLITE_OK) {
      return std::nullopt;
    }
  }
  if (sqlite3_bind_int(stmt.get(),
                       sqlite3_bind_parameter_index(stmt.get(), ":limit"),
                       limit) != SQLITE_OK) {
    return std::nullopt;
  }
  // SQL表示(デバッグ用)
  if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_DEBUG) {
    if (auto p = sqlite3_expanded_sql(stmt.get()); p) {
      M5_LOGD("%s", p);
      sqlite3_free(p);
    }
  }
  //
  size_t counter{1}; // 1 start
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    // number 0 is sensor_id
    int64_t sensor_id = sqlite3_column_int64(stmt.get(), 0);
    int64_t at = sqlite3_column_int64(stmt.get(), 1);
    double fp_value = sqlite3_column_double(stmt.get(), 2);
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

  // guard
  if (!_sqlite3_db) {
    M5_LOGE("sqlite3_db is null");
    return std::nullopt;
  }

  Sqlite3StmtPointerUnique stmt;
  if (sqlite3_stmt * pStmt{nullptr};
      sqlite3_prepare_v2(_sqlite3_db.get(), query.data(), -1, &pStmt,
                         nullptr) == SQLITE_OK) {
    // set statement handle to smartpointer
    if (pStmt) {
      stmt.reset(pStmt);
    }
  } else {
    M5_LOGE("%s", sqlite3_errmsg(_sqlite3_db.get()));
    M5_LOGE("%s", query.data());
    return std::nullopt;
  }

  //
  if (sqlite3_bind_int64(
          stmt.get(), sqlite3_bind_parameter_index(stmt.get(), ":at_begin"),
          static_cast<int64_t>(system_clock::to_time_t(at_begin))) !=
      SQLITE_OK) {
    return std::nullopt;
  }
  {
    std::string_view order_text{orderby == OrderByAtDesc ? "at DESC"
                                                         : "at ASC"};
    if (sqlite3_bind_text(stmt.get(),
                          sqlite3_bind_parameter_index(stmt.get(), ":order_by"),
                          order_text.data(), -1, SQLITE_STATIC) != SQLITE_OK) {
      return std::nullopt;
    }
  }
  // SQL表示(デバッグ用)
  if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_DEBUG) {
    if (auto p = sqlite3_expanded_sql(stmt.get()); p) {
      M5_LOGD("%s", p);
      sqlite3_free(p);
    }
  }
  //
  size_t counter{1}; // 1 start
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    // number 0 is sensor_id
    int64_t sensor_id = sqlite3_column_int64(stmt.get(), 0);
    int64_t at = sqlite3_column_int64(stmt.get(), 1);
    int int_value = sqlite3_column_int(stmt.get(), 2);
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

  // guard
  if (!_sqlite3_db) {
    M5_LOGE("sqlite3_db is null");
    return std::nullopt;
  }

  Sqlite3StmtPointerUnique stmt;
  if (sqlite3_stmt * pStmt{nullptr};
      sqlite3_prepare_v2(_sqlite3_db.get(), query.data(), -1, &pStmt,
                         nullptr) == SQLITE_OK) {
    // set statement handle to smartpointer
    if (pStmt) {
      stmt.reset(pStmt);
    }
  } else {
    M5_LOGE("%s", sqlite3_errmsg(_sqlite3_db.get()));
    M5_LOGE("%s", query.data());
    return std::nullopt;
  }

  //
  if (sqlite3_bind_int64(
          stmt.get(), sqlite3_bind_parameter_index(stmt.get(), ":at_begin"),
          static_cast<int64_t>(system_clock::to_time_t(at_begin))) !=
      SQLITE_OK) {
    return std::nullopt;
  }
  {
    std::string_view order_text{orderby == OrderByAtDesc ? "at DESC"
                                                         : "at ASC"};
    if (sqlite3_bind_text(stmt.get(),
                          sqlite3_bind_parameter_index(stmt.get(), ":order_by"),
                          order_text.data(), -1, SQLITE_STATIC) != SQLITE_OK) {
      return std::nullopt;
    }
  }
  // SQL表示(デバッグ用)
  if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_DEBUG) {
    if (auto p = sqlite3_expanded_sql(stmt.get()); p) {
      M5_LOGD("%s", p);
      sqlite3_free(p);
    }
  }
  //
  size_t counter{1}; // 1 start
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    // number 0 is sensor_id
    int64_t sensor_id = sqlite3_column_int64(stmt.get(), 0);
    int64_t at = sqlite3_column_int64(stmt.get(), 1);
    int int_value = sqlite3_column_int(stmt.get(), 2);
    std::optional<uint16_t> optional_int_value{
        sqlite3_column_type(stmt.get(), 3) == SQLITE_INTEGER
            ? std::make_optional(sqlite3_column_int(stmt.get(), 3))
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

  // guard
  if (!_sqlite3_db) {
    M5_LOGE("sqlite3 sqlite3_db is null");
    return std::nullopt;
  }

  Sqlite3StmtPointerUnique stmt;
  if (sqlite3_stmt * pStmt{nullptr};
      sqlite3_prepare_v2(_sqlite3_db.get(), query.data(), -1, &pStmt,
                         nullptr) == SQLITE_OK) {
    // set statement handle to smartpointer
    if (pStmt) {
      stmt.reset(pStmt);
    }
  } else {
    M5_LOGE("%s", sqlite3_errmsg(_sqlite3_db.get()));
    M5_LOGE("%s", query.data());
    return std::nullopt;
  }

  //
  if (sqlite3_bind_int64(stmt.get(),
                         sqlite3_bind_parameter_index(stmt.get(), ":sensor_id"),
                         sensorid) != SQLITE_OK) {
    return std::nullopt;
  }
  if (std::string_view order_text{orderby == OrderByAtDesc ? "at DESC"
                                                           : "at ASC"};
      sqlite3_bind_text(stmt.get(),
                        sqlite3_bind_parameter_index(stmt.get(), ":order_by"),
                        order_text.data(), -1, SQLITE_STATIC) != SQLITE_OK) {
    return std::nullopt;
  }
  if (sqlite3_bind_int(stmt.get(),
                       sqlite3_bind_parameter_index(stmt.get(), ":limit"),
                       limit) != SQLITE_OK) {
    return std::nullopt;
  }
  // SQL表示(デバッグ用)
  if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_DEBUG) {
    if (auto p = sqlite3_expanded_sql(stmt.get()); p) {
      M5_LOGD("%s", p);
      sqlite3_free(p);
    }
  }
  //
  size_t counter{1}; // 1 start
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    // number 0 is sensor_id
    int64_t sensor_id = sqlite3_column_int64(stmt.get(), 0);
    int64_t at = sqlite3_column_int64(stmt.get(), 1);
    int int_value = sqlite3_column_int(stmt.get(), 2);
    std::optional<uint16_t> optional_int_value{
        sqlite3_column_type(stmt.get(), 3) == SQLITE_INTEGER
            ? std::make_optional(sqlite3_column_int(stmt.get(), 3))
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
