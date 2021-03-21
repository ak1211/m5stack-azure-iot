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
bool LocalDatabase::insert_temperature(const char *sensor_id, const time_t &at,
                                       double degc) {
  constexpr char query[] = "INSERT INTO"
                           " temperature(sensor_id, at, degc)"
                           " VALUES(:sensor_id, :at, :degc);";
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result =
      sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":sensor_id"),
                        sensor_id, strlen(sensor_id), SQLITE_TRANSIENT);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":at"),
                              static_cast<int64_t>(at));
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_double(
      stmt, sqlite3_bind_parameter_index(stmt, ":degc"), degc);
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

  return true;

error:
  sqlite3_finalize(stmt);
  return false;
}
//
size_t LocalDatabase::take_least_n_temperatures(const char *sensor_id,
                                                size_t count, Temp *output) {
  constexpr char query[] = "SELECT"
                           " sensor_id, at, degc"
                           " FROM temperature"
                           " WHERE sensor_id=:sensor_id"
                           " ORDER BY at DESC"
                           " LIMIT :count;";
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result =
      sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":sensor_id"),
                        sensor_id, strlen(sensor_id), SQLITE_TRANSIENT);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":count"),
                            static_cast<int32_t>(count));
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  {
    size_t n = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      // number 0 is sensor_id
      int64_t at = sqlite3_column_int64(stmt, 1);
      double degc = sqlite3_column_double(stmt, 2);
      output[n].at = static_cast<time_t>(at);
      output[n].degc = static_cast<float>(degc);
      n = n + 1;
    }
    sqlite3_finalize(stmt);

    return n;
  }

error:
  sqlite3_finalize(stmt);
  return 0;
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
bool LocalDatabase::insert_relative_humidity(const char *sensor_id,
                                             const time_t &at, double rh) {
  constexpr char query[] = "INSERT INTO"
                           " relative_humidity(sensor_id, at, rh)"
                           " VALUES(:sensor_id, :at, :rh);";
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result =
      sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":sensor_id"),
                        sensor_id, strlen(sensor_id), SQLITE_TRANSIENT);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":at"),
                              static_cast<int64_t>(at));
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result =
      sqlite3_bind_double(stmt, sqlite3_bind_parameter_index(stmt, ":rh"), rh);
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

  return true;

error:
  sqlite3_finalize(stmt);
  return false;
}
//
size_t LocalDatabase::take_least_n_relative_humidities(const char *sensor_id,
                                                       size_t count,
                                                       Humi *output) {
  constexpr char query[] = "SELECT"
                           " sensor_id, at, rh"
                           " FROM relative_humidity"
                           " WHERE sensor_id=:sensor_id"
                           " ORDER BY at DESC"
                           " LIMIT :count;";
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result =
      sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":sensor_id"),
                        sensor_id, strlen(sensor_id), SQLITE_TRANSIENT);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":count"),
                            static_cast<int32_t>(count));
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  {
    size_t n = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      // number 0 is sensor_id
      int64_t at = sqlite3_column_int64(stmt, 1);
      double rh = sqlite3_column_double(stmt, 2);
      output[n].at = static_cast<time_t>(at);
      output[n].rh = static_cast<float>(rh);
      n = n + 1;
    }
    sqlite3_finalize(stmt);

    return n;
  }

error:
  sqlite3_finalize(stmt);
  return 0;
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
bool LocalDatabase::insert_pressure(const char *sensor_id, const time_t &at,
                                    double hpa) {
  constexpr char query[] = "INSERT INTO"
                           " pressure(sensor_id, at, hpa)"
                           " VALUES(:sensor_id, :at, :hpa);";
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result =
      sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":sensor_id"),
                        sensor_id, strlen(sensor_id), SQLITE_TRANSIENT);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":at"),
                              static_cast<int64_t>(at));
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_double(stmt, sqlite3_bind_parameter_index(stmt, ":hpa"),
                               hpa);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  while (sqlite3_step(stmt) != SQLITE_DONE) {
  }

  result = sqlite3_finalize(stmt);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "sqlite3_finalize() failed. reason: %d.", result);
  }

  return true;

error:
  sqlite3_finalize(stmt);
  return false;
}
//
size_t LocalDatabase::take_least_n_pressure(const char *sensor_id, size_t count,
                                            Pres *output) {
  constexpr char query[] = "SELECT"
                           " sensor_id, at, hpa"
                           " FROM pressure"
                           " WHERE sensor_id=:sensor_id"
                           " ORDER BY at DESC"
                           " LIMIT :count;";
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result =
      sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":sensor_id"),
                        sensor_id, strlen(sensor_id), SQLITE_TRANSIENT);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":count"),
                            static_cast<int32_t>(count));
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  {
    size_t n = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      // number 0 is sensor_id
      int64_t at = sqlite3_column_int64(stmt, 1);
      double hpa = sqlite3_column_double(stmt, 2);
      output[n].at = static_cast<time_t>(at);
      output[n].hpa = static_cast<float>(hpa);
      n = n + 1;
    }
    sqlite3_finalize(stmt);

    return n;
  }

