// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "SystemPower.hpp"
#include <cmath>
#include <ctime>

//
void SystemPower::init() noexcept {
  M5.Axp.SetCHGCurrent(AXP192::kCHG_280mA);
  // M5.Axp.SetLcdVoltage(3300);
  M5.Axp.SetLcdVoltage(2800);
  M5.Axp.SetLDOVoltage(3, 3300);
}

// バッテリー電圧
std::pair<MilliVoltage, SystemPower::BatteryPercentage>
SystemPower::getBatteryVoltage() noexcept {
  auto voltage = M5.Axp.GetBatVoltage();
  auto percentage = (voltage < 3.2f) ? 0.0f : (voltage - 3.2f) * 100.0f;
  auto clamped = std::round(std::clamp(percentage, 0.0f, 100.0f));
  return {MilliVoltage(static_cast<int32_t>(1000.0f * voltage)), clamped};
}

//
std::pair<MilliAmpere, SystemPower::BatteryCurrentDirection>
SystemPower::getBatteryCurrent() noexcept {
  MilliAmpere current{static_cast<int32_t>(M5.Axp.GetBatCurrent())};
  auto direction = (current < MilliAmpere::zero())
                       ? BatteryCurrentDirection::Discharging
                       : BatteryCurrentDirection::Charging;

  return {std::chrono::abs(current), direction};
}