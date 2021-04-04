// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "local_database.hpp"

#include <Arduino.h>

//
//
//
static constexpr char schema_temperature[] =
    "CREATE TABLE IF NOT EXISTS temperature"
    "(id INTEGER PRIMARY KEY AUTOINCREMENT"
    ",sensor_id TEXT NOT NULL"
    ",at INTEGER NOT NULL"
    ",degc REAL NOT NULL"
    ");";
//
int64_t LocalDatabase::insert_temperature(const char *sensor_id,
                                          const time_t &at, float degc) {
  constexpr char query[] = "INSERT INTO"
                           " temperature(sensor_id,at,degc)"
                           " VALUES(?,?,?);";
  rawid_temperature = raw_insert_time_and_float(query, sensor_id, at, degc);
  return rawid_temperature;
}
//
size_t LocalDatabase::get_temperatures_desc(
    const char *sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndFloat callback) {
  constexpr char query[] = "SELECT"
                           " sensor_id,at,degc"
                           " FROM temperature"
                           " WHERE sensor_id=?"
                           " ORDER BY at DESC"
                           " LIMIT ?;";
  return raw_get_n_desc_time_and_float(query, sensor_id, limit, callback);
}

//
//
//
static constexpr char schema_relative_humidity[] =
    "CREATE TABLE IF NOT EXISTS relative_humidity"
    "(id INTEGER PRIMARY KEY AUTOINCREMENT"
    ",sensor_id TEXT NOT NULL"
    ",at INTEGER NOT NULL"
    ",rh REAL NOT NULL"
    ");";
//
int64_t LocalDatabase::insert_relative_humidity(const char *sensor_id,
                                                const time_t &at, float rh) {
  constexpr char query[] = "INSERT INTO"
                           " relative_humidity(sensor_id,at,rh)"
                           " VALUES(?,?,?);";
  rawid_relative_humidity = raw_insert_time_and_float(query, sensor_id, at, rh);
  return rawid_relative_humidity;
}
//
size_t LocalDatabase::get_relative_humidities_desc(
    const char *sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndFloat callback) {
  constexpr char query[] = "SELECT"
                           " sensor_id,at,rh"
                           " FROM relative_humidity"
                           " WHERE sensor_id=?"
                           " ORDER BY at DESC"
                           " LIMIT ?;";
  return raw_get_n_desc_time_and_float(query, sensor_id, limit, callback);
}

//
//
//
static constexpr char schema_pressure[] =
    "CREATE TABLE IF NOT EXISTS pressure"
    "(id INTEGER PRIMARY KEY AUTOINCREMENT"
    ",sensor_id TEXT NOT NULL"
    ",at INTEGER NOT NULL"
    ",hpa REAL NOT NULL"
    ");";
//
int64_t LocalDatabase::insert_pressure(const char *sensor_id, const time_t &at,
                                       float hpa) {
  constexpr char query[] = "INSERT INTO"
                           " pressure(sensor_id,at,hpa)"
                           " VALUES(?,?,?);";
  rawid_pressure = raw_insert_time_and_float(query, sensor_id, at, hpa);
  return rawid_pressure;
}
//
size_t LocalDatabase::get_pressures_desc(
    const char *sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndFloat callback) {
  constexpr char query[] = "SELECT"
                           " sensor_id,at,hpa"
                           " FROM pressure"
                           " WHERE sensor_id=?"
                           " ORDER BY at DESC"
                           " LIMIT ?;";
  return raw_get_n_desc_time_and_float(query, sensor_id, limit, callback);
}

//
//
//
static constexpr char schema_carbon_dioxide[] =
    "CREATE TABLE IF NOT EXISTS carbon_dioxide"
    "(id INTEGER PRIMARY KEY AUTOINCREMENT"
    ",sensor_id TEXT NOT NULL"
    ",at INTEGER NOT NULL"
    ",ppm REAL NOT NULL"
    ",baseline INTEGER"
    ");";
