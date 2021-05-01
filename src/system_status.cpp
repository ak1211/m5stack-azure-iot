// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "system_status.hpp"

#include <M5Core2.h>
#include <SD.h>
#include <algorithm>
#include <ctime>

#undef max
#undef min

//
//
//
System::Uptime System::uptime(const Status &status) {
  clock_t time_epoch = clock() - status.startup_epoch;
  uint32_t seconds = static_cast<uint32_t>(time_epoch / CLOCKS_PER_SEC);
  uint32_t minutes = seconds / 60;
  uint32_t hours = minutes / 60;
  uint32_t days = hours / 24;
  return {
      .days = static_cast<uint16_t>(days),
      .hours = static_cast<uint8_t>(hours % 24),
      .minutes = static_cast<uint8_t>(minutes % 60),
      .seconds = static_cast<uint8_t>(seconds % 60),
  };
}

//
//
//
System::BatteryStatus System::getBatteryStatus() {
  float batt_v = M5.Axp.GetBatVoltage();
  float batt_full_v = 4.18f;
  float shutdown_v = 3.30f;
  float indicated_v = batt_v - shutdown_v;
  float fullscale_v = batt_full_v - shutdown_v;
  float batt_percentage = 100.00f * indicated_v / fullscale_v;
  float batt_current = M5.Axp.GetBatCurrent(); // mA
  return {
      .voltage = batt_v,
      .percentage = std::max(0.0f, std::min(batt_percentage, 100.0f)),
      .current = batt_current,
  };
}
