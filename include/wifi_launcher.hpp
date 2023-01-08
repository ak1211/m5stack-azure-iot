// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once

#include <cstdint>
#include <string>

//
//
//
class WifiLauncher {
  bool wifi_connected;

public:
  constexpr static auto NUM_OF_TRY_TO_WIFI_CONNECTION = uint16_t{50};
  //
  WifiLauncher() : wifi_connected(false) {}
  bool hasWifiConnection() { return wifi_connected; }
  bool begin(std::string_view wifi_ssid, std::string_view wifi_password);
};
