// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "LocalDatabase.hpp"
#include "Application.hpp"
#include <chrono>
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
LocalDatabase::RowId LocalDatabase::insert_temperature(SensorId sensor_id,
                                                       std::time_t at,
                                                       DegC degc) {
  constexpr static std::string_view query{"INSERT INTO"
                                          " temperature(sensor_id,at,degc)"
                                          " VALUES(?,?,?);"};
  const CentiDegC tCeltius = std::chrono::round<CentiDegC>(degc);
  rowid_temperature =
      raw_insert_time_and_float(query, sensor_id, at, tCeltius.count());
  return rowid_temperature;
}

//
size_t LocalDatabase::get_temperatures_desc(
    SensorId sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndFloat callback) {
  constexpr static std::string_view query{"SELECT"
                                          " sensor_id,at,degc"
                                          " FROM temperature"
                                          " WHERE sensor_id=?"
                                          " ORDER BY at DESC"
                                          " LIMIT ?;"};
  if (!available()) {
    M5_LOGI("database is not available.");
    return 0;
  }
  return raw_get_n_desc_time_and_float(query, sensor_id, limit, callback);
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
LocalDatabase::RowId LocalDatabase::insert_relative_humidity(SensorId sensor_id,
                                                             std::time_t at,
                                                             PctRH rh) {
  constexpr static std::string_view query{"INSERT INTO"
                                          " relative_humidity(sensor_id,at,rh)"
                                          " VALUES(?,?,?);"};
  if (!available()) {
    M5_LOGI("database is not available.");
    return 0;
  }
  rowid_relative_humidity =
      raw_insert_time_and_float(query, sensor_id, at, rh.count());
  return rowid_relative_humidity;
}

//
size_t LocalDatabase::get_relative_humidities_desc(
    SensorId sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndFloat callback) {
  constexpr static std::string_view query{"SELECT"
                                          " sensor_id,at,rh"
                                          " FROM relative_humidity"
                                          " WHERE sensor_id=?"
                                          " ORDER BY at DESC"
                                          " LIMIT ?;"};
  if (!available()) {
    M5_LOGI("database is not available.");
    return 0;
  }
  return raw_get_n_desc_time_and_float(query, sensor_id, limit, callback);
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
LocalDatabase::RowId LocalDatabase::insert_pressure(SensorId sensor_id,
                                                    std::time_t at,
                                                    HectoPa hpa) {
  static const std::string_view query{"INSERT INTO"
                                      " pressure(sensor_id,at,hpa)"
                                      " VALUES(?,?,?);"};
  if (!available()) {
    M5_LOGI("database is not available.");
    return 0;
  }
  rowid_pressure = raw_insert_time_and_float(query, sensor_id, at, hpa.count());
  return rowid_pressure;
}

//
size_t LocalDatabase::get_pressures_desc(
    SensorId sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndFloat callback) {
  constexpr static std::string_view query{"SELECT"
                                          " sensor_id,at,hpa"
                                          " FROM pressure"
                                          " WHERE sensor_id=?"
                                          " ORDER BY at DESC"
                                          " LIMIT ?;"};
  if (!available()) {
    M5_LOGI("database is not available.");
    return 0;
  }
  return raw_get_n_desc_time_and_float(query, sensor_id, limit, callback);
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
LocalDatabase::RowId
LocalDatabase::insert_carbon_dioxide(SensorId sensor_id, std::time_t at,
                                     Ppm ppm, const uint16_t *baseline) {
  constexpr static std::string_view query{
      "INSERT INTO"
      " carbon_dioxide(sensor_id,at,ppm,baseline)"
      " VALUES(?,?,?,?);"};
  if (!available()) {
    M5_LOGI("database is not available.");
    return 0;
  }
  rowid_carbon_dioxide = raw_insert_time_and_uint16_and_nullable_uint16(
      query, sensor_id, at, ppm.value, baseline);
  return rowid_carbon_dioxide;
}

//
size_t LocalDatabase::get_carbon_deoxides_desc(
    SensorId sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndUint16AndNullableUint16 callback) {
  constexpr static std::string_view query{"SELECT"
                                          " sensor_id,at,ppm,baseline"
                                          " FROM carbon_dioxide"
                                          " WHERE sensor_id=?"
                                          " ORDER BY at DESC"
                                          " LIMIT ?;"};
  if (!available()) {
    M5_LOGI("database is not available.");
    return 0;
  }
  return raw_get_n_time_and_uint16_and_nullable_uint16(query, sensor_id, limit,
                                                       callback);
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
LocalDatabase::RowId LocalDatabase::insert_total_voc(SensorId sensor_id,
                                                     std::time_t at, Ppb ppb,
                                                     const uint16_t *baseline) {
  constexpr static std::string_view query{
      "INSERT INTO"
      " total_voc(sensor_id,at,ppb,baseline)"
      " VALUES(?,?,?,?);"};
  if (!available()) {
    M5_LOGI("database is not available.");
    return 0;
  }
  rowid_total_voc = raw_insert_time_and_uint16_and_nullable_uint16(
      query, sensor_id, at, ppb.value, baseline);
  return rowid_total_voc;
}

//
size_t LocalDatabase::get_total_vocs_desc(
    SensorId sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndUint16AndNullableUint16 callback) {
  constexpr static std::string_view query{"SELECT"
                                          " sensor_id,at,ppb,baseline"
                                          " FROM total_voc"
                                          " WHERE sensor_id=?"
                                          " ORDER BY at DESC"
                                          " LIMIT ?;"};
  if (!available()) {
    M5_LOGI("database is not available.");
    return 0;
  }
  return raw_get_n_time_and_uint16_and_nullable_uint16(query, sensor_id, limit,
                                                       callback);
}

//
std::tuple<bool, std::time_t, BaselineECo2>
LocalDatabase::get_latest_baseline_eco2(SensorId sensor_id) {
  constexpr static std::string_view query{"SELECT"
                                          " sensor_id" // 0
                                          ",at"        // 1
                                          ",baseline"  // 2
                                          " FROM carbon_dioxide"
                                          " WHERE sensor_id=?"
                                          " AND baseline NOTNULL"
                                          " ORDER BY at DESC"
                                          " LIMIT 1;"};
  if (!available()) {
    M5_LOGI("database is not available.");
    return std::make_tuple(false, 0, BaselineECo2(0));
  }
  auto result = raw_get_latest_baseline(query, sensor_id);
  return std::make_tuple(std::get<0>(result), std::get<1>(result),
                         BaselineECo2(std::get<2>(result)));
}

//
std::tuple<bool, std::time_t, BaselineTotalVoc>
LocalDatabase::get_latest_baseline_total_voc(SensorId sensor_id) {
  constexpr static std::string_view query{"SELECT"
                                          " sensor_id" // 0
                                          ",at"        // 1
                                          ",baseline"  // 2
                                          " FROM total_voc"
                                          " WHERE sensor_id=?"
                                          " AND baseline NOTNULL"
                                          " ORDER BY at DESC"
                                          " LIMIT 1;"};
  if (!available()) {
    M5_LOGI("database is not available.");
    return std::make_tuple(false, 0, BaselineTotalVoc(0));
  }
  auto result = raw_get_latest_baseline(query, sensor_id);
  return std::make_tuple(std::get<0>(result), std::get<1>(result),
                         BaselineTotalVoc(std::get<2>(result)));
}

//
//
//
bool LocalDatabase::begin() noexcept {
  char *error_msg = nullptr;

  if (auto result = sqlite3_initialize(); result != SQLITE_OK) {
    M5_LOGE("sqlite3_initialize() failure: %d", result);
    goto error_exit;
  }
  //
  if (auto result =
          sqlite3_open_v2(sqlite3_filename.data(), &database,
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
    if (auto result = sqlite3_exec(database, schema_temperature.data(), nullptr,
                                   nullptr, &error_msg);
        result != SQLITE_OK) {
      M5_LOGE("%s", error_msg);
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
void LocalDatabase::terminate() noexcept {
  if (database) {
    if (auto result = sqlite3_close(database); result != SQLITE_OK) {
      M5_LOGE("sqlite3_close() failure: %d", result);
    }
  }
  database = nullptr;
  _available = false;
}

//
//
//
bool LocalDatabase::insert(const MeasurementBme280 &in) {
  if (!available()) {
    M5_LOGI("database is not available.");
    return false;
  }

  if (auto rawid = insert_temperature(SensorId(in.second.sensor_descriptor),
                                      system_clock::to_time_t(in.first),
                                      in.second.temperature);
      rawid < 0) {
    M5_LOGE("insert_temperature() failure.");
    return false;
  }

  if (auto rawid = insert_pressure(SensorId(in.second.sensor_descriptor),
                                   system_clock::to_time_t(in.first),
                                   in.second.pressure);
      rawid < 0) {
    M5_LOGE("insert_pressure() failure.");
    return false;
  }

  if (auto rawid = insert_relative_humidity(
          SensorId(in.second.sensor_descriptor),
          system_clock::to_time_t(in.first), in.second.relative_humidity);
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
bool LocalDatabase::insert(const MeasurementSgp30 &in) {
  if (!available()) {
    M5_LOGI("database is not available.");
    return false;
  }
  if (in.second.tvoc_baseline.has_value()) {
    BaselineTotalVoc tvoc_base = in.second.tvoc_baseline.value();
    if (auto rawid = insert_total_voc(SensorId(in.second.sensor_descriptor),
                                      system_clock::to_time_t(in.first),
                                      in.second.tvoc, &tvoc_base.value);
        rawid < 0) {
      M5_LOGE("insert_total_voc() failure.");
      return false;
    }
  } else {
    if (auto rawid = insert_total_voc(SensorId(in.second.sensor_descriptor),
                                      system_clock::to_time_t(in.first),
                                      in.second.tvoc, nullptr);
        rawid < 0) {
      M5_LOGE("insert_total_voc() failure.");
      return false;
    }
  }

  if (in.second.eCo2_baseline.has_value()) {
    BaselineECo2 eco2_base = in.second.eCo2_baseline.value();
    if (auto rawid =
            insert_carbon_dioxide(SensorId(in.second.sensor_descriptor),
                                  system_clock::to_time_t(in.first),
                                  in.second.eCo2, &eco2_base.value);
        rawid < 0) {
      M5_LOGE("insert_carbon_dioxide() failure.");
      return false;
    }
  } else {
    if (auto rawid = insert_carbon_dioxide(
            SensorId(in.second.sensor_descriptor),
            system_clock::to_time_t(in.first), in.second.eCo2, nullptr);
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
bool LocalDatabase::insert(const MeasurementScd30 &in) {
  if (!available()) {
    M5_LOGI("database is not available.");
    return false;
  }

  if (auto rawid = insert_temperature(SensorId(in.second.sensor_descriptor),
                                      system_clock::to_time_t(in.first),
                                      in.second.temperature);
      rawid < 0) {
    M5_LOGE("insert_temperature() failure.");
    return false;
  }

  if (auto rawid = insert_relative_humidity(
          SensorId(in.second.sensor_descriptor),
          system_clock::to_time_t(in.first), in.second.relative_humidity);
      rawid < 0) {
    M5_LOGE("insert_relative_humidity() failure.");
    return false;
  }

  if (auto rawid = insert_carbon_dioxide(SensorId(in.second.sensor_descriptor),
                                         system_clock::to_time_t(in.first),
                                         in.second.co2, nullptr);
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
bool LocalDatabase::insert(const MeasurementScd41 &in) {
  if (!available()) {
    M5_LOGI("database is not available.");
    return false;
  }

  if (auto rawid = insert_temperature(SensorId(in.second.sensor_descriptor),
                                      system_clock::to_time_t(in.first),
                                      in.second.temperature);
      rawid < 0) {
    M5_LOGE("insert_temperature() failure.");
    return false;
  }

  if (auto rawid = insert_relative_humidity(
          SensorId(in.second.sensor_descriptor),
          system_clock::to_time_t(in.first), in.second.relative_humidity);
      rawid < 0) {
    M5_LOGE("insert_relative_humidity() failure.");
    return false;
  }

  if (auto rawid = insert_carbon_dioxide(SensorId(in.second.sensor_descriptor),
                                         system_clock::to_time_t(in.first),
                                         in.second.co2, nullptr);
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
bool LocalDatabase::insert(const MeasurementM5Env3 &in) {
  if (!available()) {
    M5_LOGI("database is not available.");
    return false;
  }

  if (auto rawid = insert_temperature(SensorId(in.second.sensor_descriptor),
                                      system_clock::to_time_t(in.first),
                                      in.second.temperature);
      rawid < 0) {
    M5_LOGE("insert_temperature() failure.");
    return false;
  }

  if (auto rawid = insert_pressure(SensorId(in.second.sensor_descriptor),
                                   system_clock::to_time_t(in.first),
                                   in.second.pressure);
      rawid < 0) {
    M5_LOGE("insert_pressure() failure.");
    return false;
  }

  if (auto rawid = insert_relative_humidity(
          SensorId(in.second.sensor_descriptor),
          system_clock::to_time_t(in.first), in.second.relative_humidity);
      rawid < 0) {
    M5_LOGE("insert_relative_humidity() failure.");
    return false;
  }

  M5_LOGD("insert M5Env3 is success.");
  return true;
}

//
//
//
int64_t LocalDatabase::raw_insert_time_and_float(std::string_view query,
                                                 SensorId sensor_id,
                                                 std::time_t time,
                                                 float float_value) {
  sqlite3_stmt *stmt{nullptr};

  if (!database) {
    M5_LOGE("sqlite3 database is null");
    goto error_exit;
  }

  if (auto result =
          sqlite3_prepare_v2(database, query.data(), -1, &stmt, nullptr);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    goto error_exit;
  }
  //
  if (auto result = sqlite3_bind_int64(stmt, 1, sensor_id);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    goto error_exit;
  }
  if (auto result = sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(time));
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    goto error_exit;
  }
  if (auto result =
          sqlite3_bind_double(stmt, 3, static_cast<double>(float_value));
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    goto error_exit;
  }
  //
  for (int32_t retry = 0; sqlite3_step(stmt) != SQLITE_DONE; ++retry) {
    M5_LOGV("sqlite3_step()");
    if (retry >= RETRY_COUNT) {
      M5_LOGE("sqlite3_step() over");
      M5_LOGE("query is \"%s\"", query.data());
      M5_LOGE("%s", sqlite3_errmsg(database));
      retry_failed();
      goto error_exit;
    }
  }

  if (auto result = sqlite3_finalize(stmt); result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    stmt = nullptr; // omit double finalize
    goto error_exit;
  }

  return sqlite3_last_insert_rowid(database);

error_exit:
  sqlite3_finalize(stmt);
  return -1;
}

//
//
//
int64_t LocalDatabase::raw_insert_time_and_uint16_and_nullable_uint16(
    std::string_view query, SensorId sensor_id, std::time_t time,
    uint16_t uint16_value, const uint16_t *nullable_uint16_value) {
  sqlite3_stmt *stmt{nullptr};

  if (!database) {
    M5_LOGE("sqlite3 database is null");
    goto error_exit;
  }

  if (auto result =
          sqlite3_prepare_v2(database, query.data(), -1, &stmt, nullptr);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    goto error_exit;
  }
  //
  if (auto result = sqlite3_bind_int64(stmt, 1, sensor_id);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    goto error_exit;
  }
  if (auto result = sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(time));
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    goto error_exit;
  }
  if (auto result = sqlite3_bind_int(stmt, 3, uint16_value);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    goto error_exit;
  }
  if (nullable_uint16_value) {
    if (auto result = sqlite3_bind_int(stmt, 4, *nullable_uint16_value);
        result != SQLITE_OK) {
      M5_LOGE("%s", sqlite3_errmsg(database));
      goto error_exit;
    }
  } else {
    if (auto result = sqlite3_bind_null(stmt, 4); result != SQLITE_OK) {
      M5_LOGE("%s", sqlite3_errmsg(database));
      goto error_exit;
    }
  }
  //
  for (int32_t retry = 0; sqlite3_step(stmt) != SQLITE_DONE; ++retry) {
    M5_LOGV("sqlite3_step()");
    if (retry >= RETRY_COUNT) {
      M5_LOGE("sqlite3_step() over");
      M5_LOGE("query is \"%s\"", query.data());
      M5_LOGE("%s", sqlite3_errmsg(database));
      retry_failed();
      goto error_exit;
    }
  }

  if (auto result = sqlite3_finalize(stmt); result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    stmt = nullptr; // omit double finalize
    goto error_exit;
  }

  return sqlite3_last_insert_rowid(database);

error_exit:
  sqlite3_finalize(stmt);
  return -1;
}

//
//
//
size_t LocalDatabase::raw_get_n_desc_time_and_float(
    std::string_view query, SensorId sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndFloat callback) {
  sqlite3_stmt *stmt{nullptr};

  if (!database) {
    M5_LOGE("sqlite3 database is null");
    goto error_exit;
  }

  if (auto result =
          sqlite3_prepare_v2(database, query.data(), -1, &stmt, nullptr);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    goto error_exit;
  }
  //
  if (auto result = sqlite3_bind_int64(stmt, 1, sensor_id);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    goto error_exit;
  }
  if (auto result = sqlite3_bind_int(stmt, 2, static_cast<int32_t>(limit));
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    goto error_exit;
  }
  //
  {
    size_t counter = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      M5_LOGV("sqlite3_step()");
      // number 0 is sensor_id
      int64_t at = sqlite3_column_int64(stmt, 1);
      double degc = sqlite3_column_double(stmt, 2);
      if (callback(counter, at, static_cast<float>(degc)) == false) {
        break;
      }
      counter++;
    }

    if (auto result = sqlite3_finalize(stmt); result != SQLITE_OK) {
      M5_LOGE("%s", sqlite3_errmsg(database));
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
size_t LocalDatabase::raw_get_n_time_and_uint16_and_nullable_uint16(
    std::string_view query, SensorId sensor_id, size_t limit,
    CallbackRowTimeAndUint16AndNullableUint16 callback) {
  sqlite3_stmt *stmt{nullptr};

  if (!database) {
    M5_LOGE("sqlite3 database is null");
    goto error_exit;
  }

  if (auto result =
          sqlite3_prepare_v2(database, query.data(), -1, &stmt, nullptr);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    goto error_exit;
  }
  //
  if (auto result = sqlite3_bind_int64(stmt, 1, sensor_id);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    goto error_exit;
  }
  if (auto result = sqlite3_bind_int(stmt, 2, static_cast<int32_t>(limit));
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    goto error_exit;
  }
  //
  {
    size_t counter = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      M5_LOGV("sqlite3_step()");
      // number 0 is sensor_id
      int64_t at = sqlite3_column_int64(stmt, 1);
      int v1 = sqlite3_column_int(stmt, 2);
      bool has_v2 = (sqlite3_column_type(stmt, 3) != SQLITE_NULL);
      int v2 = (has_v2) ? sqlite3_column_int(stmt, 3) : 0;
      if (callback(counter, at, v1, v2, has_v2) == false) {
        break;
      }
      counter++;
    }

    if (auto result = sqlite3_finalize(stmt); result != SQLITE_OK) {
      M5_LOGE("%s", sqlite3_errmsg(database));
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
std::tuple<bool, std::time_t, BaselineSGP30T>
LocalDatabase::raw_get_latest_baseline(std::string_view query,
                                       SensorId sensor_id) {
  sqlite3_stmt *stmt{nullptr};
  auto retval{std::make_tuple(false, 0, 0)};

  if (!database) {
    M5_LOGE("sqlite3 database is null");
    goto error_exit;
  }

  if (query == nullptr) {
    M5_LOGE("invalid query");
    goto error_exit;
  }

  if (auto result =
          sqlite3_prepare_v2(database, query.data(), -1, &stmt, nullptr);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    goto error_exit;
  }
  //
  if (auto result = sqlite3_bind_int64(stmt, 1, sensor_id);
      result != SQLITE_OK) {
    M5_LOGE("%s", sqlite3_errmsg(database));
    goto error_exit;
  }
  //
  {
    size_t counter = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      M5_LOGV("sqlite3_step()");
      // number 0 is sensor_id
      int64_t at = sqlite3_column_int64(stmt, 1);
      if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
        uint16_t baseline = sqlite3_column_int(stmt, 2);
        retval = std::make_tuple(true, static_cast<std::time_t>(at),
                                 static_cast<BaselineSGP30T>(baseline));
      }
      counter++;
    }
    if (auto result = sqlite3_finalize(stmt); result != SQLITE_OK) {
      M5_LOGE("%s", sqlite3_errmsg(database));
      stmt = nullptr; // omit double finalize
      goto error_exit;
    }

    return retval;
  }

error_exit:
  sqlite3_finalize(stmt);
  return retval;
}
