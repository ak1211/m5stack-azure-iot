// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "wifi_launcher.hpp"
#include "peripherals.hpp"

#include <WiFi.h>

//
//
//
bool WifiLauncher::begin(std::string_view wifi_ssid,
                         std::string_view wifi_password) {
  Screen::lcd.println(F("Wifi Connecting..."));
  WiFi.begin(wifi_ssid.data(), wifi_password.data());
  wifi_connected = false;
  for (auto n = 1; n <= NUM_OF_TRY_TO_WIFI_CONNECTION; ++n) {
    if (WiFi.status() == WL_CONNECTED) {
      wifi_connected = true;
      break;
    } else {
      delay(500);
      Screen::lcd.print(F("."));
    }
  }
  if (wifi_connected) {
    Screen::lcd.println(F("Wifi connected"));
    Screen::lcd.println(F("IP address: "));
    Screen::lcd.println(WiFi.localIP());
  } else {
    Screen::lcd.println(F("Wifi is NOT connected."));
  }
  return wifi_connected;
}
