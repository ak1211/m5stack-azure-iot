// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "sgp30_sensor.hpp"
#include <M5Core2.h>

using namespace Sgp30;

//
//
//
bool Sensor::begin() {
  if (!sgp30.begin()) {
    sgp30_healthy = false;
    return false;
  }

  sgp30_healthy = true;
  initRing();
  return true;
}

//
//
//
TvocEco2 *Sensor::sensing(const time_t &measured_at) {
  if (!healthy())
    return nullptr;
  //
  if (!sgp30.IAQmeasure()) {
    sgp30_healthy = false;
    return nullptr;
  }

  uint16_t eco2_base;
  uint16_t tvoc_base;
  if (!sgp30.getIAQBaseline(&eco2_base, &tvoc_base)) {
    sgp30_healthy = false;
    return nullptr;
  }

  latest.sensor_id = smoothed.sensor_id = sensor_id;
  latest.at = smoothed.at = measured_at;
  latest.tvoc = sgp30.TVOC;
  latest.eCo2 = sgp30.eCO2;
  latest.tvoc_baseline = smoothed.tvoc_baseline = eco2_base;
  latest.eCo2_baseline = smoothed.eCo2_baseline = tvoc_base;

  pushRing(sgp30.TVOC, sgp30.eCO2);

  // smoothing
  smoothed.tvoc = calculateSmoothing(tvoc_ring);
  smoothed.eCo2 = calculateSmoothing(eCo2_ring);

  return &latest;
}
