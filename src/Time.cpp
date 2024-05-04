// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "Time.hpp"
#include <Arduino.h>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <future>
#include <iomanip>
#include <sstream>

using namespace std::chrono;
using namespace std::chrono_literals;

static const steady_clock::time_point startup_time{steady_clock::now()};

bool Time::time_is_synced{false};

static void sntp_sync_time_callback(struct timeval *) {
  Time::time_is_synced = true;
}

//
// NTPと同期する
//
void Time::init() {
  time_is_synced = false;
  auto status = sntp_get_sync_status();
  if (status == SNTP_SYNC_STATUS_RESET) {
    sntp_set_time_sync_notification_cb(sntp_sync_time_callback);
    configTzTime(TZ_TIME_ZONE.data(), "ntp.jst.mfeed.ad.jp",
                 "time.cloudflare.com", "ntp.nict.jp");
  }
}

//
seconds Time::uptime() {
  auto elapsed = steady_clock::now() - startup_time;
  return duration_cast<seconds>(elapsed);
}

// iso8601 format.
std::string Time::isoformatUTC(std::time_t utctime) {
  struct tm tm;
  gmtime_r(&utctime, &tm);
  std::ostringstream oss;
  oss << std::put_time(&tm, "%FT%TZ");
  return oss.str().c_str();
}