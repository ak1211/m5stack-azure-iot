// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef POWER_STATUS_HPP
#define POWER_STATUS_HPP

#include "value_types.hpp"
#include <ctime>

class PowerStatus {
public:
  constexpr static clock_t UPDATE_INTERVAL = 10 * CLOCKS_PER_SEC; // 10 seconds
  PowerStatus() : update_at{0} {}
  inline bool needToUpdate() {
    return (clock() - update_at >= UPDATE_INTERVAL);
  }
  inline Voltage getBatteryVoltage() { return battery_voltage; }
  inline float getBatteryPercentage() { return battery_percentage; }
  Ampere getBatteryChargingCurrent();
  inline bool isBatteryCharging() { return (battery_current.value > 0.00f); }
  inline bool isBatteryDischarging() { return (battery_current.value < 0.00f); }
  void update();

private:
  clock_t update_at;
  Voltage battery_voltage;
  float battery_percentage;
  Ampere battery_current;
};

#endif // POWER_STATUS_HPP
