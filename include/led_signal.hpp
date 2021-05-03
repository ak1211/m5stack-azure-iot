// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef LED_SIGNAL_HPP
#define LED_SIGNAL_HPP

#include "value_types.hpp"
#include <Adafruit_NeoPixel.h>

//
//
//
class LedSignal {
public:
  static constexpr uint16_t NUMPIXELS = 10;
  static constexpr uint16_t GPIO_PIN_NEOPIXEL = 25;
  static constexpr uint8_t BME280_I2C_ADDRESS = 0x76;
  //
  LedSignal() {
    pixels =
        Adafruit_NeoPixel(NUMPIXELS, GPIO_PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
  }
  //
  void begin() {
    pixels.begin();
    pixels.setBrightness(64);
    offSignal();
  }
  //
  void offSignal() {
    uint32_t c = Adafruit_NeoPixel::Color(0, 0, 0);
    for (int i = 0; i < pixels.numPixels(); ++i) {
      pixels.setPixelColor(i, c);
    }
    pixels.show();
  }
  //
  void showSignal(Ppm co2) {
    uint32_t c = Adafruit_NeoPixel::Color(0, 0, 0);
    if (0 <= co2.value && co2.value < 499) {
      // blue
      c = Adafruit_NeoPixel::Color(21, 21, 88);
    } else if (500 <= co2.value && co2.value < 899) {
      // bluegreen
      c = Adafruit_NeoPixel::Color(21, 88, 88);
    } else if (900 <= co2.value && co2.value < 1199) {
      // green
      c = Adafruit_NeoPixel::Color(0, 204, 0);
    } else if (1200 <= co2.value && co2.value < 1699) {
      // yellow
      c = Adafruit_NeoPixel::Color(204, 204, 0);
    } else if (1700 <= co2.value && co2.value < 2499) {
      // pink
      c = Adafruit_NeoPixel::Color(204, 0, 102);
    } else {
      // red
      c = Adafruit_NeoPixel::Color(204, 0, 0);
    }
    for (int i = 0; i < pixels.numPixels(); ++i) {
      pixels.setPixelColor(i, c);
    }
    pixels.show();
  }

private:
  Adafruit_NeoPixel pixels;
};

#endif // LED_SIGNAL_HPP