error:
  sqlite3_finalize(stmt);
  return 0;
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
bool LocalDatabase::insert_carbon_dioxide(const char *sensor_id,
                                          const time_t &at, uint16_t ppm,
                                          const uint16_t *baseline) {
  constexpr char query[] = "INSERT INTO"
                           " carbon_dioxide(sensor_id, at, ppm, baseline)"
                           " VALUES(:sensor_id, :at, :ppm, :baseline);";
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result =
      sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":sensor_id"),
                        sensor_id, strlen(sensor_id), SQLITE_TRANSIENT);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":at"),
                              static_cast<int64_t>(at));
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result =
      sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":ppm"), ppm);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  if (baseline) {
    result = sqlite3_bind_int(
        stmt, sqlite3_bind_parameter_index(stmt, ":baseline"), *baseline);
    if (result != SQLITE_OK) {
      ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
      goto error;
    }
  } else {
    result = sqlite3_bind_null(stmt,
                               sqlite3_bind_parameter_index(stmt, ":baseline"));
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

  return true;

error:
  sqlite3_finalize(stmt);
  return false;
}
//
size_t LocalDatabase::take_least_n_carbon_deoxide(const char *sensor_id,
                                                  size_t count, Co2 *output) {
  constexpr char query[] = "SELECT"
                           " sensor_id, at, ppm, baseline"
                           " FROM carbon_dioxide"
                           " WHERE sensor_id=:sensor_id"
                           " ORDER BY at DESC"
                           " LIMIT :count;";
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result =
      sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":sensor_id"),
                        sensor_id, strlen(sensor_id), SQLITE_TRANSIENT);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":count"),
                            static_cast<int32_t>(count));
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  {
    size_t n = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      // number 0 is sensor_id
      int64_t at = sqlite3_column_int64(stmt, 1);
      int ppm = sqlite3_column_int(stmt, 2);
      output[n].at = static_cast<time_t>(at);
      output[n].ppm = static_cast<uint16_t>(ppm);
      if (sqlite3_column_type(stmt, 3) == SQLITE_NULL) {
        output[n].baseline = 0;
        output[n].has_baseline = false;
      } else {
        int baseline = sqlite3_column_int(stmt, 3);
        output[n].baseline = static_cast<uint16_t>(baseline);
        output[n].has_baseline = true;
      }
      n = n + 1;
    }
    sqlite3_finalize(stmt);

    return n;
  }

error:
  sqlite3_finalize(stmt);
  return 0;
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
bool LocalDatabase::insert_total_voc(const char *sensor_id, const time_t &at,
                                     uint16_t ppb, const uint16_t *baseline) {
  constexpr char query[] = "INSERT INTO"
                           " total_voc(sensor_id, at, ppb, baseline)"
                           " VALUES(:sensor_id, :at, :ppb, :baseline);";
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result =
      sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":sensor_id"),
                        sensor_id, strlen(sensor_id), SQLITE_TRANSIENT);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int64(stmt, sqlite3_bind_parameter_index(stmt, ":at"),
                              static_cast<int64_t>(at));
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result =
      sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":ppb"), ppb);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  if (baseline) {
    result = sqlite3_bind_int(
        stmt, sqlite3_bind_parameter_index(stmt, ":baseline"), *baseline);
    if (result != SQLITE_OK) {
      ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
      goto error;
    }
  } else {
    result = sqlite3_bind_null(stmt,
                               sqlite3_bind_parameter_index(stmt, ":baseline"));
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

  return true;

error:
  sqlite3_finalize(stmt);
  return false;
}
//
size_t LocalDatabase::take_least_n_total_voc(const char *sensor_id,
                                             size_t count, TVOC *output) {
  constexpr char query[] = "SELECT"
                           " sensor_id, at, ppb, baseline"
                           " FROM total_voc"
                           " WHERE sensor_id=:sensor_id"
                           " ORDER BY at DESC"
                           " LIMIT :count;";
  sqlite3_stmt *stmt = nullptr;
  int result;

  result = sqlite3_prepare_v2(database, query, -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  result =
      sqlite3_bind_text(stmt, sqlite3_bind_parameter_index(stmt, ":sensor_id"),
                        sensor_id, strlen(sensor_id), SQLITE_TRANSIENT);
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  result = sqlite3_bind_int(stmt, sqlite3_bind_parameter_index(stmt, ":count"),
                            static_cast<int32_t>(count));
  if (result != SQLITE_OK) {
    ESP_LOGE(F("main"), "%s", sqlite3_errmsg(database));
    goto error;
  }
  //
  {
    size_t n = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      // number 0 is sensor_id
      int64_t at = sqlite3_column_int64(stmt, 1);
      int ppb = sqlite3_column_int(stmt, 2);
      output[n].at = static_cast<time_t>(at);
      output[n].ppb = static_cast<uint16_t>(ppb);
      if (sqlite3_column_type(stmt, 3) == SQLITE_NULL) {
        output[n].baseline = 0;
        output[n].has_baseline = false;
      } else {
        int baseline = sqlite3_column_int(stmt, 3);
        output[n].baseline = static_cast<uint16_t>(baseline);
        output[n].has_baseline = true;
      }
      n = n + 1;
    }
    sqlite3_finalize(stmt);

    return n;
  }

error:
  sqlite3_finalize(stmt);
  return 0;
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
  bool t = insert_temperature(bme.sensor_id, bme.at, bme.temperature);
  bool p =
      insert_relative_humidity(bme.sensor_id, bme.at, bme.relative_humidity);
  bool h = insert_pressure(bme.sensor_id, bme.at, bme.pressure);
  if (t && p && h) {
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
  bool t =
      insert_total_voc(sgp.sensor_id, sgp.at, sgp.tvoc, &sgp.tvoc_baseline);
  bool c = insert_carbon_dioxide(sgp.sensor_id, sgp.at, sgp.eCo2,
                                 &sgp.eCo2_baseline);
  if (t && c) {
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
  bool t = insert_temperature(scd.sensor_id, scd.at, scd.temperature);
  bool p =
      insert_relative_humidity(scd.sensor_id, scd.at, scd.relative_humidity);
  bool c = insert_carbon_dioxide(scd.sensor_id, scd.at, scd.co2, nullptr);
  if (t && p && c) {
    ESP_LOGI("main", "storeTheMeasurements(SCD30) is success.");
    return true;
  } else {
    return false;
  }
}
