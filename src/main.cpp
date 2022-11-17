// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "credentials.h"
#include "iothub_client.hpp"
#include "peripherals.hpp"
#include <ArduinoOTA.h>
#include <LovyanGFX.hpp>
#include <M5Core2.h>
#include <WiFi.h>
#include <chrono>
#include <ctime>
#include <lwip/apps/sntp.h>

constexpr static const char *TAG = "MainModule";

//
// globals
//
constexpr static uint8_t IOTHUB_PUSH_MESSAGE_EVERY_MINUTES = 1; // 1 mimutes
static_assert(IOTHUB_PUSH_MESSAGE_EVERY_MINUTES < 60,
              "IOTHUB_PUSH_MESSAGE_EVERY_MINUTES is lesser than 60 minutes.");

constexpr static uint8_t IOTHUB_PUSH_STATE_EVERY_MINUTES = 15; // 15 minutes
static_assert(IOTHUB_PUSH_STATE_EVERY_MINUTES < 60,
              "IOTHUB_PUSH_STATE_EVERY_MINUTES is lesser than 60 minutes.");

//
// callbacks
//

//
void btnAEvent(Event &e) {
  Peripherals &peri = Peripherals::getInstance();
  peri.screen.prev();
  peri.screen.update(peri.ticktack.time());
}
//
void btnBEvent(Event &e) {
  Peripherals &peri = Peripherals::getInstance();
  peri.screen.home();
  peri.screen.update(peri.ticktack.time());
}
//
void btnCEvent(Event &e) {
  Peripherals &peri = Peripherals::getInstance();
  peri.screen.next();
  peri.screen.update(peri.ticktack.time());
}
//
void releaseEvent(Event &e) {
  Peripherals &peri = Peripherals::getInstance();
  peri.screen.releaseEvent(e);
}

//
//
//
void setup() {
  //
  // initializing M5Stack and UART, I2C, Touch, RTC, etc. peripherals.
  //
  M5.begin(true, true, true, true);
  Peripherals::begin(Credentials.wifi_ssid, Credentials.wifi_password,
                     Credentials.iothub_fqdn, Credentials.device_id,
                     Credentials.device_key);
  Peripherals &peri = Peripherals::getInstance();

  //
  // register the button hook
  //
  M5.BtnA.addHandler(btnAEvent, E_RELEASE);
  M5.BtnB.addHandler(btnBEvent, E_RELEASE);
  M5.BtnC.addHandler(btnCEvent, E_RELEASE);
  M5.background.addHandler(releaseEvent, E_RELEASE);

  //
  // start up
  //
  peri.screen.repaint(peri.ticktack.time());
}

//
//
//
struct MeasurementSets {
  std::time_t measured_at;
  Bme280 bme280;
  Sgp30 sgp30;
  Scd30 scd30;
};

static struct MeasurementSets
periodical_measurement_sets(std::time_t measured_at) {
  Peripherals &peri = Peripherals::getInstance();

  if (peri.bme280.readyToRead(measured_at)) {
    peri.bme280.read(measured_at);
  }
  if (peri.sgp30.readyToRead(measured_at)) {
    peri.sgp30.read(measured_at);
  }
  if (peri.scd30.readyToRead(measured_at)) {
    peri.scd30.read(measured_at);
  }
  return {.measured_at = measured_at,
          .bme280 = peri.bme280.calculateSMA(measured_at),
          .sgp30 = peri.sgp30.calculateSMA(measured_at),
          .scd30 = peri.scd30.calculateSMA(measured_at)};
}

//
//
//
static std::string
absolute_sensor_id_from_SensorDescriptor(SensorDescriptor descriptor) {
  std::string a{Credentials.device_id};
  std::string b{'-'};
  std::string c{descriptor.toString()};
  return std::string{a + b + c};
}

