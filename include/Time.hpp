// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include <chrono>
#include <esp_sntp.h>
#include <string>

namespace Time {
// time zone = Asia_Tokyo(UTC+9)
constexpr static std::string_view TZ_TIME_ZONE{"JST-9"};

extern bool time_is_synced;

// NTPと同期する
extern void init() noexcept;

// NTPと同期完了？
inline bool sync_completed() noexcept { return time_is_synced; }

//
extern std::chrono::seconds uptime() noexcept;

// iso8601 format.
extern std::string isoformatUTC(std::time_t utctime) noexcept;
inline std::string
isoformatUTC(std::chrono::system_clock::time_point utctp) noexcept {
  return isoformatUTC(std::chrono::system_clock::to_time_t(utctp));
}
} // namespace Time
