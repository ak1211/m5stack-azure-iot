// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef VALUE_TYPES_HPP
#define VALUE_TYPES_HPP

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

/// [V] voltage
struct Voltage final {
  explicit Voltage(float init = 0.0f) : value{init} {}
  float get() const { return value; }
  float voltage() const {
    return std::round(static_cast<double>(value) * 1000.0) / 1000.0;
  }

private:
  float value;
};

/// [A] ampere
struct Ampere final {
  explicit Ampere(float init = 0.0f) : value{init} {}
  float get() const { return value; }
  float ampere() const {
    return std::round(static_cast<double>(value) * 1000.0) / 1000.0;
  }
  bool positive() const { return value > 0.0f; }
  bool negative() const { return value < 0.0f; }
  bool zero() const {
    return std::abs(value - 0.0f) <= std::numeric_limits<float>::epsilon();
  }

private:
  float value;
};

// degree celsius
struct DegC final {
  explicit DegC(float init = 0.0f) : value{init} {}
  float get() const { return value; }
  float degc() const {
    return std::round(static_cast<double>(value) * 100.0) / 100.0;
  }

private:
  float value;
};

// [hPa] hecto-pascal
struct HPa final {
  explicit HPa(float init = 0.0f) : value{init} {}
  float get() const { return value; }
  float hpa() const {
    return std::round(static_cast<double>(value) * 100.0) / 100.0;
  }

private:
  float value;
};

// [%] relative humidity
struct PcRH final {
  explicit PcRH(float init = 0.0f) : value{init} {}
  float get() const { return value; }
  float percentRH() const {
    return std::round(static_cast<double>(value) * 100.0) / 100.0;
  }

private:
  float value;
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
class SensorDescriptor {
public:
  SensorId id;
  //
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
  std::string toString() {
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
};

#endif // VALUE_TYPES_HPP