//
//
//
static void periodical_push_message(const MeasurementSets &m) {
  Peripherals &peri = Peripherals::getInstance();

  // BME280 sensor values.
  // Temperature, Relative Humidity, Pressure
  if (m.bme280.has_value()) {
    TempHumiPres temp_humi_pres = m.bme280.value();
    //
    // calculate the Aboslute Humidity from Temperature and Relative Humidity
    MilligramPerCubicMetre absolute_humidity = calculateAbsoluteHumidity(
        temp_humi_pres.temperature, temp_humi_pres.relative_humidity);
    ESP_LOGI(TAG, "absolute humidity: %d", absolute_humidity.value);
    // set "Absolute Humidity" to the SGP30 sensor.
    if (!peri.sgp30.setHumidity(absolute_humidity)) {
      ESP_LOGE(TAG, "setHumidity error.");
    }
    if (auto sid = absolute_sensor_id_from_SensorDescriptor(
            temp_humi_pres.sensor_descriptor);
        peri.iothub_client.pushTempHumiPres(sid, temp_humi_pres)) {
      ESP_LOGD(TAG, "pushTempHumiPres() success.");
    } else {
      ESP_LOGE(TAG, "pushTempHumiPres() failure.");
      peri.iothub_client.check();
    }
  }
  // SGP30 sensor values.
  // eCo2, TVOC
  if (m.sgp30.has_value()) {
    TvocEco2 tvoc_eco2 = m.sgp30.value();
    if (auto sid = absolute_sensor_id_from_SensorDescriptor(
            tvoc_eco2.sensor_descriptor);
        peri.iothub_client.pushTvocEco2(sid, tvoc_eco2)) {
      ESP_LOGD(TAG, "pushTvocEco2() success.");
    } else {
      ESP_LOGE(TAG, "pushTvocEco2() failure.");
      peri.iothub_client.check();
    }
  }
  // SCD30 sensor values.
  // co2, Temperature, Relative Humidity
  if (m.scd30.has_value()) {
    Co2TempHumi co2_temp_humi = m.scd30.value();
    if (auto sid = absolute_sensor_id_from_SensorDescriptor(
            co2_temp_humi.sensor_descriptor);
        peri.iothub_client.pushCo2TempHumi(sid, co2_temp_humi)) {
      ESP_LOGD(TAG, "pushCo2TempHumi() success.");
    } else {
      ESP_LOGE(TAG, "pushCo2TempHumi() failure.");
      peri.iothub_client.check();
    }
  }
}

//
//
//
static void periodical_send_to_iothub(const MeasurementSets &m) {
  struct tm utc;
  gmtime_r(&m.measured_at, &utc);
  //
  bool allowToPushMessage =
      utc.tm_sec == 0 && utc.tm_min % IOTHUB_PUSH_MESSAGE_EVERY_MINUTES == 0;
  bool allowToPushState =
      utc.tm_sec == 0 && utc.tm_min % IOTHUB_PUSH_STATE_EVERY_MINUTES == 0;

  Peripherals &peri = Peripherals::getInstance();
  //
  if (peri.wifi_launcher.hasWifiConnection()) {
    //
    // send to IoT Hub
    //
    if (allowToPushMessage) {
      periodical_push_message(m);
    }
  }
  //
  if (allowToPushMessage) {
    //
    // insert to local logging file
    //
    if (m.bme280.has_value() && m.sgp30.has_value() && m.scd30.has_value()) {
      peri.data_logging_file.write_data_to_log_file(
          m.measured_at, m.bme280.value(), m.sgp30.value(), m.scd30.value());
    }
    //
    // insert to local database
    //
    if (m.bme280.has_value()) {
      peri.local_database.insert(m.bme280.value());
    }
    if (m.sgp30.has_value()) {
      peri.local_database.insert(m.sgp30.value());
    }
    if (m.scd30.has_value()) {
      peri.local_database.insert(m.scd30.value());
    }
  }
}

//
//
//
void loop() {
  ArduinoOTA.handle();
  delay(1);

  Peripherals &peri = Peripherals::getInstance();
  if (peri.wifi_launcher.hasWifiConnection()) {
    peri.iothub_client.check();
  }
  using namespace std::chrono;

  static time_point before_time = system_clock::now();

  if (auto time = system_clock::now(); (time - before_time) >= seconds{1}) {
    peri.ticktack.update();
    MeasurementSets m = periodical_measurement_sets(peri.ticktack.time());
    peri.screen.update(m.measured_at);
    if (m.scd30.has_value()) {
      Ppm co2 = m.scd30.value().co2;
      peri.led_signal.showSignal(co2);
    }
    periodical_send_to_iothub(m);
    before_time = time;
  }

  M5.update();
}
