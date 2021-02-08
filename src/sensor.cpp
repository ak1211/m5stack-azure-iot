// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include <M5Core2.h>
#include "sensor.hpp"

//
//
//
TempHumiPres *Sensor::begin(uint8_t i2c_address)
{
  for (int8_t challenge = 1; challenge <= 50; challenge++)
  {
    if (bme280.begin(i2c_address))
    {
      bme280_temperature = bme280.getTemperatureSensor();
      bme280_pressure = bme280.getPressureSensor();
      bme280_humidity = bme280.getHumiditySensor();
      break;
    }
    else
    {
      ESP_LOGI("main", "%dth challenge failed... try again.", challenge);
      delay(50);
    }
  }
  return sensing();
}

//
//
//
TempHumiPres *Sensor::sensing()
{
  if (!healthy())
    return NULL;
  //
  sensors_event_t temperature_event;
  sensors_event_t humidity_event;
  sensors_event_t pressure_event;
  //
  bme280_temperature->getEvent(&temperature_event);
  bme280_humidity->getEvent(&humidity_event);
  bme280_pressure->getEvent(&pressure_event);

  latest.sensor_id = sensor_id;
  latest.temperature = temperature_event.temperature;
  latest.relative_humidity = humidity_event.relative_humidity;
  latest.pressure = pressure_event.pressure;
  return &latest;
}
