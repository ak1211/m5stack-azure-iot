// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <ostream>
#include <string>
#include <vector>

/// [V] voltage
using Voltage = std::chrono::duration<float_t>;
using MilliVoltage = std::chrono::duration<int32_t, std::milli>; // 1/1000

/// [A] ampere
using Ampere = std::chrono::duration<float_t>;
using MilliAmpere = std::chrono::duration<int32_t, std::milli>; // 1/1000

// [℃] degree celsius
using DegC = std::chrono::duration<float_t>;
using CentiDegC = std::chrono::duration<int16_t, std::centi>; // 1/100

// [hPa] hecto-pascal
using HectoPa = std::chrono::duration<float_t, std::hecto>; // 100/1
using Pascal = std::chrono::duration<float_t>;
using DeciPa = std::chrono::duration<int32_t, std::deci>; // 1/10

// [%RH] relative humidity
using PctRH = std::chrono::duration<float_t>;
using CentiRH = std::chrono::duration<int16_t, std::centi>; // 1/100

// [mg / m^3] absolute humidity
struct MilligramPerCubicMetre final {
  uint32_t value;
  explicit MilligramPerCubicMetre(uint32_t init = 0u) : value{init} {}
  bool operator==(const MilligramPerCubicMetre &other) const {
    return value == other.value;
  }
  bool operator!=(const MilligramPerCubicMetre &other) const {
    return !(value == other.value);
  }
};

// [ppm] parts per million
struct Ppm final {
  uint16_t value;
  explicit Ppm(uint16_t init = 0u) : value{init} {}
  bool operator==(const Ppm &other) const { return value == other.value; }
  bool operator!=(const Ppm &other) const { return !(value == other.value); }
};

// [ppb] parts per billion
struct Ppb final {
  uint16_t value;
  explicit Ppb(uint16_t init = 0u) : value{init} {}
  bool operator==(const Ppb &other) const { return value == other.value; }
  bool operator!=(const Ppb &other) const { return !(value == other.value); }
};

// SGP30 baseline type
using BaselineSGP30T = uint16_t;

// SGP30 baseline(equivalent CO2)
struct BaselineECo2 final {
  BaselineSGP30T value;
  explicit BaselineECo2(BaselineSGP30T init = 0u) : value{init} {}
  bool operator==(const BaselineECo2 &other) const {
    return value == other.value;
  }
  bool operator!=(const BaselineECo2 &other) const {
    return !(value == other.value);
  }
};

// SGP30 baseline(Total VOC)
struct BaselineTotalVoc final {
  BaselineSGP30T value;
  explicit BaselineTotalVoc(BaselineSGP30T init = 0u) : value{init} {}
  bool operator==(const BaselineTotalVoc &other) const {
    return value == other.value;
  }
  bool operator!=(const BaselineTotalVoc &other) const {
    return !(value == other.value);
  }
};

using SensorId = uint64_t;
//
struct SensorDescriptor final {
  constexpr static uint64_t to_u64(const uint8_t *in) noexcept {
    return static_cast<uint64_t>(*in++) << 56 | // 1st byte
           static_cast<uint64_t>(*in++) << 48 | // 2nd byte
           static_cast<uint64_t>(*in++) << 40 | // 3rd byte
           static_cast<uint64_t>(*in++) << 32 | // 4th byte
           static_cast<uint64_t>(*in++) << 24 | // 5th byte
           static_cast<uint64_t>(*in++) << 16 | // 6th byte
           static_cast<uint64_t>(*in++) << 8 |  // 7th byte
           static_cast<uint64_t>(*in++);        // 8th byte
  }
  constexpr static void from_u64(uint64_t in, uint8_t *out) noexcept {
    *out++ = static_cast<uint8_t>(in >> 56 & 0xff); // 1st byte
    *out++ = static_cast<uint8_t>(in >> 48 & 0xff); // 2nd byte
    *out++ = static_cast<uint8_t>(in >> 40 & 0xff); // 3rd byte
    *out++ = static_cast<uint8_t>(in >> 32 & 0xff); // 4th byte
    *out++ = static_cast<uint8_t>(in >> 24 & 0xff); // 5th byte
    *out++ = static_cast<uint8_t>(in >> 16 & 0xff); // 6th byte
    *out++ = static_cast<uint8_t>(in >> 8 & 0xff);  // 7th byte
    *out++ = static_cast<uint8_t>(in & 0xff);       // 8th byte
  }
  //
  std::array<uint8_t, 9> strDescriptor{'\0', '\0', '\0', '\0', '\0',
                                       '\0', '\0', '\0', '\0'};
  explicit SensorDescriptor(SensorId init = 0) noexcept {
    from_u64(init, strDescriptor.begin());
  }
  constexpr SensorDescriptor(std::array<uint8_t, 8> init) noexcept
      : strDescriptor{init[0], init[1], init[2], init[3], init[4],
                      init[5], init[6], init[7], '\0'} {}
  bool operator==(const SensorDescriptor &other) const {
    return this->strDescriptor == other.strDescriptor;
  }
  bool operator!=(const SensorDescriptor &other) const {
    return !(*this == other);
  }
  constexpr operator SensorId() const noexcept {
    return to_u64(strDescriptor.begin());
  }
  std::string str() const noexcept {
    return std::string(reinterpret_cast<const char *>(strDescriptor.data()));
  }
};
