// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "peripherals.hpp"
#include "credentials.h"

static constexpr char data_log_file_name[] = "/data-logging.csv";
static constexpr char header_log_file_name[] = "/header-data-logging.csv";

static constexpr uint8_t BME280_I2C_ADDRESS = 0x76;
Peripherals Peripherals::_instance = Peripherals();

//
//
//
Peripherals::Peripherals()
    : bme280(Sensor<Bme280>(Credentials.sensor_id.bme280)),
      sgp30(Sensor<Sgp30>(Credentials.sensor_id.sgp30)),
      scd30(Sensor<Scd30>(Credentials.sensor_id.scd30)),
      local_database(LocalDatabase("/sd/measurements.sqlite3")),
      data_logging_file(
          DataLoggingFile(data_log_file_name, header_log_file_name)),
      screen(Screen(local_database)), status({
                                          .startup_epoch = clock(),
                                          .has_WIFI_connection = false,
                                          .is_freestanding_mode = false,
                                      }),
      led_signal(LedSignal()) {
  ESP_LOGD("peripherals", "Peripherals construct success.");
}
//
bool Peripherals::begin() {
  // initializing the neopixel leds
  _instance.led_signal.begin();
  // initializing the data logging file
  _instance.data_logging_file.begin();
  // initializing the local database
  _instance.local_database.beginDb();
  // initializing screen
  _instance.screen.begin();
  // initializing sensor
  {
    HasSensor bme = _instance.bme280.begin(BME280_I2C_ADDRESS);
    HasSensor sgp = _instance.sgp30.begin();
    HasSensor scd = _instance.scd30.begin();
    do {
      if (bme == HasSensor::NoSensorFound) {
        Screen::lcd.print(F("BME280センサが見つかりません。\n"));
        delay(100);
        bme = _instance.bme280.begin(BME280_I2C_ADDRESS);
      }
      if (sgp == HasSensor::NoSensorFound) {
        Screen::lcd.print(F("SGP30センサが見つかりません。\n"));
        delay(100);
        sgp = _instance.sgp30.begin();
      }
      if (scd == HasSensor::NoSensorFound) {
        Screen::lcd.print(F("SCD30センサが見つかりません。\n"));
        delay(100);
        scd = _instance.scd30.begin();
      }
    } while (!(bme == HasSensor::Ok && sgp == HasSensor::Ok &&
               scd == HasSensor::Ok));
    Screen::lcd.clear();
    /*
    system_properties.scd30.printSensorDetails();
    system_properties.sgp30.printSensorDetails();
    system_properties.bme280.printSensorDetails();
    */
  }

  //
  return true;
}
//
Peripherals &Peripherals::getInstance() { return _instance; }
