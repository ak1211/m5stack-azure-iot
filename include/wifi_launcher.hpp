// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef WIFI_LAUNCHER_HPP
#define WIFI_LAUNCHER_HPP

#include <cstdint>

//
//
//
class WifiLauncher {
public:
  constexpr static uint16_t NUM_OF_TRY_TO_WIFI_CONNECTION = 50;
  //
  WifiLauncher() : wifi_connected(false) {}
  bool hasWifiConnection() { return wifi_connected; }
  bool begin(const char *wifi_ssid, const char *wifi_password);

private:
  bool wifi_connected;
};

#endif // WIFI_LAUNCHER_HPP
