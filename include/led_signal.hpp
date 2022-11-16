// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once

#include "value_types.hpp"
#include <FastLED.h>
#include <array>

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
  void showSignal(Ppm co2) {
    auto c = CRGB::Black;
    if (0 <= co2.value && co2.value < 500) {
      // blue
      c = CRGB::Blue;
    } else if (500 <= co2.value && co2.value < 900) {
      // lightblue
      c = CRGB::LightBlue;
    } else if (900 <= co2.value && co2.value < 1200) {
      // green
      c = CRGB::Green;
    } else if (1200 <= co2.value && co2.value < 1700) {
      // yellow
      c = CRGB::Yellow;
    } else if (1700 <= co2.value && co2.value < 2500) {
      // orange
      c = CRGB::Orange;
    } else {
      // red
      c = CRGB::Red;
    }
    for (auto &v : leds) {
      v = c;
    }
    FastLED.show();
  }

private:
  std::array<CRGB, NUM_OF_LEDS> leds;
};
