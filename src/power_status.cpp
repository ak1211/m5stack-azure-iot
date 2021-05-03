// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "power_status.hpp"

#include <M5Core2.h>
#include <algorithm>

#undef max
#undef min

Ampere PowerStatus::getBatteryChargingCurrent() {
  return Ampere(abs(battery_current.value));
}

void PowerStatus::update() {
  float batt_v = M5.Axp.GetBatVoltage();
  float batt_full_v = 4.18f;
  float shutdown_v = 3.30f;
  float indicated_v = batt_v - shutdown_v;
  float fullscale_v = batt_full_v - shutdown_v;
  float batt_percentage = 100.00f * indicated_v / fullscale_v;
  float batt_current = M5.Axp.GetBatCurrent(); // mA
  battery_voltage = Voltage(batt_v);
  battery_percentage = std::max(0.0f, std::min(batt_percentage, 100.0f));
  battery_current = Ampere(batt_current / 1000.0f);
  update_at = clock();
}
