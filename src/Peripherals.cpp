// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "Peripherals.hpp"

#include <M5Unified.h>

std::vector<std::unique_ptr<Sensor::Device>> Peripherals::sensors{};

//
void Peripherals::init(TwoWire &wire, int8_t sda_pin, int8_t scl_pin) {
  sensors.emplace_back(std::move(std::make_unique<Sensor::Bme280Device>(
      SENSOR_DESCRIPTOR_BME280, BME280_I2C_ADDRESS, wire)));
  sensors.emplace_back(std::move(
      std::make_unique<Sensor::Sgp30Device>(SENSOR_DESCRIPTOR_SGP30, wire)));
  sensors.emplace_back(std::move(
      std::make_unique<Sensor::Scd30Device>(SENSOR_DESCRIPTOR_SCD30, wire)));
  sensors.emplace_back(std::move(
      std::make_unique<Sensor::Scd41Device>(SENSOR_DESCRIPTOR_SCD41, wire)));
  sensors.emplace_back(std::move(std::make_unique<Sensor::M5Env3Device>(
      SENSOR_DESCRIPTOR_M5ENV3, wire, sda_pin, scl_pin)));
}
