// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef LOCAL_DATABASE_HPP
#define LOCAL_DATABASE_HPP

#include "bme280_sensor.hpp"
#include "scd30_sensor.hpp"
#include "sgp30_sensor.hpp"

#include <cstddef>
#include <cstring>
#include <sqlite3.h>

//
//
//
class LocalDatabase {
public:
  static constexpr size_t FILENAME_MAX_LEN = 50;
  //
  struct Temp {
    time_t at;
    float degc;
  };
  //
  struct Humi {
    time_t at;
    float rh;
  };
  //
  struct Pres {
    time_t at;
    float hpa;
  };
  //
  struct Co2 {
    time_t at;
    uint16_t ppm;
    uint16_t baseline;
    bool has_baseline;
  };
  //
  struct TVOC {
    time_t at;
    uint16_t ppb;
    uint16_t baseline;
    bool has_baseline;
  };
  //
  LocalDatabase(const char *filename) : sqlite3_filename("") {
    memset(const_cast<char *>(sqlite3_filename), 0, FILENAME_MAX_LEN + 1);
    strncpy(const_cast<char *>(sqlite3_filename), filename, FILENAME_MAX_LEN);
    database = nullptr;
  };
  //
  bool healthy() { return database != nullptr ? true : false; }
  //
  bool beginDb();
  //
  bool insert(const Bme280::TempHumiPres &bme);
  //
  bool insert(const Sgp30::TvocEco2 &sgp);
  //
  bool insert(const Scd30::Co2TempHumi &scd);
  //
  size_t take_least_n_temperatures(const char *sensor_id, size_t count,
                                   Temp *output);
  //
  size_t take_least_n_relative_humidities(const char *sensor_id, size_t count,
                                          Humi *output);
  //
  size_t take_least_n_pressure(const char *sensor_id, size_t count,
                               Pres *output);
  //
  size_t take_least_n_carbon_deoxide(const char *sensor_id, size_t count,
                                     Co2 *output);
  //
  size_t take_least_n_total_voc(const char *sensor_id, size_t count,
                                TVOC *output);
  //
  void printToSerial(Temp t) {
    // time zone offset UTC+9 = asia/tokyo
    time_t local_time = t.at + 9 * 60 * 60;
    struct tm local;
    gmtime_r(&local_time, &local);
    char buffer[50];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S+09:00", &local);
    Serial.printf("%s, %f[C]", buffer, t.degc);
    Serial.println("");
  }
  //
  void printToSerial(Humi h) {
    // time zone offset UTC+9 = asia/tokyo
    time_t local_time = h.at + 9 * 60 * 60;
    struct tm local;
    gmtime_r(&local_time, &local);
    char buffer[50];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S+09:00", &local);
    Serial.printf("%s, %f[%%]", buffer, h.rh);
    Serial.println("");
  }
  //
  void printToSerial(Pres p) {
    // time zone offset UTC+9 = asia/tokyo
    time_t local_time = p.at + 9 * 60 * 60;
    struct tm local;
    gmtime_r(&local_time, &local);
    char buffer[50];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S+09:00", &local);
    Serial.printf("%s, %f[hpa]", buffer, p.hpa);
    Serial.println("");
  }
  //
  void printToSerial(Co2 c) {
    // time zone offset UTC+9 = asia/tokyo
    time_t local_time = c.at + 9 * 60 * 60;
    struct tm local;
    gmtime_r(&local_time, &local);
    char buffer[50] = "";
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S+09:00", &local);
    if (c.has_baseline) {
      Serial.printf("%s, %d[ppm], %d[baseline]", buffer, c.ppm, c.baseline);
    } else {
      Serial.printf("%s, %d[ppm]", buffer, c.ppm);
    }
    Serial.println("");
  }
  //
  void printToSerial(TVOC t) {
    // time zone offset UTC+9 = asia/tokyo
    time_t local_time = t.at + 9 * 60 * 60;
    struct tm local;
    gmtime_r(&local_time, &local);
    char buffer[50] = "";
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S+09:00", &local);
    if (t.has_baseline) {
      Serial.printf("%s, %d[ppb], %d[baseline]", buffer, t.ppb, t.baseline);
    } else {
      Serial.printf("%s, %d[ppb]", buffer, t.ppb);
    }
    Serial.println("");
  }

private:
  const char sqlite3_filename[FILENAME_MAX_LEN + 1];
  sqlite3 *database;
  //
  bool insert_temperature(const char *sensor_id, const time_t &at, double degc);
  //
  bool insert_relative_humidity(const char *sensor_id, const time_t &at,
                                double rh);
  //
  bool insert_pressure(const char *sensor_id, const time_t &at, double hpa);
  //
  bool insert_carbon_dioxide(const char *sensor_id, const time_t &at,
                             uint16_t ppm, const uint16_t *baseline);
  //
  bool insert_total_voc(const char *sensor_id, const time_t &at, uint16_t ppb,
                        const uint16_t *baseline);
};

#endif // LOCAL_DATABASE_HPP
