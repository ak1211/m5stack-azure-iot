// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "Sensor.hpp"
#include <Wire.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace Peripherals {
constexpr static auto BME280_I2C_ADDRESS = uint8_t{0x76};
//
constexpr static auto SENSOR_DESCRIPTOR_BME280 =
    SensorDescriptor({'B', 'M', 'E', '2', '8', '0', '\0', '\0'});
constexpr static auto SENSOR_DESCRIPTOR_SGP30 =
    SensorDescriptor({'S', 'G', 'P', '3', '0', '\0', '\0', '\0'});
constexpr static auto SENSOR_DESCRIPTOR_SCD30 =
    SensorDescriptor({'S', 'C', 'D', '3', '0', '\0', '\0', '\0'});
constexpr static auto SENSOR_DESCRIPTOR_SCD41 =
    SensorDescriptor({'S', 'C', 'D', '4', '1', '\0', '\0', '\0'});
constexpr static auto SENSOR_DESCRIPTOR_M5ENV3 =
    SensorDescriptor({'M', '5', 'E', 'N', 'V', '3', '\0', '\0'});
//
extern std::vector<std::unique_ptr<Sensor::Device>> sensors;
// 初期化
extern void init(TwoWire &wire, int8_t sda_pin, int8_t scl_pin);
} // namespace Peripherals