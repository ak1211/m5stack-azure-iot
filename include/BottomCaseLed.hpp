// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "value_types.hpp"
#include <FastLED.h>
#include <array>
#include <cstddef>

namespace BottomCaseLed {
constexpr static auto NUM_OF_LEDS = uint8_t{10};
constexpr static auto GPIO_PIN_SK6815 = uint8_t{25};
extern std::array<CRGB, NUM_OF_LEDS> leds;
//
extern void init() noexcept;
//
extern void showProgressive(CRGB color) noexcept;
//
extern void offSignal() noexcept;
//
extern void showSignal(Ppm co2) noexcept;
//
extern CRGB hslToRgb(float_t hue /* 0 < 360*/, float_t saturation /* 0 < 1 */,
                     float_t lightness /* 0 < 1 */) noexcept;
} // namespace BottomCaseLed
