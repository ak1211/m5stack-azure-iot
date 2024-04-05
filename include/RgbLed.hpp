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
  RgbLed() noexcept {
    if (_instance) {
      delete _instance;
    }
    _instance = this;
  }
  virtual ~RgbLed() noexcept {}
  static RgbLed *getInstance() noexcept { return _instance; }
  //
  void begin() noexcept;
  //
  void setBrightness(uint8_t scale) noexcept { FastLED.setBrightness(scale); }
  //
  void clear() noexcept { fill(CRGB::Black); }
  //
  void fill(CRGB color) noexcept;
  //
  static CRGB colorFromCarbonDioxide(uint16_t ppm) noexcept;
  //
  static CRGB hslToRgb(float_t hue /* 0 < 360*/, float_t saturation /* 0 < 1 */,
                       float_t lightness /* 0 < 1 */) noexcept;

private:
  constexpr static auto NUM_OF_LEDS = uint8_t{10};
  constexpr static auto GPIO_PIN_SK6815 = uint16_t{25};
  std::array<CRGB, NUM_OF_LEDS> leds{};
};
