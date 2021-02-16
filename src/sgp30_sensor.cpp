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
  latest.tvoc = 0;
  latest.eCo2 = 0;
  smoothed.tvoc = 0;
  smoothed.eCo2 = 0;
  periods_index = 0;

  if (!sgp30.begin())
    return nullptr;

  sgp30_healthy = true;
  //
  time_t tm_now;
  time(&tm_now);
  TvocEco2 *result = sensing(tm_now);
  if (result)
  {
    for (int16_t n = 0; n < MOVING_AVERAGES_PERIOD; n++)
    {
      periods[n].tvoc = result->tvoc;
      periods[n].eCo2 = result->eCo2;
    }
    smoothed.tvoc = result->tvoc;
    smoothed.eCo2 = result->eCo2;
  }
  return result;
}

//
//
//
TvocEco2 *Sensor::sensing(const time_t &measured_at)
{
  if (!healthy())
    return nullptr;
  //
  if (!sgp30.IAQmeasure())
  {
    sgp30_healthy = false;
    return nullptr;
  }

  uint16_t eco2_base;
  uint16_t tvoc_base;
  if (!sgp30.getIAQBaseline(&eco2_base, &tvoc_base))
  {
    sgp30_healthy = false;
    return nullptr;
  }

  latest.sensor_id = smoothed.sensor_id = sensor_id;
  latest.at = smoothed.at = measured_at;
  latest.tvoc = sgp30.TVOC;
  latest.eCo2 = sgp30.eCO2;
  latest.tvoc_baseline = smoothed.tvoc_baseline = eco2_base;
  latest.eCo2_baseline = smoothed.eCo2_baseline = tvoc_base;

  // smoothing
  periods[periods_index].tvoc = latest.tvoc;
  periods[periods_index].eCo2 = latest.eCo2;
  // periods_index = (periods_index + 1) % MOVING_AVERAGES_PERIOD
  periods_index = (periods_index + 1) & (MOVING_AVERAGES_PERIOD - 1);

  uint32_t accum_tvoc = periods[0].tvoc;
  uint32_t accum_eCo2 = periods[0].eCo2;
  for (int16_t n = 1; n < MOVING_AVERAGES_PERIOD; n++)
  {
    accum_tvoc += periods[n].tvoc;
    accum_eCo2 += periods[n].eCo2;
  }

  smoothed.tvoc = accum_tvoc >> MOVING_AVERAGES_PERIOD_AS_2N;
  smoothed.eCo2 = accum_eCo2 >> MOVING_AVERAGES_PERIOD_AS_2N;

  return &latest;
}
