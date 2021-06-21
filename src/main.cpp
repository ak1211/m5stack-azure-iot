// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "credentials.h"
#include "iothub_client.hpp"
#include "peripherals.hpp"
#include <ArduinoOTA.h>
#include <M5Core2.h>
#include <Wifi.h>
#include <ctime>
#include <lwip/apps/sntp.h>

#include <LovyanGFX.hpp>

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
                     Credentials.connection_string);
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
static std::string &
absolute_sensor_id_from_SensorDescriptor(std::string &output,
                                         SensorDescriptor descriptor) {
  std::string strDescriptor;
  descriptor.toString(strDescriptor);
  //
  output = Credentials.device_id;
  output.push_back('-');
  output += strDescriptor;
  return output;
}

//
//
//
static void periodical_push_message(const MeasurementSets &m) {
  Peripherals &peri = Peripherals::getInstance();

  // BME280 sensor values.
  // Temperature, Relative Humidity, Pressure
  if (m.bme280.good()) {
    TempHumiPres temp_humi_pres = m.bme280.get();
    //
    // calculate the Aboslute Humidity from Temperature and Relative Humidity
    MilligramPerCubicMetre absolute_humidity = calculateAbsoluteHumidity(
        temp_humi_pres.temperature, temp_humi_pres.relative_humidity);
    ESP_LOGI(TAG, "absolute humidity: %d", absolute_humidity.value);
    // set "Absolute Humidity" to the SGP30 sensor.
    if (!peri.sgp30.setHumidity(absolute_humidity)) {
      ESP_LOGE(TAG, "setHumidity error.");
    }
    std::string sensor_id;
    absolute_sensor_id_from_SensorDescriptor(sensor_id,
                                             temp_humi_pres.sensor_descriptor);
    if (peri.iothub_client.pushTempHumiPres(sensor_id, temp_humi_pres)) {
      ESP_LOGD(TAG, "pushTempHumiPres() success.");
    } else {
      ESP_LOGE(TAG, "pushTempHumiPres() failure.");
      peri.iothub_client.check(false);
    }
  }
  // SGP30 sensor values.
  // eCo2, TVOC
  if (m.sgp30.good()) {
    TvocEco2 tvoc_eco2 = m.sgp30.get();
    std::string sensor_id;
    absolute_sensor_id_from_SensorDescriptor(sensor_id,
                                             tvoc_eco2.sensor_descriptor);
    if (peri.iothub_client.pushTvocEco2(sensor_id, tvoc_eco2)) {
      ESP_LOGD(TAG, "pushTvocEco2() success.");
    } else {
      ESP_LOGE(TAG, "pushTvocEco2() failure.");
      peri.iothub_client.check(false);
    }
  }
  // SCD30 sensor values.
  // co2, Temperature, Relative Humidity
  if (m.scd30.good()) {
    Co2TempHumi co2_temp_humi = m.scd30.get();
    std::string sensor_id;
    absolute_sensor_id_from_SensorDescriptor(sensor_id,
                                             co2_temp_humi.sensor_descriptor);
    if (peri.iothub_client.pushCo2TempHumi(sensor_id, co2_temp_humi)) {
      ESP_LOGD(TAG, "pushCo2TempHumi() success.");
    } else {
      ESP_LOGE(TAG, "pushCo2TempHumi() failure.");
      peri.iothub_client.check(false);
    }
  }
}

//
//
//
static void periodical_push_state() {
  Peripherals &peri = Peripherals::getInstance();
  if (peri.system_power.needToUpdate()) {
    peri.system_power.update();
  }
  DynamicJsonDocument json(IotHubClient::MESSAGE_MAX_LEN);
  if (json.capacity() == 0) {
    ESP_LOGE(TAG, "memory allocation error.");
    return;
  }
  char buf[10] = {'\0'};
  snprintf(buf, 10, "%d%%",
           static_cast<int>(peri.system_power.getBatteryPercentage()));
  json["batteryLevel"] = buf;
  uint32_t upseconds = peri.ticktack.uptimeSeconds();
  json["uptime"] = upseconds;
  if (peri.iothub_client.pushState(json)) {
    ESP_LOGD(TAG, "pushState() success.");
  } else {
    ESP_LOGE(TAG, "pushState() failure.");
    peri.iothub_client.check(false);
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
    if (allowToPushState) {
      periodical_push_state();
    }
  }
  //
  if (allowToPushMessage) {
    //
    // insert to local logging file
    //
    if (m.bme280.good() && m.sgp30.good() && m.scd30.good()) {
      peri.data_logging_file.write_data_to_log_file(
          m.measured_at, m.bme280.get(), m.sgp30.get(), m.scd30.get());
    }
    //
    // insert to local database
    //
    if (m.bme280.good()) {
      peri.local_database.insert(m.bme280.get());
    }
    if (m.sgp30.good()) {
      peri.local_database.insert(m.sgp30.get());
    }
    if (m.scd30.good()) {
      peri.local_database.insert(m.scd30.get());
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
    peri.iothub_client.check(true);
  }

  static clock_t before_clock = 0;
  clock_t now_clock = clock();

  if ((now_clock - before_clock) >= CLOCKS_PER_SEC) {
    peri.ticktack.update();
    MeasurementSets m = periodical_measurement_sets(peri.ticktack.time());
    peri.screen.update(m.measured_at);
    if (m.scd30.good()) {
      Ppm co2 = m.scd30.get().co2;
      peri.led_signal.showSignal(co2);
    }
    periodical_send_to_iothub(m);
    before_clock = now_clock;
  }

  M5.update();
}
