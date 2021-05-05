// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "system_power.hpp"

#include <M5Core2.h>
#include <algorithm>

#undef max
#undef min

void SystemPower::begin() {
  M5.Axp.SetCHGCurrent(AXP192::kCHG_100mA);
  M5.Axp.SetLcdVoltage(3300);
  M5.Axp.SetLDOVoltage(3, 3300);
}
SystemPower::PowerNow SystemPower::power_now() {
  return (M5.Axp.isACIN()) ? PowerNow::External : PowerNow::Internal;
}

Ampere SystemPower::getBatteryChargingCurrent() {
  return Ampere(abs(battery_current.value));
}

void SystemPower::update() {
  float batVoltage = M5.Axp.GetBatVoltage();
  float batPercentage = (batVoltage < 3.2) ? 0 : (batVoltage - 3.2) * 100;
  float batt_current = M5.Axp.GetBatCurrent(); // mA
  battery_voltage = Voltage(batVoltage);
  battery_percentage = batPercentage;
  battery_current = Ampere(batt_current / 1000.0f);
  update_at = clock();
}
