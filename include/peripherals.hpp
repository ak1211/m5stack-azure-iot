// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef PERIPHERALS_HPP
#define PERIPHERALS_HPP

#include "data_logging_file.hpp"
#include "led_signal.hpp"
#include "local_database.hpp"
#include "screen.hpp"
#include "sensor.hpp"
#include "system_power.hpp"
#include "ticktack.hpp"
#include "wifi_launcher.hpp"
#include <cstdint>
#include <string>

static const SensorDescriptor SENSOR_DESCRIPTOR_BME280 =
    SensorDescriptor('b', 'm', 'e', '2', '8', '0', '\0', '\0');
static const SensorDescriptor SENSOR_DESCRIPTOR_SGP30 =
    SensorDescriptor('s', 'g', 'p', '3', '0', '\0', '\0', '\0');
static const SensorDescriptor SENSOR_DESCRIPTOR_SCD30 =
    SensorDescriptor('s', 'c', 'd', '3', '0', '\0', '\0', '\0');
//
//
//
class Peripherals {
public:
  constexpr static uint8_t BME280_I2C_ADDRESS = 0x76;
  constexpr static const char *data_log_file_name = "/data-logging.csv";
  constexpr static const char *header_log_file_name =
      "/header-data-logging.csv";
  constexpr static const char *sqlite3_file_name = "/sd/measurements.sqlite3";
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
  //
  static bool begin(const std::string &wifi_ssid,
                    const std::string &wifi_password);
  static Peripherals &getInstance();

private:
  static Peripherals _instance;
  void operator=(const Peripherals &);
  Peripherals(const Peripherals &);
  Peripherals();
};

#endif // PERIPHERALS_HPP
