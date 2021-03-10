// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include <M5Core2.h>
#include "scd30_sensor.hpp"

using namespace Scd30;

//
//
//
Co2TempHumi *Sensor::sensing(const time_t &measured_at)
{
  if (!healthy())
    return nullptr;
  //
  if (!scd30.dataAvailable())
  {
    return nullptr;
  }
  latest.sensor_id = smoothed.sensor_id = sensor_id;
  latest.at = smoothed.at = measured_at;
  latest.co2 = scd30.getCO2();
  latest.temperature = scd30.getTemperature();
  latest.relative_humidity = scd30.getHumidity();

  pushRing(latest.co2, latest.temperature, latest.relative_humidity);

  // smoothing
  smoothed.co2 = calculateSmoothing_uint16(co2_ring);
  smoothed.temperature = calculateSmoothing_float(temperature_ring);
  smoothed.relative_humidity = calculateSmoothing_float(humidity_ring);

  return &latest;
}
