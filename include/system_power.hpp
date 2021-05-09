// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef SYSTEM_POWER_HPP
#define SYSTEM_POWER_HPP

#include "value_types.hpp"
#include <ctime>

class SystemPower {
public:
  constexpr static std::clock_t UPDATE_INTERVAL =
      10 * CLOCKS_PER_SEC; // 10 seconds
  SystemPower() : update_at{0} {}
  bool begin();
  inline bool needToUpdate() {
    return (std::clock() - update_at >= UPDATE_INTERVAL);
  }
  enum class PowerNow { External, Internal };
  PowerNow power_now();
  //
  inline Voltage getBatteryVoltage() { return battery_voltage; }
  inline float getBatteryPercentage() { return battery_percentage; }
  Ampere getBatteryChargingCurrent();
  inline bool isBatteryCharging() { return (battery_current.value > 0.00f); }
  inline bool isBatteryDischarging() { return (battery_current.value < 0.00f); }
  void update();

private:
  std::clock_t update_at;
  Voltage battery_voltage;
  float battery_percentage;
  Ampere battery_current;
};

#endif // SYSTEM_POWER_HPP