//
int64_t LocalDatabase::insert_carbon_dioxide(const char *sensor_id,
                                             const time_t &at, uint16_t ppm,
                                             const uint16_t *baseline) {
  constexpr char query[] = "INSERT INTO"
                           " carbon_dioxide(sensor_id,at,ppm,baseline)"
                           " VALUES(?,?,?,?);";
  rawid_carbon_dioxide = raw_insert_time_and_uint16_and_nullable_uint16(
      query, sensor_id, at, ppm, baseline);
  return rawid_carbon_dioxide;
}
//
size_t LocalDatabase::get_carbon_deoxides_desc(
    const char *sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndUint16AndNullableUint16 callback) {
  constexpr char query[] = "SELECT"
                           " sensor_id,at,ppm,baseline"
                           " FROM carbon_dioxide"
                           " WHERE sensor_id=?"
                           " ORDER BY at DESC"
                           " LIMIT ?;";
  return raw_get_n_time_and_uint16_and_nullable_uint16(query, sensor_id, limit,
                                                       callback);
}

//
//
//
static constexpr char schema_total_voc[] =
    "CREATE TABLE IF NOT EXISTS total_voc"
    "(id INTEGER PRIMARY KEY AUTOINCREMENT"
    ",sensor_id TEXT NOT NULL"
    ",at INTEGER NOT NULL"
    ",ppb REAL NOT NULL"
    ",baseline INTEGER"
    ");";
//
int64_t LocalDatabase::insert_total_voc(const char *sensor_id, const time_t &at,
                                        uint16_t ppb,
                                        const uint16_t *baseline) {
  constexpr char query[] = "INSERT INTO"
                           " total_voc(sensor_id,at,ppb,baseline)"
                           " VALUES(?,?,?,?);";
  rawid_total_voc = raw_insert_time_and_uint16_and_nullable_uint16(
      query, sensor_id, at, ppb, baseline);
  return rawid_total_voc;
}
//
size_t LocalDatabase::get_total_vocs_desc(
    const char *sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndUint16AndNullableUint16 callback) {
  constexpr char query[] = "SELECT"
                           " sensor_id,at,ppb,baseline"
                           " FROM total_voc"
                           " WHERE sensor_id=?"
                           " ORDER BY at DESC"
                           " LIMIT ?;";
  return raw_get_n_time_and_uint16_and_nullable_uint16(query, sensor_id, limit,
                                                       callback);
}

//
//
//
bool LocalDatabase::beginDb() {
  int result;
  result = sqlite3_initialize();
  if (result != SQLITE_OK) {
    ESP_LOGE("main", "sqlite3_initialize() failed. reason:\"%s\"",
             sqlite3_errmsg(database));
    goto error;
  }
  //
  result = sqlite3_open_v2(sqlite3_filename, &database,
                           SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE("main", "sqlite3_open() failed. reason:\"%s\"",
             sqlite3_errmsg(database));
    goto error;
  }
  //
  {
    char *error_msg = nullptr;
    // temperature
    result = sqlite3_exec(database, schema_temperature, NULL, NULL, &error_msg);
    if (result != SQLITE_OK) {
      ESP_LOGE("main", "%s", error_msg);
      goto error;
    }
    // relative humidity
    result = sqlite3_exec(database, schema_relative_humidity, NULL, NULL,
                          &error_msg);
    if (result != SQLITE_OK) {
      ESP_LOGE("main", "%s", error_msg);
      goto error;
    }
    // pressure
    result = sqlite3_exec(database, schema_pressure, NULL, NULL, &error_msg);
    if (result != SQLITE_OK) {
      ESP_LOGE("main", "%s", error_msg);
      goto error;
    }
    // carbon dioxide
    result =
        sqlite3_exec(database, schema_carbon_dioxide, NULL, NULL, &error_msg);
    if (result != SQLITE_OK) {
      ESP_LOGE("main", "%s", error_msg);
      goto error;
    }
    // total voc
    result = sqlite3_exec(database, schema_total_voc, NULL, NULL, &error_msg);
    if (result != SQLITE_OK) {
      ESP_LOGE("main", "%s", error_msg);
      goto error;
    }
  }

  ESP_LOGI("main", "beginDb() is success.");
  return true;
//
error:
  return false;
}

//
//
//
bool LocalDatabase::insert(const Bme280::TempHumiPres &bme) {
  if (!healthy()) {
    return false;
  }
  int64_t t = insert_temperature(bme.sensor_id, bme.at, bme.temperature);
  int64_t p = insert_pressure(bme.sensor_id, bme.at, bme.pressure);
  int64_t h =
      insert_relative_humidity(bme.sensor_id, bme.at, bme.relative_humidity);
  if (t >= 0 && p >= 0 && h >= 0) {
    ESP_LOGI("main", "storeTheMeasurements(BME280) is success.");
    return true;
  } else {
    return false;
  }
}

