// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once

#include "data_logging_file.hpp"
#include "iothub_client.hpp"
#include "led_signal.hpp"
#include "local_database.hpp"
#include "screen.hpp"
#include "sensor.hpp"
#include "system_power.hpp"
#include "ticktack.hpp"
#include "wifi_launcher.hpp"
#include <cstdint>
#include <string>

constexpr static auto SENSOR_DESCRIPTOR_BME280 =
    SensorDescriptor({'b', 'm', 'e', '2', '8', '0', '\0', '\0'});
constexpr static auto SENSOR_DESCRIPTOR_SGP30 =
    SensorDescriptor({'s', 'g', 'p', '3', '0', '\0', '\0', '\0'});
constexpr static auto SENSOR_DESCRIPTOR_SCD30 =
    SensorDescriptor({'s', 'c', 'd', '3', '0', '\0', '\0', '\0'});
//
//
//
class Peripherals final {
  static Peripherals _instance;
  void operator=(const Peripherals &);
  explicit Peripherals(const Peripherals &);
  Peripherals();

public:
  constexpr static auto BME280_I2C_ADDRESS = uint8_t{0x76};
  constexpr static auto data_log_file_name =
      std::string_view{"/data-logging.csv"};
  constexpr static auto header_log_file_name =
      std::string_view{"/header-data-logging.csv"};
  constexpr static auto sqlite3_file_name =
      std::string_view{"/sd/measurements.sqlite3"};
  //
  TickTack ticktack;
  SystemPower system_power;
  Sensor<Bme280> bme280;
  Sensor<Sgp30> sgp30;
  Sensor<Scd30> scd30;
  LocalDatabase local_database;
  DataLoggingFile data_logging_file;
  Screen screen;
  LedSignal led_signal;
  WifiLauncher wifi_launcher;
  IotHubClient iothub_client;
  //
  static bool begin(std::string_view wifi_ssid, std::string_view wifi_password,
                    std::string_view iothub_fqdn, std::string_view device_id,
                    std::string_view device_key);
  static Peripherals &getInstance();
};
