// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "peripherals.hpp"

Peripherals Peripherals::_instance = Peripherals();

//
//
//
Peripherals::Peripherals()
    : ticktack(TickTack()), power_status(PowerStatus()),
      bme280(Sensor<Bme280>(sensor_id_bme280)),
      sgp30(Sensor<Sgp30>(sensor_id_sgp30)),
      scd30(Sensor<Scd30>(sensor_id_scd30)),
      local_database(LocalDatabase(sqlite3_file_name)),
      data_logging_file(
          DataLoggingFile(data_log_file_name, header_log_file_name)),
      screen(Screen(local_database)), led_signal(LedSignal()), wifi_launcher() {
  ESP_LOGD("peripherals", "Peripherals construct success.");
}
//
bool Peripherals::begin(const std::string &wifi_ssid,
                        const std::string &wifi_password) {
  // initializing screen
  _instance.screen.begin();
  // initializing the neopixel leds
  _instance.led_signal.begin();
  // initializing the data logging file
  _instance.data_logging_file.begin();
  // initializing the local database
  _instance.local_database.beginDb();
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
  // connect to Wifi network
  _instance.wifi_launcher.begin(wifi_ssid, wifi_password);
  //
  return true;
}
//
Peripherals &Peripherals::getInstance() { return _instance; }
