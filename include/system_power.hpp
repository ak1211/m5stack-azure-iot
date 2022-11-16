// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef SYSTEM_POWER_HPP
#define SYSTEM_POWER_HPP

#include "value_types.hpp"
#include <chrono>
#include <ctime>

class SystemPower {
public:
  constexpr static std::chrono::seconds UPDATE_INTERVAL{10};
  //  = constexpr static std::clock_t UPDATE_INTERVAL = 10 * CLOCKS_PER_SEC; //
  //  10 seconds
  SystemPower() : update_at{std::chrono::system_clock::now()} {}
  bool begin();
  inline bool needToUpdate() {
    auto elapsed = std::chrono::system_clock::now() - update_at;
    return (elapsed >= UPDATE_INTERVAL);
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
  std::chrono::time_point<std::chrono::system_clock> update_at;
  Voltage battery_voltage;
  float battery_percentage;
  Ampere battery_current;
};

#endif // SYSTEM_POWER_HPP
