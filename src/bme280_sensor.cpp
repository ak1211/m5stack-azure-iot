// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "bme280_sensor.hpp"
#include <M5Core2.h>

using namespace Bme280;

//
//
//
bool Sensor::begin(uint8_t i2c_address) {
  bme280_temperature = nullptr;
  bme280_pressure = nullptr;
  bme280_humidity = nullptr;
  if (!bme280.begin(i2c_address))
    return false;

  bme280_temperature = bme280.getTemperatureSensor();
  bme280_pressure = bme280.getPressureSensor();
  bme280_humidity = bme280.getHumiditySensor();
  //
  bme280.setSampling(Adafruit_BME280::MODE_FORCED, Adafruit_BME280::SAMPLING_X1,
                     Adafruit_BME280::SAMPLING_X1, Adafruit_BME280::SAMPLING_X1,
                     Adafruit_BME280::FILTER_OFF,
                     Adafruit_BME280::STANDBY_MS_1000);
  return true;
}

//
//
//
TempHumiPres *Sensor::sensing(const time_t &measured_at) {
  if (!healthy())
    return nullptr;

  //
  sensors_event_t temperature_event;
  sensors_event_t humidity_event;
  sensors_event_t pressure_event;
  //
  bme280.takeForcedMeasurement();
  bme280_temperature->getEvent(&temperature_event);
  bme280_humidity->getEvent(&humidity_event);
  bme280_pressure->getEvent(&pressure_event);

  latest.sensor_id = sensor_id;
  latest.at = measured_at;
  latest.temperature = temperature_event.temperature;
  latest.relative_humidity = humidity_event.relative_humidity;
  latest.pressure = pressure_event.pressure;
  return &latest;
}
