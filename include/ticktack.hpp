// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef TICKTACK_HPP
#define TICKTACK_HPP

#include <chrono>
#include <cstdint>
#include <ctime>
#include <esp_sntp.h>
#include <lwip/apps/sntp.h>
#include <tuple>

//
//
//
class TickTack {
public:
  constexpr static const char *TAG = "TickTackModule";
  // time zone = Asia_Tokyo(UTC+9)
  // constexpr static const char *TZ_TIME_ZONE = "JST-9";
  constexpr static const char *TZ_TIME_ZONE = "UTC0";
  //
  TickTack() : _available{false}, startup_time{} {}
  //
  bool available() { return _available; }
  //
  bool begin() { return (_available = initializeTime()); }
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
    using namespace std::chrono;
    return system_clock::to_time_t(system_clock::now());
  }
  //
  static char *isoformatUTC(char *str, std::size_t size, std::time_t utctime) {
    struct tm tm;
    gmtime_r(&utctime, &tm);
    std::strftime(str, size, "%Y-%m-%dT%H:%M:%SZ", &tm);
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
    using namespace std::chrono;
    auto now = system_clock::to_time_t(system_clock::now());
    return static_cast<uint64_t>(std::difftime(now, startup_time));
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
    using namespace std::chrono;
    startup_time = system_clock::to_time_t(system_clock::now());
    _available = true;
    char buf[30];
    ESP_LOGI(TAG, "startup time: \"%s\"", isoformatUTC(buf, 30, startup_time));
  }
  //
  bool initializeTime(std::size_t retry_count = 100) {
    sntp_sync_status_t status = sntp_get_sync_status();
    //
    if (status == SNTP_SYNC_STATUS_COMPLETED) {
      ESP_LOGI(TAG, "SNTP synced, pass");
      return true;
    }
    ESP_LOGI(TAG, "Setting time using SNTP");

    configTzTime(TZ_TIME_ZONE, "ntp.jst.mfeed.ad.jp", "time.cloudflare.com",
                 "ntp.nict.jp");
    //
    for (std::size_t retry = 0; retry < retry_count; ++retry) {
      status = sntp_get_sync_status();
      if (status == SNTP_SYNC_STATUS_COMPLETED) {
        break;
      } else {
        delay(1000);
        Serial.print(".");
      }
    }

    Serial.println("");

    if (status == SNTP_SYNC_STATUS_COMPLETED) {
      sync();
      ESP_LOGI(TAG, "Time initialized!");
      return true;
    } else {
      ESP_LOGE(TAG, "SNTP sync failed");
      return false;
    }
  }
};

#endif // TICKTACK_HPP