//
//
//
bool LocalDatabase::insert(const Sgp30::TvocEco2 &sgp) {
  if (!healthy()) {
    return false;
  }
  int64_t t =
      insert_total_voc(sgp.sensor_id, sgp.at, sgp.tvoc, &sgp.tvoc_baseline);
  int64_t c = insert_carbon_dioxide(sgp.sensor_id, sgp.at, sgp.eCo2,
                                    &sgp.eCo2_baseline);
  if (t >= 0 && c >= 0) {
    ESP_LOGI("main", "storeTheMeasurements(SGP30) is success.");
    return true;
  } else {
    return false;
  }
}

//
//
//
bool LocalDatabase::insert(const Scd30::Co2TempHumi &scd) {
  if (!healthy()) {
    return false;
  }
  int64_t t = insert_temperature(scd.sensor_id, scd.at, scd.temperature);
  int64_t p =
      insert_relative_humidity(scd.sensor_id, scd.at, scd.relative_humidity);
  int64_t c = insert_carbon_dioxide(scd.sensor_id, scd.at, scd.co2, nullptr);
  if (t >= 0 && p >= 0 && c >= 0) {
    ESP_LOGI("main", "storeTheMeasurements(SCD30) is success.");
    return true;
  } else {
    return false;
  }
}

//
//
//
int64_t LocalDatabase::raw_insert_time_and_float(const char *query,
                                                 const char *sensor_id,
                                                 time_t time,
                                                 float float_value) {
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result = sqlite3_bind_text(stmt, 1, sensor_id, strlen(sensor_id),
                             SQLITE_TRANSIENT);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(time));
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_double(stmt, 3, static_cast<double>(float_value));
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  while (sqlite3_step(stmt) != SQLITE_DONE) {
  }

  result = sqlite3_finalize(stmt);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
  }
  return sqlite3_last_insert_rowid(database);

error:
  sqlite3_finalize(stmt);
  return -1;
}

//
//
//
int64_t LocalDatabase::raw_insert_time_and_uint16_and_nullable_uint16(
    const char *query, const char *sensor_id, time_t time,
    uint16_t uint16_value, const uint16_t *nullable_uint16_value) {
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result = sqlite3_bind_text(stmt, 1, sensor_id, strlen(sensor_id),
                             SQLITE_TRANSIENT);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(time));
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int(stmt, 3, uint16_value);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  if (nullable_uint16_value) {
    result = sqlite3_bind_int(stmt, 4, *nullable_uint16_value);
    if (result != SQLITE_OK) {
      ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
      goto error;
    }
  } else {
    result = sqlite3_bind_null(stmt, 4);
    if (result != SQLITE_OK) {
      ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
      goto error;
    }
  }
  //
  while (sqlite3_step(stmt) != SQLITE_DONE) {
  }

  result = sqlite3_finalize(stmt);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "sqlite3_finalize() failed. reason: %d.", result);
  }

  return sqlite3_last_insert_rowid(database);

error:
  sqlite3_finalize(stmt);
  return -1;
}

//
//
//
size_t LocalDatabase::raw_get_n_desc_time_and_float(
    const char *query, const char *sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndFloat callback) {
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result = sqlite3_bind_text(stmt, 1, sensor_id, strlen(sensor_id),
                             SQLITE_TRANSIENT);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int(stmt, 2, static_cast<int32_t>(limit));
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  {
    size_t counter = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      // number 0 is sensor_id
      int64_t at = sqlite3_column_int64(stmt, 1);
      double degc = sqlite3_column_double(stmt, 2);
      if (callback(counter, at, static_cast<float>(degc)) == false) {
        break;
      }
      counter++;
    }
    sqlite3_finalize(stmt);

    return counter;
  }

error:
  sqlite3_finalize(stmt);
  return 0;
}

//
//
//
size_t LocalDatabase::raw_get_n_time_and_uint16_and_nullable_uint16(
    const char *query, const char *sensor_id, size_t limit,
    CallbackRowTimeAndUint16AndNullableUint16 callback) {
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result = sqlite3_bind_text(stmt, 1, sensor_id, strlen(sensor_id),
                             SQLITE_TRANSIENT);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int(stmt, 2, static_cast<int32_t>(limit));
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  {
    size_t counter = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
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
    sqlite3_finalize(stmt);

    return counter;
  }

error:
  sqlite3_finalize(stmt);
  return 0;
}