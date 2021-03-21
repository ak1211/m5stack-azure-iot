// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef BME280_SENSOR_HPP
#define BME280_SENSOR_HPP

#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <cstdint>
#include <ctime>

namespace Bme280 {
//
//
//
struct TempHumiPres {
  const char *sensor_id;
  time_t at;
  float temperature;       // degree celsius
  float relative_humidity; // %
  float pressure;          // hectopascal
};

//
//
//
class Sensor {
public:
  static const size_t SENSOR_ID_MAX_LEN = 50;
  Sensor(const char *custom_sensor_id) : sensor_id("") {
    memset(const_cast<char *>(sensor_id), 0, SENSOR_ID_MAX_LEN + 1);
    strncpy(const_cast<char *>(sensor_id), custom_sensor_id, SENSOR_ID_MAX_LEN);
    bme280_temperature = nullptr;
    bme280_pressure = nullptr;
    bme280_humidity = nullptr;
  }
  //
  bool begin(uint8_t i2c_address = 0x76);
  //
  TempHumiPres *sensing(const time_t &measured_at);
  //
  TempHumiPres *getLatestTempHumiPres() {
    return (healthy()) ? &latest : nullptr;
  }
  //
  void printSensorDetails() {
    if (!healthy()) {
      ESP_LOGE("main", "sensor has problems.");
      return;
    }
    //
    bme280_temperature->printSensorDetails();
    bme280_pressure->printSensorDetails();
    bme280_humidity->printSensorDetails();
  }
  //
  bool healthy() {
    return (bme280_temperature && bme280_pressure && bme280_humidity) ? true
                                                                      : false;
  }

private:
  Adafruit_BME280 bme280;
  Adafruit_Sensor *bme280_temperature;
  Adafruit_Sensor *bme280_pressure;
  Adafruit_Sensor *bme280_humidity;
  //
  const char sensor_id[SENSOR_ID_MAX_LEN + 1];
  struct TempHumiPres latest;
};
}; // namespace Bme280

#endif // BME280_SENSOR_HPP
