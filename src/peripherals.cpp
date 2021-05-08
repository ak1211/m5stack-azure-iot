// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "peripherals.hpp"
#include <lwip/apps/sntp.h>

constexpr static const char *TAG = "PeripheralsModule";

Peripherals Peripherals::_instance = Peripherals();

//
//
//
Peripherals::Peripherals()
    : ticktack{TickTack()},
      system_power{SystemPower()},
      bme280{Sensor<Bme280>(SENSOR_DESCRIPTOR_BME280)},
      sgp30{Sensor<Sgp30>(SENSOR_DESCRIPTOR_SGP30)},
      scd30{Sensor<Scd30>(SENSOR_DESCRIPTOR_SCD30)},
      local_database{LocalDatabase(sqlite3_file_name)},
      data_logging_file{
          DataLoggingFile(data_log_file_name, header_log_file_name)},
      screen{Screen(local_database)},
      led_signal{LedSignal()},
      wifi_launcher{WifiLauncher()},
      iothub_client{IotHubClient()} {}
//
bool Peripherals::begin(const std::string &wifi_ssid,
                        const std::string &wifi_password,
                        const std::string &iothub_connectionstring) {
  // initialize module: SystemPower
  if (_instance.system_power.begin()) {
    ESP_LOGD(TAG, "initialize module: SystemPower Ok.");
  }
  // initialize module: Screen
  if (_instance.screen.begin()) {
    ESP_LOGD(TAG, "initialize module: Screen Ok.");
  }
  // initialize module: LedSignal
  if (_instance.led_signal.begin()) {
    ESP_LOGD(TAG, "initialize module: LedSignal Ok.");
  }
  // initialize module: WifiLauncher
  bool available_wifi = _instance.wifi_launcher.begin(wifi_ssid, wifi_password);
  if (available_wifi) {
    ESP_LOGD(TAG, "initialize module: Wifilauncher Ok.");
  }
  // initialize module: IotHubClient
  if (_instance.iothub_client.begin(iothub_connectionstring)) {
    ESP_LOGD(TAG, "initialize module: IotHubClient Ok.");
  }
  // initialize module: DataLoggingFile
  if (_instance.data_logging_file.begin()) {
    ESP_LOGD(TAG, "initialize module: DataLoggingFile Ok.");
  }
  // initializing the local database
  bool available_database = _instance.local_database.begin();
  if (available_database) {
    ESP_LOGD(TAG, "initialize module: LocalDatabase Ok.");
  }
  // initializing sensor
  {
    // get baseline for "Sensirion SGP30: Air Quality Sensor" from database
    size_t count;
    std::time_t baseline_eco2_measured_at;
    MeasuredValues<BaselineECo2> baseline_eco2;
    count = _instance.local_database.get_latest_baseline_eco2(
        _instance.sgp30.getSensorDescriptor().id, baseline_eco2_measured_at,
        baseline_eco2);
    if (baseline_eco2.good()) {
      ESP_LOGD(
          TAG,
          "get_latest_baseline_eco2: count(%d), eco2_at(%d), baseline_eco2(%d)",
          count, baseline_eco2_measured_at, baseline_eco2.get().value);
    } else {
      ESP_LOGD(TAG, "get_latest_baseline_eco2: count(%d).", count);
    }
    //
    std::time_t baseline_tvoc_measured_at;
    MeasuredValues<BaselineTotalVoc> baseline_tvoc;
    count = _instance.local_database.get_latest_baseline_total_voc(
        _instance.sgp30.getSensorDescriptor().id, baseline_tvoc_measured_at,
        baseline_tvoc);
    if (baseline_tvoc.good()) {
      ESP_LOGD(TAG,
               "get_latest_baseline_total_voc: count(%d), tvoc_at(%d), "
               "baseline_tvoc(%d)",
               count, baseline_tvoc_measured_at, baseline_tvoc.get().value);
    } else {
      ESP_LOGD(TAG, "get_latest_baseline_total_voc: count(%d).", count);
    }
    //
    HasSensor bme = HasSensor::NoSensorFound;
    HasSensor sgp = HasSensor::NoSensorFound;
    HasSensor scd = HasSensor::NoSensorFound;
    while (1) {
      bme = _instance.bme280.begin(BME280_I2C_ADDRESS);
      if (bme == HasSensor::NoSensorFound) {
        Screen::lcd.print(F("BME280センサが見つかりません。\n"));
        delay(100);
      }
      sgp = _instance.sgp30.begin(baseline_eco2, baseline_tvoc);
      if (sgp == HasSensor::NoSensorFound) {
        Screen::lcd.print(F("SGP30センサが見つかりません。\n"));
        delay(100);
      }
      scd = _instance.scd30.begin();
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
  //
  return true;
}
//
Peripherals &Peripherals::getInstance() { return _instance; }
