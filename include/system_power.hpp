// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once

#include "value_types.hpp"
#include <M5Core2.h>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <tuple>

#undef max
#undef min

//
//
//
class SystemPower {
  std::chrono::system_clock::time_point update_at;
  MilliVoltage battery_voltage;
  MilliAmpere battery_current;
  float battery_percentage;

public:
  constexpr static auto UPDATE_INTERVAL = std::chrono::seconds{10};
  // 外部電源 / バッテリ―電源
  enum class PowerNow { External, Internal };
  // 充電中 / バランス / 放電中
  enum class BatteryNow { Charging, Balanced, Discharging };
  //
  SystemPower() : update_at{std::chrono::system_clock::now()} {}
  //
  bool begin() {
    M5.Axp.SetCHGCurrent(AXP192::kCHG_100mA);
    M5.Axp.SetLcdVoltage(3300);
    M5.Axp.SetLDOVoltage(3, 3300);
    return true;
  }
  //
  inline bool needToUpdate() {
    auto elapsed = std::chrono::system_clock::now() - update_at;
    return (elapsed >= UPDATE_INTERVAL);
  }
  //
  PowerNow power_now() {
    return (M5.Axp.isACIN()) ? PowerNow::External : PowerNow::Internal;
  }
  //
  inline MilliVoltage getBatteryVoltage() { return battery_voltage; }
  //
  inline float getBatteryPercentage() { return battery_percentage; }
  //
  inline std::pair<BatteryNow, MilliAmpere> getBatteryCurrent() {
    if (battery_current > MilliAmpere::zero()) {
      return {BatteryNow::Charging, std::chrono::abs(battery_current)};
    } else if (battery_current < MilliAmpere::zero()) {
      return {BatteryNow::Discharging, std::chrono::abs(battery_current)};
    } else {
      return {BatteryNow::Balanced, MilliAmpere::zero()};
    }
  }
  //
  void update() {
    float batVoltage = M5.Axp.GetBatVoltage();
    float batPercentage = (batVoltage < 3.2) ? 0 : (batVoltage - 3.2) * 100;
    float batt_current = M5.Axp.GetBatCurrent(); // mA
    battery_voltage = MilliVoltage(static_cast<int32_t>(1000.0f * batVoltage));
    battery_percentage = std::min(100.0f, batPercentage);
    battery_current = MilliAmpere(static_cast<int32_t>(batt_current));
    update_at = std::chrono::system_clock::now();
  }
};
