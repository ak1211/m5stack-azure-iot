// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once

#include "value_types.hpp"
#include <FastLED.h>
#include <algorithm>
#include <array>
#include <cmath>

//
//
//
class LedSignal {

public:
  constexpr static uint8_t NUM_OF_LEDS{10};
  constexpr static uint8_t GPIO_PIN_SK6815{25};
  constexpr static uint8_t BME280_I2C_ADDRESS{0x76};
  //
  bool begin() {
    FastLED.addLeds<SK6812, GPIO_PIN_SK6815, GRB>(std::data(leds),
                                                  std::size(leds));
    FastLED.setBrightness(64);
    offSignal();
    return true;
  }
  //
  void offSignal() {
    for (auto &v : leds) {
      v = CRGB::Black;
    }
    FastLED.show();
  }
  //
  static CRGB hslToRgb(double hue /* 0 < 360 */, double saturation /* 0 <= 1 */,
                       double lightness /* 0 <= 1 */) {
    double C = saturation * (1.0 - std::abs(2.0 * lightness - 1.0));
    double Max = lightness + (C / 2.0);
    double Min = lightness - (C / 2.0);
    double r, g, b;
    switch (static_cast<int>(hue) / 60) {
    case 0: /* 0 < 60 */
      r = Max;
      g = Min + (Max - Min) * hue / 60.0;
      b = Min;
      break;
    case 1: /* 60 < 120 */
      r = Min + (Max - Min) * (120.0 - hue) / 60.0;
      g = Max;
      b = Min;
      break;
    case 2: /* 120 < 180 */
      r = Min;
      g = Max;
      b = Min + (Max - Min) * (hue - 120.0) / 60.0;
      break;
    case 3: /* 180 < 240 */
      r = Min;
      g = Min + (Max - Min) * (240 - hue) / 60.0;
      b = Max;
      break;
    case 4: /* 240 < 300 */
      r = Min + (Max - Min) * (hue - 240.0) / 60.0;
      g = Min;
      b = Max;
      break;
    case 5: /* 300 < 360 */
      r = Max;
      g = Min;
      b = Min + (Max - Min) * (360.0 - hue) / 60.0;
      break;
    default:
      r = g = b = 0.0;
      break;
    }
    return CRGB{static_cast<uint8_t>(255.0 * r),
                static_cast<uint8_t>(255.0 * g),
                static_cast<uint8_t>(255.0 * b)};
  }
  //
  void showSignal(Ppm co2) {
    constexpr double start = 360 + 240.0;       // 600 degrees == BLUE
    constexpr double end = 300.0;               // 300 degrees == PURPLE
    constexpr double L = std::abs(start - end); //
    constexpr double rotation = -1.0;           // anticlockwise
    //
    double normalized =
        std::clamp(static_cast<double>(co2.value), 0.0, 3500.0) / 3500.0;
    double hue = start + rotation * (L * normalized);

    CRGB rgb = hslToRgb(hue >= 360.0 ? hue - 360.0 : hue, 0.85, 0.35);
    std::fill(leds.begin(), leds.end(), rgb);
    FastLED.show();
  }

private:
  std::array<CRGB, NUM_OF_LEDS> leds;
};
