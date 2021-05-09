// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "peripherals.hpp"
#include <ArduinoOTA.h>
#include <lwip/apps/sntp.h>
#include <tuple>

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
      screen{Screen()},
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
    ESP_LOGD(TAG, "initialize module: WifiLauncher Ok.");
  }

  //
  // OTA
  //
  ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else // U_SPIFFS
          type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS
        // using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
          Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
          Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
          Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
          Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
          Serial.println("End Failed");
      });

  ArduinoOTA.begin();

  // initialize module: IotHubClient
  if (_instance.iothub_client.begin(iothub_connectionstring)) {
    ESP_LOGD(TAG, "initialize module: IotHubClient Ok.");
  }
  // initialize module: DataLoggingFile
  if (_instance.data_logging_file.begin()) {
    ESP_LOGD(TAG, "initialize module: DataLoggingFile Ok.");
  }
  // initialize module: LocalDatabase
  bool available_database = _instance.local_database.begin();
  if (available_database) {
    ESP_LOGD(TAG, "initialize module: LocalDatabase Ok.");
  }
  // initializing sensor
  {
    // get baseline for "Sensirion SGP30: Air Quality Sensor" from database
    MeasuredValues<BaselineECo2> baseline_eco2;
    auto eco2 = _instance.local_database.get_latest_baseline_eco2(
        _instance.sgp30.getSensorDescriptor().id);
    if (std::get<0>(eco2)) {
      baseline_eco2 = MeasuredValues<BaselineECo2>(std::get<2>(eco2));
      ESP_LOGD(TAG, "get_latest_baseline_eco2: at(%d), baseline(%d)",
               std::get<1>(eco2), std::get<2>(eco2));
    } else {
      ESP_LOGD(TAG, "get_latest_baseline_eco2: failed.");
    }
    //
    MeasuredValues<BaselineTotalVoc> baseline_tvoc;
    auto tvoc = _instance.local_database.get_latest_baseline_total_voc(
        _instance.sgp30.getSensorDescriptor().id);
    if (std::get<0>(tvoc)) {
      baseline_tvoc = MeasuredValues<BaselineTotalVoc>(std::get<2>(tvoc));
      ESP_LOGD(TAG, "get_latest_baseline_total_voc: at(%d), baseline(%d)",
               std::get<1>(tvoc), std::get<2>(tvoc));
    } else {
      ESP_LOGD(TAG, "get_latest_baseline_total_voc: failed.");
    }
    //
    bool bme{false};
    bool sgp{false};
    bool scd{false};
    while (1) {
      bme = _instance.bme280.begin(BME280_I2C_ADDRESS);
      if (!bme) {
        Screen::lcd.print(F("BME280センサが見つかりません。\n"));
        delay(100);
      }
      sgp = _instance.sgp30.begin(baseline_eco2, baseline_tvoc);
      if (!sgp) {
        Screen::lcd.print(F("SGP30センサが見つかりません。\n"));
        delay(100);
      }
      scd = _instance.scd30.begin();
      if (!scd) {
        Screen::lcd.print(F("SCD30センサが見つかりません。\n"));
        delay(100);
      }
      if (bme && sgp && scd) {
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
  // waiting for Synchronize NTP server
  while (!sntp_enabled()) {
    delay(100);
  }
  // initialize module: TickTack
  if (_instance.ticktack.begin()) {
    ESP_LOGD(TAG, "initialize module: TickTack Ok.");
  }
  //
  return true;
}
//
Peripherals &Peripherals::getInstance() { return _instance; }
