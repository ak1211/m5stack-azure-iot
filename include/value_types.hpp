// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

/// [V] voltage
class Voltage final {
  float value;

public:
  constexpr static float SHIFT = 1000.0f;
  explicit Voltage(float init = 0.0f) : value{init} {}
  float get() const { return value; }
  float voltage() const { return std::nearbyintf(value * SHIFT) / SHIFT; }
};

/// [A] ampere
class Ampere final {
  float value;

public:
  constexpr static float SHIFT = 1000.0f;
  explicit Ampere(float init = 0.0f) : value{init} {}
  float get() const { return value; }
  float ampere() const { return std::nearbyintf(value * SHIFT) / SHIFT; }
  bool negative() const { return std::signbit(value); }
  bool positive() const { return !negative(); }
  bool zero() const { return std::fpclassify(value) == FP_ZERO; }
};

// degree celsius
class DegC final {
  constexpr static float SHIFT = 100.0f;
  float value;

public:
  explicit DegC(float init = 0.0f) : value{init} {}
  float get() const { return value; }
  float degc() const { return std::nearbyintf(value * SHIFT) / SHIFT; }
};

// [hPa] hecto-pascal
class HPa final {
  float value;

public:
  constexpr static float SHIFT = 100.0f;
  explicit HPa(float init = 0.0f) : value{init} {}
  float get() const { return value; }
  float hpa() const { return std::nearbyintf(value * SHIFT) / SHIFT; }
};

// [%] relative humidity
class PcRH final {
  float value;

public:
  constexpr static float SHIFT = 100.0f;
  explicit PcRH(float init = 0.0f) : value{init} {}
  float get() const { return value; }
  float percentRH() const { return std::nearbyintf(value * SHIFT) / SHIFT; }
};

// [mg / m^3] absolute humidity
struct MilligramPerCubicMetre final {
  explicit MilligramPerCubicMetre(uint32_t init = 0u) : value{init} {}
  uint32_t value;
};

// [ppm] parts per million
struct Ppm final {
  explicit Ppm(uint16_t init = 0u) : value{init} {}
  uint16_t value;
};

// [ppb] parts per billion
struct Ppb final {
  explicit Ppb(uint16_t init = 0u) : value{init} {}
  uint16_t value;
};

// SGP30 baseline type
using BaselineSGP30T = uint16_t;

// SGP30 baseline(equivalent CO2)
struct BaselineECo2 final {
  BaselineECo2(BaselineSGP30T init = 0u) : value{init} {}
  BaselineSGP30T value;
};

// SGP30 baseline(Total VOC)
struct BaselineTotalVoc final {
  BaselineTotalVoc(BaselineSGP30T init = 0u) : value{init} {}
  BaselineSGP30T value;
};

using SensorId = uint64_t;
//
struct SensorDescriptor final {
  explicit SensorDescriptor(SensorId inital = 0) : id{inital} {}
  explicit SensorDescriptor(char c0, char c1, char c2, char c3, char c4,
                            char c5, char c6, char c7) {
    id = static_cast<uint64_t>(c0) << 56 | // 1st byte
         static_cast<uint64_t>(c1) << 48 | // 2nd byte
         static_cast<uint64_t>(c2) << 40 | // 3rd byte
         static_cast<uint64_t>(c3) << 32 | // 4th byte
         static_cast<uint64_t>(c4) << 24 | // 5th byte
         static_cast<uint64_t>(c5) << 16 | // 6th byte
         static_cast<uint64_t>(c6) << 8 |  // 7th byte
         static_cast<uint64_t>(c7) << 0;   // 8th byte
  }
  //
  std::string toString() const {
    char buf[8] = {
        static_cast<char>(id >> 56 & 0xff), // 1st byte
        static_cast<char>(id >> 48 & 0xff), // 2nd byte
        static_cast<char>(id >> 40 & 0xff), // 3rd byte
        static_cast<char>(id >> 32 & 0xff), // 4th byte
        static_cast<char>(id >> 24 & 0xff), // 5th byte
        static_cast<char>(id >> 16 & 0xff), // 6th byte
        static_cast<char>(id >> 8 & 0xff),  // 7th byte
        static_cast<char>(id >> 0 & 0xff),  // 8th byte
    };
    return std::string(buf);
  }
  //
  SensorId id;
};
