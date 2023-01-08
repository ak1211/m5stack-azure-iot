// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

/// [V] voltage
using Voltage = std::chrono::duration<float>;
using MilliVoltage = std::chrono::duration<int32_t, std::milli>; // 1/1000

/// [A] ampere
using Ampere = std::chrono::duration<float>;
using MilliAmpere = std::chrono::duration<int32_t, std::milli>; // 1/1000

// [℃] degree celsius
using DegC = std::chrono::duration<float>;
using CentiDegC = std::chrono::duration<int16_t, std::centi>; // 1/100

// [hPa] hecto-pascal
using HectoPa = std::chrono::duration<float, std::hecto>; // 100/1
using Pascal = std::chrono::duration<uint32_t>;

// [RH] relative humidity
using RelHumidity = std::chrono::duration<float>;
using MilliRH = std::chrono::duration<uint16_t, std::milli>; // 1/1000

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
  explicit BaselineECo2(BaselineSGP30T init = 0u) : value{init} {}
  BaselineSGP30T value;
};

// SGP30 baseline(Total VOC)
struct BaselineTotalVoc final {
  explicit BaselineTotalVoc(BaselineSGP30T init = 0u) : value{init} {}
  BaselineSGP30T value;
};

using SensorId = uint64_t;
//
struct SensorDescriptor final {
  SensorId id;
  //
  explicit SensorDescriptor(SensorId inital = 0) : id{inital} {}
  constexpr SensorDescriptor(std::array<uint8_t, 8> init)
      : id{static_cast<uint64_t>(init[0]) << 56 | // 1st byte
           static_cast<uint64_t>(init[1]) << 48 | // 2nd byte
           static_cast<uint64_t>(init[2]) << 40 | // 3rd byte
           static_cast<uint64_t>(init[3]) << 32 | // 4th byte
           static_cast<uint64_t>(init[4]) << 24 | // 5th byte
           static_cast<uint64_t>(init[5]) << 16 | // 6th byte
           static_cast<uint64_t>(init[6]) << 8 |  // 7th byte
           static_cast<uint64_t>(init[7])}        // 8th byte
  {}
  operator std::string() const {
    std::vector<char> buf;
    buf.reserve(9);
    buf.push_back(static_cast<char>(id >> 56 & 0xff)); // 1st byte
    buf.push_back(static_cast<char>(id >> 48 & 0xff)); // 2nd byte
    buf.push_back(static_cast<char>(id >> 40 & 0xff)); // 3rd byte
    buf.push_back(static_cast<char>(id >> 32 & 0xff)); // 4th byte
    buf.push_back(static_cast<char>(id >> 24 & 0xff)); // 5th byte
    buf.push_back(static_cast<char>(id >> 16 & 0xff)); // 6th byte
    buf.push_back(static_cast<char>(id >> 8 & 0xff));  // 7th byte
    buf.push_back(static_cast<char>(id & 0xff));       // 8th byte
    buf.push_back('\0');
    return std::string(buf.data());
  }
};
