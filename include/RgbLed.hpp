// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include <FastLED.h>
#include <array>

//
//
//
class RgbLed {
  inline static RgbLed *_instance{nullptr};

public:
  RgbLed() {
    if (_instance) {
      delete _instance;
    }
    _instance = this;
  }
  virtual ~RgbLed() {}
  static RgbLed *getInstance() { return _instance; }
  //
  void begin();
  //
  void setBrightness(uint8_t scale) { FastLED.setBrightness(scale); }
  //
  void clear() { fill(CRGB::Black); }
  //
  void fill(CRGB color);
  //
  static CRGB colorFromCarbonDioxide(uint16_t ppm);
  //
  static CRGB hslToRgb(float_t hue /* 0 < 360*/, float_t saturation /* 0 < 1 */,
                       float_t lightness /* 0 < 1 */);

private:
  constexpr static auto NUM_OF_LEDS = uint8_t{10};
  constexpr static auto GPIO_PIN_SK6815 = uint16_t{25};
  std::array<CRGB, NUM_OF_LEDS> leds{};
};
