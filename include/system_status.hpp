// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef SYSTEM_STATUS_HPP
#define SYSTEM_STATUS_HPP

#include <cstdint>
#include <ctime>

namespace System {
//
//
//
struct Status {
  clock_t startup_epoch;
  bool has_WIFI_connection;
  bool is_freestanding_mode;
};

//
//
//
struct Uptime {
  uint16_t days;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
};

//
struct BatteryStatus {
  float voltage;
  float percentage;
  float current;
};

inline bool isBatteryCharging(const BatteryStatus &stat) {
  return (stat.current > 0.00f);
}
//
inline bool isBatteryDischarging(const BatteryStatus &stat) {
  return (stat.current < 0.00f);
}

//
//
//
extern Uptime uptime(const Status &status);

//
//
//
extern BatteryStatus getBatteryStatus();

}; // namespace System

#endif // SYSTEM_STATUS_HPP
