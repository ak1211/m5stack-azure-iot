// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef PERIPHERALS_HPP
#define PERIPHERALS_HPP

#include "data_logging_file.hpp"
#include "led_signal.hpp"
#include "local_database.hpp"
#include "power_status.hpp"
#include "screen.hpp"
#include "sensor.hpp"
#include "ticktack.hpp"
#include "wifi_launcher.hpp"
#include <cstdint>
#include <string>

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
  constexpr static const char *sensor_id_bme280 =
      "m5stack-bme280-device-bme280";
  constexpr static const char *sensor_id_sgp30 = "m5stack-bme280-device-sgp30";
  constexpr static const char *sensor_id_scd30 = "m5stack-bme280-device-scd30";
  //
  TickTack ticktack;
  PowerStatus power_status;
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
