// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef VALUE_TYPES_HPP
#define VALUE_TYPES_HPP

#include <cstdint>
#include <ctime>

// degree celsius
struct DegC final {
  DegC(float init = 0.0f) : value(init) {}
  float value;
};

// hecto-pascal
struct HPa final {
  HPa(float init = 0.0f) : value(init) {}
  float value;
};

// % relative humidity
struct PcRH final {
  PcRH(float init = 0.0f) : value(init) {}
  float value;
};

// [mg / m^3] absolute humidity
struct MilligramPerCubicMetre final {
  MilligramPerCubicMetre(uint32_t init = 0u) : value(init) {}
  uint32_t value;
};

// parts per million
struct Ppm final {
  Ppm(uint16_t init = 0u) : value(init) {}
  uint16_t value;
};

// parts per billion
struct Ppb final {
  Ppb(uint16_t init = 0u) : value(init) {}
  uint16_t value;
};

// SGP30 baseline(equivalent CO2)
struct BaselineECo2 final {
  BaselineECo2(uint16_t init = 0u) : value(init) {}
  uint16_t value;
};

// SGP30 baseline(Total VOC)
struct BaselineTotalVoc final {
  BaselineTotalVoc(uint16_t init = 0u) : value(init) {}
  uint16_t value;
};

#endif // VALUE_TYPES_HPP
