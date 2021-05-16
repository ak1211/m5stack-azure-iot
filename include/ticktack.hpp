// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef TICKTACK_HPP
#define TICKTACK_HPP

#include <cstdint>
#include <ctime>
#include <lwip/apps/sntp.h>
#include <tuple>

//
//
//
class TickTack {
public:
  constexpr static const char *TAG = "TickTackModule";
  TickTack() : _available{false}, startup_time{} {}
  //
  bool available() { return _available; }
  //
  bool begin() { return true; }
  //
  bool update() {
    if (_available) {
      return _available;
    }
    if (sntp_enabled()) {
      sync();
    }
    return _available;
  }
  //
  std::time_t time() {
    if (!_available) {
      ESP_LOGE(TAG, "TickTack was not available.");
    }
    return std::time(nullptr);
  }
  //
  static char *isoformatUTC(char *str, std::size_t size, std::time_t utctime) {
    struct tm tm;
    gmtime_r(&utctime, &tm);
    strftime(str, size, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return str;
  }
  //
  static std::string &isoformatUTC(std::string &str, std::time_t utctime) {
    // iso8601 format.
    // "2021-02-11T00:56:00.000+00:00"
    //  12345678901234567890123456789
    char buffer[30];
    str = isoformatUTC(buffer, sizeof(buffer), utctime);
    return str;
  }
  //
  uint64_t uptimeSeconds() {
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
    if (!_available) {
      ESP_LOGE(TAG, "TickTack was not available.");
    }
    uint64_t elapsed_time_of_seconds = uptimeSeconds();
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
  //
  void sync() {
    startup_time = std::time(nullptr);
    _available = true;
    char buf[30];
    ESP_LOGI(TAG, "startup time: \"%s\"", isoformatUTC(buf, 30, startup_time));
  }
};

#endif // TICKTACK_HPP
