// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "local_database.hpp"
#include <Arduino.h>

constexpr static const char *TAG = "DbModule";

//
//
//
constexpr static const char schema_temperature[] =
    "CREATE TABLE IF NOT EXISTS temperature"
    "(id INTEGER PRIMARY KEY AUTOINCREMENT"
    ",sensor_id INTEGER NOT NULL"
    ",at INTEGER NOT NULL"
    ",degc REAL NOT NULL"
    ");";
//
int64_t LocalDatabase::insert_temperature(uint64_t sensor_id, std::time_t at,
                                          DegC degc) {
  constexpr static const char query[] = "INSERT INTO"
                                        " temperature(sensor_id,at,degc)"
                                        " VALUES(?,?,?);";
  rawid_temperature =
      raw_insert_time_and_float(query, sensor_id, at, degc.value);
  return rawid_temperature;
}
//
size_t LocalDatabase::get_temperatures_desc(
    uint64_t sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndFloat callback) {
  constexpr static const char query[] = "SELECT"
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
constexpr static const char schema_relative_humidity[] =
    "CREATE TABLE IF NOT EXISTS relative_humidity"
    "(id INTEGER PRIMARY KEY AUTOINCREMENT"
    ",sensor_id INTEGER NOT NULL"
    ",at INTEGER NOT NULL"
    ",rh REAL NOT NULL"
    ");";
//
int64_t LocalDatabase::insert_relative_humidity(uint64_t sensor_id,
                                                std::time_t at, PcRH rh) {
  constexpr static const char query[] = "INSERT INTO"
                                        " relative_humidity(sensor_id,at,rh)"
                                        " VALUES(?,?,?);";
  rawid_relative_humidity =
      raw_insert_time_and_float(query, sensor_id, at, rh.value);
  return rawid_relative_humidity;
}
//
size_t LocalDatabase::get_relative_humidities_desc(
    uint64_t sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndFloat callback) {
  constexpr static const char query[] = "SELECT"
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
constexpr static const char schema_pressure[] =
    "CREATE TABLE IF NOT EXISTS pressure"
    "(id INTEGER PRIMARY KEY AUTOINCREMENT"
    ",sensor_id INTEGER NOT NULL"
    ",at INTEGER NOT NULL"
    ",hpa REAL NOT NULL"
    ");";
//
int64_t LocalDatabase::insert_pressure(uint64_t sensor_id, std::time_t at,
                                       HPa hpa) {
  constexpr static const char query[] = "INSERT INTO"
                                        " pressure(sensor_id,at,hpa)"
                                        " VALUES(?,?,?);";
  rawid_pressure = raw_insert_time_and_float(query, sensor_id, at, hpa.value);
  return rawid_pressure;
}
//
size_t LocalDatabase::get_pressures_desc(
    uint64_t sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndFloat callback) {
  constexpr static const char query[] = "SELECT"
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
constexpr static char schema_carbon_dioxide[] =
    "CREATE TABLE IF NOT EXISTS carbon_dioxide"
    "(id INTEGER PRIMARY KEY AUTOINCREMENT"
    ",sensor_id INTEGER NOT NULL"
    ",at INTEGER NOT NULL"
    ",ppm REAL NOT NULL"
    ",baseline INTEGER"
    ");";
//
int64_t LocalDatabase::insert_carbon_dioxide(uint64_t sensor_id, std::time_t at,
                                             Ppm ppm,
                                             const uint16_t *baseline) {
  constexpr static const char query[] =
      "INSERT INTO"
      " carbon_dioxide(sensor_id,at,ppm,baseline)"
      " VALUES(?,?,?,?);";
  rawid_carbon_dioxide = raw_insert_time_and_uint16_and_nullable_uint16(
      query, sensor_id, at, ppm.value, baseline);
  return rawid_carbon_dioxide;
}
//
size_t LocalDatabase::get_carbon_deoxides_desc(
    uint64_t sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndUint16AndNullableUint16 callback) {
  constexpr static const char query[] = "SELECT"
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
constexpr static const char schema_total_voc[] =
    "CREATE TABLE IF NOT EXISTS total_voc"
    "(id INTEGER PRIMARY KEY AUTOINCREMENT"
    ",sensor_id INTEGER NOT NULL"
    ",at INTEGER NOT NULL"
    ",ppb REAL NOT NULL"
    ",baseline INTEGER"
    ");";
//
int64_t LocalDatabase::insert_total_voc(uint64_t sensor_id, std::time_t at,
                                        Ppb ppb, const uint16_t *baseline) {
  constexpr static const char query[] = "INSERT INTO"
                                        " total_voc(sensor_id,at,ppb,baseline)"
                                        " VALUES(?,?,?,?);";
  rawid_total_voc = raw_insert_time_and_uint16_and_nullable_uint16(
      query, sensor_id, at, ppb.value, baseline);
  return rawid_total_voc;
}
//
size_t LocalDatabase::get_total_vocs_desc(
    uint64_t sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndUint16AndNullableUint16 callback) {
  constexpr static const char query[] = "SELECT"
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
    ESP_LOGE(TAG, "sqlite3_initialize() failed. reason:\"%s\"",
             sqlite3_errmsg(database));
    goto error;
  }
  //
  result = sqlite3_open_v2(sqlite3_filename.c_str(), &database,
                           SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "sqlite3_open() failed. reason:\"%s\"",
             sqlite3_errmsg(database));
    goto error;
  }
  //
  {
    char *error_msg = nullptr;
    // temperature
    result = sqlite3_exec(database, schema_temperature, nullptr, nullptr,
                          &error_msg);
    if (result != SQLITE_OK) {
      ESP_LOGE(TAG, "%s", error_msg);
      goto error;
    }
    // relative humidity
    result = sqlite3_exec(database, schema_relative_humidity, nullptr, nullptr,
                          &error_msg);
    if (result != SQLITE_OK) {
      ESP_LOGE(TAG, "%s", error_msg);
      goto error;
    }
    // pressure
    result =
        sqlite3_exec(database, schema_pressure, nullptr, nullptr, &error_msg);
    if (result != SQLITE_OK) {
      ESP_LOGE(TAG, "%s", error_msg);
      goto error;
    }
    // carbon dioxide
    result = sqlite3_exec(database, schema_carbon_dioxide, nullptr, nullptr,
                          &error_msg);
    if (result != SQLITE_OK) {
      ESP_LOGE(TAG, "%s", error_msg);
      goto error;
    }
    // total voc
    result =
        sqlite3_exec(database, schema_total_voc, nullptr, nullptr, &error_msg);
    if (result != SQLITE_OK) {
      ESP_LOGE(TAG, "%s", error_msg);
      goto error;
    }
  }

  ESP_LOGD(TAG, "beginDb() is success.");
  return true;
//
error:
  return false;
}

//
//
//
bool LocalDatabase::insert(const TempHumiPres &temp_humi_pres) {
  if (!healthy()) {
    return false;
  }
  int64_t t =
      insert_temperature(temp_humi_pres.sensor_descriptor.id, temp_humi_pres.at,
                         temp_humi_pres.temperature.value);
  int64_t p = insert_pressure(temp_humi_pres.sensor_descriptor.id,
                              temp_humi_pres.at, temp_humi_pres.pressure.value);
  int64_t h = insert_relative_humidity(temp_humi_pres.sensor_descriptor.id,
                                       temp_humi_pres.at,
                                       temp_humi_pres.relative_humidity.value);
  if (t >= 0 && p >= 0 && h >= 0) {
    ESP_LOGD(TAG, "insert TempHumiPres is success.");
    return true;
  } else {
    return false;
  }
}

//
//
//
bool LocalDatabase::insert(const TvocEco2 &tvoc_eco2) {
  if (!healthy()) {
    return false;
  }
  int64_t t =
      insert_total_voc(tvoc_eco2.sensor_descriptor.id, tvoc_eco2.at,
                       tvoc_eco2.tvoc.value, &tvoc_eco2.tvoc_baseline.value);
  int64_t c = insert_carbon_dioxide(tvoc_eco2.sensor_descriptor.id,
                                    tvoc_eco2.at, tvoc_eco2.eCo2.value,
                                    &tvoc_eco2.eCo2_baseline.value);
  if (t >= 0 && c >= 0) {
    ESP_LOGI(TAG, "insert TvocEco2 is success.");
    return true;
  } else {
    return false;
  }
}

//
//
//
bool LocalDatabase::insert(const Co2TempHumi &co2_temp_humi) {
  if (!healthy()) {
    return false;
  }
  int64_t t =
      insert_temperature(co2_temp_humi.sensor_descriptor.id, co2_temp_humi.at,
                         co2_temp_humi.temperature.value);
  int64_t p = insert_relative_humidity(co2_temp_humi.sensor_descriptor.id,
                                       co2_temp_humi.at,
                                       co2_temp_humi.relative_humidity.value);
  int64_t c =
      insert_carbon_dioxide(co2_temp_humi.sensor_descriptor.id,
                            co2_temp_humi.at, co2_temp_humi.co2.value, nullptr);
  if (t >= 0 && p >= 0 && c >= 0) {
    ESP_LOGI(TAG, "insert Co2TempHumi is success.");
    return true;
  } else {
    return false;
  }
}

//
//
//
int64_t LocalDatabase::raw_insert_time_and_float(const char *query,
                                                 uint64_t sensor_id,
                                                 std::time_t time,
                                                 float float_value) {
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result = sqlite3_bind_int64(stmt, 1, sensor_id);
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(time));
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_double(stmt, 3, static_cast<double>(float_value));
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  while (sqlite3_step(stmt) != SQLITE_DONE) {
  }

  result = sqlite3_finalize(stmt);
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
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
    const char *query, uint64_t sensor_id, std::time_t time,
    uint16_t uint16_value, const uint16_t *nullable_uint16_value) {
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result = sqlite3_bind_int64(stmt, 1, sensor_id);
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int64(stmt, 2, static_cast<int64_t>(time));
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int(stmt, 3, uint16_value);
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
    goto error;
  }
  if (nullable_uint16_value) {
    result = sqlite3_bind_int(stmt, 4, *nullable_uint16_value);
    if (result != SQLITE_OK) {
      ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
      goto error;
    }
  } else {
    result = sqlite3_bind_null(stmt, 4);
    if (result != SQLITE_OK) {
      ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
      goto error;
    }
  }
  //
  while (sqlite3_step(stmt) != SQLITE_DONE) {
  }

  result = sqlite3_finalize(stmt);
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "sqlite3_finalize() failed. reason: %d.", result);
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
    const char *query, uint64_t sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndFloat callback) {
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result = sqlite3_bind_int64(stmt, 1, sensor_id);
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int(stmt, 2, static_cast<int32_t>(limit));
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
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
    const char *query, uint64_t sensor_id, size_t limit,
    CallbackRowTimeAndUint16AndNullableUint16 callback) {
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result = sqlite3_bind_int64(stmt, 1, sensor_id);
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int(stmt, 2, static_cast<int32_t>(limit));
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
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