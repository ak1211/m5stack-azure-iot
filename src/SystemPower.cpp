// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "SystemPower.hpp"
#include <cmath>
#include <ctime>

//
void SystemPower::init() noexcept {
  M5.Power.Axp192.setChargeCurrent(280);
  M5.Display.setBrightness(200);
  // stop the vibration
  M5.Power.Axp192.setLDO3(0);
}

// バッテリー電圧
MilliVoltage SystemPower::getBatteryVoltage() noexcept {
  auto voltage = M5.Power.Axp192.getBatteryVoltage();
  return MilliVoltage(static_cast<int32_t>(1000.0f * voltage));
}

// バッテリー残量
SystemPower::BatteryLevel SystemPower::getBatteryLevel() noexcept {
  return M5.Power.Axp192.getBatteryLevel();
}

//
std::pair<MilliAmpere, SystemPower::BatteryCurrentDirection>
SystemPower::getBatteryCurrent() noexcept {
  MilliAmpere chargeCurrent{
      static_cast<int32_t>(M5.Power.Axp192.getBatteryChargeCurrent())};
  MilliAmpere dischargeCurrent{
      static_cast<int32_t>(M5.Power.Axp192.getBatteryDischargeCurrent())};

  if (chargeCurrent > dischargeCurrent) {
    return {chargeCurrent, BatteryCurrentDirection::Charging};
  } else {
    return {dischargeCurrent, BatteryCurrentDirection::Discharging};
  }
}