// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "peripherals.hpp"

constexpr static const char *TAG = "PeripheralsModule";

Peripherals Peripherals::_instance = Peripherals();

//
//
//
Peripherals::Peripherals()
    : ticktack(TickTack()),
      system_power(SystemPower()),
      bme280(Sensor<Bme280>(SENSOR_DESCRIPTOR_BME280)),
      sgp30(Sensor<Sgp30>(SENSOR_DESCRIPTOR_SGP30)),
      scd30(Sensor<Scd30>(SENSOR_DESCRIPTOR_SCD30)),
      local_database(LocalDatabase(sqlite3_file_name)),
      data_logging_file(
          DataLoggingFile(data_log_file_name, header_log_file_name)),
      screen(Screen(local_database)),
      led_signal(LedSignal()),
      wifi_launcher() {
  ESP_LOGD(TAG, "Peripherals construct success.");
}
//
bool Peripherals::begin(const std::string &wifi_ssid,
                        const std::string &wifi_password) {
  // initializing system status
  _instance.system_power.begin();
  // initializing screen
  _instance.screen.begin();
  // initializing the neopixel leds
  _instance.led_signal.begin();
  // initializing the data logging file
  _instance.data_logging_file.begin();
  // initializing the local database
  _instance.local_database.begin();
  // initializing sensor
  {
    // get baseline for "Sensirion SGP30: Air Quality Sensor" from database
    std::time_t baseline_eco2_measured_at;
    std::time_t baseline_tvoc_measured_at;
    MeasuredValues<BaselineECo2> baseline_eco2;
    MeasuredValues<BaselineTotalVoc> baseline_tvoc;
    size_t count = _instance.local_database.get_latest_eco2_tvoc_baseline(
        _instance.sgp30.getSensorDescriptor().id, baseline_eco2_measured_at,
        baseline_eco2, baseline_tvoc_measured_at, baseline_tvoc);
    if (baseline_eco2.good() && baseline_tvoc.good()) {
      ESP_LOGD(TAG,
               "SGP30 baseline from DB: count(%d), eco2_at(%d), eco2(%d), "
               "tvoc_at(%d), tvoc(%d).",
               count, baseline_eco2_measured_at, baseline_eco2.get().value,
               baseline_tvoc_measured_at, baseline_tvoc.get().value);
    } else {
      ESP_LOGD(TAG, "SGP30 baseline from DB: count(%d).", count);
    }
    //
    HasSensor bme = HasSensor::NoSensorFound;
    HasSensor sgp = HasSensor::NoSensorFound;
    HasSensor scd = HasSensor::NoSensorFound;
    while (1) {
      bme = _instance.bme280.begin(std::time(nullptr), BME280_I2C_ADDRESS);
      if (bme == HasSensor::NoSensorFound) {
        Screen::lcd.print(F("BME280センサが見つかりません。\n"));
        delay(100);
      }
      sgp = _instance.sgp30.begin(std::time(nullptr), baseline_eco2_measured_at,
                                  baseline_eco2, baseline_tvoc_measured_at,
                                  baseline_tvoc);
      if (sgp == HasSensor::NoSensorFound) {
        Screen::lcd.print(F("SGP30センサが見つかりません。\n"));
        delay(100);
      }
      scd = _instance.scd30.begin(std::time(nullptr));
      if (scd == HasSensor::NoSensorFound) {
        Screen::lcd.print(F("SCD30センサが見つかりません。\n"));
        delay(100);
      }
      if (bme == HasSensor::Ok && sgp == HasSensor::Ok &&
          scd == HasSensor::Ok) {
        break;
      }

      delay(1000);
      Screen::lcd.clear();
    }
    /*
    system_properties.scd30.printSensorDetails();
    system_properties.sgp30.printSensorDetails();
    system_properties.bme280.printSensorDetails();
    */
    Screen::lcd.clear();
  }
  // connect to Wifi network
  _instance.wifi_launcher.begin(wifi_ssid, wifi_password);
  //
  return true;
}
//
Peripherals &Peripherals::getInstance() { return _instance; }
