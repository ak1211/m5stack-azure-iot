// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "BottomCaseLed.hpp"
#include <FastLED.h>
#include <algorithm>
#include <array>
#include <cmath>

std::array<CRGB, BottomCaseLed::NUM_OF_LEDS> BottomCaseLed::leds{};

//
void BottomCaseLed::init() noexcept {
  FastLED.addLeds<SK6812, GPIO_PIN_SK6815, GRB>(leds.data(), leds.size());
  FastLED.setBrightness(50);
  offSignal();
}

//
void BottomCaseLed::offSignal() noexcept {
  std::fill(leds.begin(), leds.end(), CRGB::Black);
  FastLED.show();
}

//
void BottomCaseLed::showSignal(Ppm co2) noexcept {
  constexpr float_t start = 360.0 + 240.0;     // 600 degrees == BLUE
  constexpr float_t end = 300.0;               // 300 degrees == PURPLE
  constexpr float_t L = std::abs(start - end); //
  constexpr float_t rotation = -1.0;           // anticlockwise
  //
  const float_t normalized =
      std::clamp(static_cast<double>(co2.value), 0.0, 3500.0) / 3500.0;
  const float_t hue = start + rotation * (L * normalized);
  constexpr float_t saturation = 0.9;
  constexpr float_t lightness = 0.3;

  auto rgb = hslToRgb(hue >= 360.0 ? hue - 360.0 : hue, saturation, lightness);
  std::fill(leds.begin(), leds.end(), rgb);
  FastLED.show();
}

//
CRGB BottomCaseLed::hslToRgb(float_t hue /* 0 < 360*/,
                             float_t saturation /* 0 < 1 */,
                             float_t lightness /* 0 < 1 */) noexcept {
  float_t C = saturation * (1.0 - std::abs(2.0 * lightness - 1.0));
  float_t Max = lightness + (C / 2.0); /* 0 <= 1.0*/
  float_t Min = lightness - (C / 2.0); /* 0 <= 1.0*/
  float_t r, g, b;
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
  return CRGB{static_cast<uint8_t>(255.0 * r), static_cast<uint8_t>(255.0 * g),
              static_cast<uint8_t>(255.0 * b)};
}
