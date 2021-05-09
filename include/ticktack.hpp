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
  TickTack() : _available{false}, startup_time{} {}
  //
  bool available() { return _available; }
  //
  bool begin() {
    startup_time = std::time(nullptr);
    _available = true;
    return _available;
  }
  //
  uint64_t uptime_seconds() {
    std::time_t now = std::time(nullptr);
    return static_cast<uint64_t>(difftime(now, startup_time));
  }
  //
  struct Uptime {
    uint32_t days;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
  };
  Uptime uptime() {
    uint64_t elapsed_time_of_seconds = uptime_seconds();
    uint32_t minutes = elapsed_time_of_seconds / 60;
    uint32_t hours = minutes / 60;
    uint32_t days = hours / 24;
    return {
        .days = days,
        .hours = static_cast<uint8_t>(hours % 24),
        .minutes = static_cast<uint8_t>(minutes % 60),
        .seconds = static_cast<uint8_t>(elapsed_time_of_seconds % 60),
    };
  }

private:
  bool _available;
  std::time_t startup_time;
};

#endif // TICKTACK_HPP
