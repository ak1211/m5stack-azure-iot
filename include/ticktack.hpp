// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef TICKTACK_HPP
#define TICKTACK_HPP

#include <cstdint>
#include <ctime>

//
//
//
class TickTack {
public:
  TickTack() { startup_epoch = clock(); }
  //
  uint32_t uptime_seconds() {
    clock_t time_epoch = clock() - startup_epoch;
    uint32_t seconds = static_cast<uint32_t>(time_epoch / CLOCKS_PER_SEC);
    return seconds;
  }
  //
  struct Uptime {
    uint16_t days;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
  };
  Uptime uptime() {
    clock_t time_epoch = clock() - startup_epoch;
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

private:
  clock_t startup_epoch;
};

#endif // TICKTACK_HPP
