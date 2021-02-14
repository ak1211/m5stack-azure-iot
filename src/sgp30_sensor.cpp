// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include <M5Core2.h>
#include "sgp30_sensor.hpp"

using namespace Sgp30;

//
//
//
TvocEco2 *Sensor::begin()
{
  for (int8_t challenge = 1; challenge <= 50; challenge++)
  {
    if (sgp30.begin())
    {
      sgp30_healthy = true;
      break;
    }
    else
    {
      ESP_LOGI("main", "%dth challenge failed... try again.", challenge);
      delay(50);
    }
  }
  time_t tm_now;
  time(&tm_now);
  return sensing(tm_now);
}

//
//
//
TvocEco2 *Sensor::sensing(const time_t &measured_at)
{
  if (!healthy())
    return NULL;
  //
  if (!sgp30.IAQmeasure())
  {
    sgp30_healthy = false;
    return NULL;
  }

  uint16_t eco2_base;
  uint16_t tvoc_base;
  if (!sgp30.getIAQBaseline(&eco2_base, &tvoc_base))
  {
    sgp30_healthy = false;
    return NULL;
  }

  latest.sensor_id = sensor_id;
  latest.at = measured_at;
  latest.tvoc = sgp30.TVOC;
  latest.eCo2 = sgp30.eCO2;
  latest.tvoc_baseline = eco2_base;
  latest.eCo2_baseline = tvoc_base;
  return &latest;
}
