// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "local_database.hpp"
#include <Arduino.h>
#include <tuple>

constexpr static const char *TAG = "DbModule";

//
LocalDatabase::LocalDatabase(const std::string &filename)
    : rawid_temperature{-1},
      rawid_relative_humidity{-1},
      rawid_pressure{-1},
      rawid_carbon_dioxide{-1},
      rawid_total_voc{-1},
      _available{false},
      sqlite3_filename{filename},
      database{nullptr} {}

//
//
//
static const char schema_temperature[] =
    "CREATE TABLE IF NOT EXISTS temperature"
    "(id INTEGER PRIMARY KEY AUTOINCREMENT"
    ",sensor_id INTEGER NOT NULL"
    ",at INTEGER NOT NULL"
    ",degc REAL NOT NULL"
    ");";
//
int64_t LocalDatabase::insert_temperature(uint64_t sensor_id, std::time_t at,
                                          DegC degc) {
  static const char query[] = "INSERT INTO"
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
  static const char query[] = "SELECT"
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
static const char schema_relative_humidity[] =
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
static const char schema_pressure[] = "CREATE TABLE IF NOT EXISTS pressure"
                                      "(id INTEGER PRIMARY KEY AUTOINCREMENT"
                                      ",sensor_id INTEGER NOT NULL"
                                      ",at INTEGER NOT NULL"
                                      ",hpa REAL NOT NULL"
                                      ");";
//
int64_t LocalDatabase::insert_pressure(uint64_t sensor_id, std::time_t at,
                                       HPa hpa) {
  static const char query[] = "INSERT INTO"
                              " pressure(sensor_id,at,hpa)"
                              " VALUES(?,?,?);";
  rawid_pressure = raw_insert_time_and_float(query, sensor_id, at, hpa.value);
  return rawid_pressure;
}
//
size_t LocalDatabase::get_pressures_desc(
    uint64_t sensor_id, size_t limit,
    LocalDatabase::CallbackRowTimeAndFloat callback) {
  static const char query[] = "SELECT"
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
static char schema_carbon_dioxide[] =
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
  static const char query[] = "INSERT INTO"
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
  static const char query[] = "SELECT"
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
static const char schema_total_voc[] = "CREATE TABLE IF NOT EXISTS total_voc"
                                       "(id INTEGER PRIMARY KEY AUTOINCREMENT"
                                       ",sensor_id INTEGER NOT NULL"
                                       ",at INTEGER NOT NULL"
                                       ",ppb REAL NOT NULL"
                                       ",baseline INTEGER"
                                       ");";
//
int64_t LocalDatabase::insert_total_voc(uint64_t sensor_id, std::time_t at,
                                        Ppb ppb, const uint16_t *baseline) {
  static const char query[] = "INSERT INTO"
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
  static const char query[] = "SELECT"
                              " sensor_id,at,ppb,baseline"
                              " FROM total_voc"
                              " WHERE sensor_id=?"
                              " ORDER BY at DESC"
                              " LIMIT ?;";
  return raw_get_n_time_and_uint16_and_nullable_uint16(query, sensor_id, limit,
                                                       callback);
}
//
std::tuple<bool, std::time_t, BaselineECo2>
LocalDatabase::get_latest_baseline_eco2(uint64_t sensor_id) {
  static const char query[] = "SELECT"
                              " sensor_id" // 0
                              ",at"        // 1
                              ",baseline"  // 2
                              " FROM carbon_dioxide"
                              " WHERE sensor_id=?"
                              " AND baseline NOTNULL"
                              " ORDER BY at DESC"
                              " LIMIT 1;";
  auto result = raw_get_latest_baseline(query, sensor_id);
  return std::make_tuple(std::get<0>(result), std::get<1>(result),
                         BaselineECo2(std::get<2>(result)));
}
//
std::tuple<bool, std::time_t, BaselineTotalVoc>
LocalDatabase::get_latest_baseline_total_voc(uint64_t sensor_id) {
  static const char query[] = "SELECT"
                              " sensor_id" // 0
                              ",at"        // 1
                              ",baseline"  // 2
                              " FROM total_voc"
                              " WHERE sensor_id=?"
                              " AND baseline NOTNULL"
                              " ORDER BY at DESC"
                              " LIMIT 1;";
  auto result = raw_get_latest_baseline(query, sensor_id);
  return std::make_tuple(std::get<0>(result), std::get<1>(result),
                         BaselineTotalVoc(std::get<2>(result)));
}

//
//
//
bool LocalDatabase::begin() {
  char *error_msg = nullptr;
  int result;

  result = sqlite3_initialize();
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result = sqlite3_open_v2(sqlite3_filename.c_str(), &database,
                           SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
    goto error;
  }
  // temperature
  result =
      sqlite3_exec(database, schema_temperature, nullptr, nullptr, &error_msg);
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

  // succsessfully exit
  _available = true;
  return _available;

error:
  _available = false;
  return _available;
}

//
//
//
bool LocalDatabase::insert(const TempHumiPres &temp_humi_pres) {
  if (!available()) {
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
  if (!available()) {
    return false;
  }
  int64_t t;
  if (tvoc_eco2.tvoc_baseline.nothing()) {
    t = insert_total_voc(tvoc_eco2.sensor_descriptor.id, tvoc_eco2.at,
                         tvoc_eco2.tvoc.value, nullptr);
  } else {
    BaselineTotalVoc tvoc_base = tvoc_eco2.tvoc_baseline.get();
    t = insert_total_voc(tvoc_eco2.sensor_descriptor.id, tvoc_eco2.at,
                         tvoc_eco2.tvoc.value, &tvoc_base.value);
  }

  int64_t c;
  if (tvoc_eco2.eCo2_baseline.nothing()) {
    c = insert_carbon_dioxide(tvoc_eco2.sensor_descriptor.id, tvoc_eco2.at,
                              tvoc_eco2.eCo2.value, nullptr);

  } else {
    BaselineECo2 eco2_base = tvoc_eco2.eCo2_baseline.get();
    c = insert_carbon_dioxide(tvoc_eco2.sensor_descriptor.id, tvoc_eco2.at,
                              tvoc_eco2.eCo2.value, &eco2_base.value);
  }
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
  if (!available()) {
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
    stmt = nullptr; // omit double finalize
    goto error;
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
    ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
    stmt = nullptr; // omit double finalize
    goto error;
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

    result = sqlite3_finalize(stmt);
    if (result != SQLITE_OK) {
      ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
      stmt = nullptr; // omit double finalize
      goto error;
    }

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

    result = sqlite3_finalize(stmt);
    if (result != SQLITE_OK) {
      ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
      stmt = nullptr; // omit double finalize
      goto error;
    }

    return counter;
  }

error:
  sqlite3_finalize(stmt);
  return 0;
}

//
//
//
std::tuple<bool, std::time_t, BaselineSGP30T>
LocalDatabase::raw_get_latest_baseline(const char *query, uint64_t sensor_id) {
  sqlite3_stmt *stmt = nullptr;
  int result;
  auto retval{std::make_tuple(false, 0, 0)};

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
  //
  {
    size_t counter = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      // number 0 is sensor_id
      int64_t at = sqlite3_column_int64(stmt, 1);
      if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
        uint16_t baseline = sqlite3_column_int(stmt, 2);
        retval = std::make_tuple(true, static_cast<std::time_t>(at),
                                 static_cast<BaselineSGP30T>(baseline));
      }
      counter++;
    }
    result = sqlite3_finalize(stmt);
    if (result != SQLITE_OK) {
      ESP_LOGE(TAG, "%s", sqlite3_errmsg(database));
      stmt = nullptr; // omit double finalize
      goto error;
    }

    return retval;
  }

error:
  sqlite3_finalize(stmt);
  return retval;
}
