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
static void sendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result) {
  if (result == IOTHUB_CLIENT_CONFIRMATION_OK) {
    ESP_LOGD(TAG, "Send Confirmation Callback finished.");
  }
}
//
static void messageCallback(const char *payLoad, int size) {
  ESP_LOGI(TAG, "Message callback:%s", payLoad);
}
//
static int deviceMethodCallback(const char *methodName,
                                const unsigned char *payload, int size,
                                unsigned char **response, int *response_size) {
  ESP_LOGI(TAG, "Try to invoke method %s", methodName);
  const char *responseMessage = "\"Successfully invoke device method\"";
  int result = 200;

  if (strcmp(methodName, "calibration") == 0) {
    /*
    ESP_LOGI(TAG, "Start calibrate the sensor");
    if (!system_properties.sgp30.begin()) {
      responseMessage = "\"calibration failed\"";
      result = 503;
    }
    */
  } else if (strcmp(methodName, "start") == 0) {
    ESP_LOGI(TAG, "Start sending temperature and humidity data");
    //    messageSending = true;
  } else if (strcmp(methodName, "stop") == 0) {
    ESP_LOGI(TAG, "Stop sending temperature and humidity data");
    //    messageSending = false;
  } else {
    ESP_LOGI(TAG, "No method %s found", methodName);
    responseMessage = "\"No method found\"";
    result = 404;
  }

  *response_size = strlen(responseMessage) + 1;
  *response = (unsigned char *)strdup(responseMessage);

  return result;
}
//
static void
connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result,
                         IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason) {
  switch (reason) {
  case IOTHUB_CLIENT_CONNECTION_EXPIRED_SAS_TOKEN:
    // SASトークンの有効期限切れ。
    ESP_LOGD(TAG, "SAS token expired.");
    if (result == IOTHUB_CLIENT_CONNECTION_UNAUTHENTICATED) {
      //
      // Info: >>>Connection status: timeout
      // Info: >>>Re-connect.
      // Info: Initializing SNTP
      // assertion "Operating mode must not be set while SNTP client is running"
      // failed: file
      // "/home/runner/work/esp32-arduino-lib-builder/esp32-arduino-lib-builder/esp-idf/components/lwip/lwip/src/apps/sntp/sntp.c",
      // line 600, function: sntp_setoperatingmode abort() was called at PC
      // 0x401215bf on core 1
      //
      // Esp32MQTTClient 側で再接続時に以上のログが出てabortするので,
      // この時点で SNTPを停止しておくことで abort を回避する。
      ESP_LOGD(TAG, "SAS token expired, stop the SNTP.");
      sntp_stop();
    }
    break;
  case IOTHUB_CLIENT_CONNECTION_DEVICE_DISABLED:
    ESP_LOGE(TAG, "IOTHUB_CLIENT_CONNECTION_DEVICE_DISABLED");
    break;
  case IOTHUB_CLIENT_CONNECTION_BAD_CREDENTIAL:
    ESP_LOGE(TAG, "IOTHUB_CLIENT_CONNECTION_BAD_CREDENTIAL");
    break;
  case IOTHUB_CLIENT_CONNECTION_RETRY_EXPIRED:
    ESP_LOGE(TAG, "IOTHUB_CLIENT_CONNECTION_RETRY_EXPIRED");
    break;
  case IOTHUB_CLIENT_CONNECTION_NO_NETWORK:
    ESP_LOGE(TAG, "IOTHUB_CLIENT_CONNECTION_NO_NETWORK");
    break;
  case IOTHUB_CLIENT_CONNECTION_COMMUNICATION_ERROR:
    ESP_LOGE(TAG, "IOTHUB_CLIENT_CONNECTION_COMMUNICATION_ERROR");
    break;
  case IOTHUB_CLIENT_CONNECTION_OK:
    ESP_LOGD(TAG, "IOTHUB_CLIENT_CONNECTION_OK");
    break;
  }
}
//
static void deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState,
                               const unsigned char *payLoad, int size) {
  switch (updateState) {
  case DEVICE_TWIN_UPDATE_COMPLETE:
    ESP_LOGD(TAG, "device_twin_update_complete");
    break;

  case DEVICE_TWIN_UPDATE_PARTIAL:
    ESP_LOGD(TAG, "device_twin_update_partial");
    break;
  }

  // Display Twin message.
  char *buff = (char *)calloc(size + 1, sizeof(char));
  if (!buff) {
    ESP_LOGE(TAG, "memory allocation error");
    return;
  }

  strncpy(buff, (const char *)payLoad, size);
  ESP_LOGI(TAG, "%s", buff);

  StaticJsonDocument<MESSAGE_MAX_LEN> json;
  DeserializationError error = deserializeJson(json, buff);
  if (error) {
    ESP_LOGE(TAG, "%s", error.f_str());
    return;
  }
  free(buff);
  //
  /*
  const char *updatedAt = json["reported"]["sgp30_baseline"]["updatedAt"];
  if (updatedAt) {
    ESP_LOGI(TAG, "%s", updatedAt);
    // set baseline
    uint16_t tvoc_baseline =
        json["reported"]["sgp30_baseline"]["tvoc"].as<uint16_t>();
    uint16_t eCo2_baseline =
        json["reported"]["sgp30_baseline"]["eCo2"].as<uint16_t>();
    ESP_LOGD(TAG, "eCo2:%d, TVOC:%d", eCo2_baseline, tvoc_baseline);
    int ret =
        system_properties.sgp30.setIAQBaseline(eCo2_baseline, tvoc_baseline);
    ESP_LOGD(TAG, "setIAQBaseline():%d", ret);
  }
  */
}
//
void btnAEvent(Event &e) {
  Peripherals &peri = Peripherals::getInstance();
  peri.screen.prev();
  peri.screen.update(time(nullptr));
}
//
void btnBEvent(Event &e) {
  Peripherals &peri = Peripherals::getInstance();
  peri.screen.home();
  peri.screen.update(time(nullptr));
}
//
void btnCEvent(Event &e) {
  Peripherals &peri = Peripherals::getInstance();
  peri.screen.next();
  peri.screen.update(time(nullptr));
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
  Peripherals::begin(Credentials.wifi_ssid, Credentials.wifi_password);
  Peripherals &peri = Peripherals::getInstance();

  //
  // register the button hook
  //
  M5.BtnA.addHandler(btnAEvent, E_RELEASE);
  M5.BtnB.addHandler(btnBEvent, E_RELEASE);
  M5.BtnC.addHandler(btnCEvent, E_RELEASE);
  M5.background.addHandler(releaseEvent, E_RELEASE);

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

  //
  // initializing IotHub client
  //
  if (peri.wifi_launcher.hasWifiConnection()) {
    IotHubClient::init(sendConfirmationCallback, messageCallback,
                       deviceMethodCallback, deviceTwinCallback,
                       connectionStatusCallback);
  }

  //
  // set to Real Time Clock
  //
  if (sntp_enabled()) {
    ESP_LOGD(TAG, "sntp enabled.");
    //
    time_t tm_now;
    struct tm utc;
    time(&tm_now);
    ESP_LOGD(TAG, "tm_now: %d", tm_now);
    gmtime_r(&tm_now, &utc);
    //
    RTC_TimeTypeDef rtcTime = {
        .Hours = static_cast<uint8_t>(utc.tm_hour),
        .Minutes = static_cast<uint8_t>(utc.tm_min),
        .Seconds = static_cast<uint8_t>(utc.tm_sec),
    };
    M5.Rtc.SetTime(&rtcTime);
    //
    RTC_DateTypeDef rtcDate = {
        .WeekDay = static_cast<uint8_t>(utc.tm_wday),
        .Month = static_cast<uint8_t>(utc.tm_mon + 1),
        .Date = static_cast<uint8_t>(utc.tm_mday),
        .Year = static_cast<uint16_t>(utc.tm_year + 1900),
    };
    M5.Rtc.SetDate(&rtcDate);
  } else {
    ESP_LOGD(TAG, "sntp disabled.");
    //
    // get time and date from RTC
    //
    RTC_DateTypeDef rtcDate;
    RTC_TimeTypeDef rtcTime;
    M5.Rtc.GetDate(&rtcDate);
    M5.Rtc.GetTime(&rtcTime);
    ESP_LOGD(TAG, "RTC \"%04d-%02d-%02dT%02d:%02d:%02dZ\"", rtcDate.Year,
             rtcDate.Month, rtcDate.Date, rtcTime.Hours, rtcTime.Minutes,
             rtcTime.Seconds);
  }

  //
  // start up
  //
  peri.screen.repaint(time(nullptr));
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

static struct MeasurementSets periodical_measurement_sets(std::time_t now) {
  Peripherals &peri = Peripherals::getInstance();

  if (peri.bme280.readyToRead(now)) {
    peri.bme280.read(now);
  }
  if (peri.sgp30.readyToRead(now)) {
    peri.sgp30.read(now);
  }
  if (peri.scd30.readyToRead(now)) {
    peri.scd30.read(now);
  }
  return {.measured_at = now,
          .bme280 = peri.bme280.calculateSMA(),
          .sgp30 = peri.sgp30.calculateSMA(),
          .scd30 = peri.scd30.calculateSMA()};
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
  JsonDocSets doc_sets = {};

  // BME280 sensor values.
  // Temperature, Relative Humidity, Pressure
  if (m.bme280.good()) {
    TempHumiPres temp_humi_pres = m.bme280.get();
    //
    Peripherals &peri = Peripherals::getInstance();
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
    IotHubClient::pushMessage(takeMessageFromJsonDocSets(
        mapToJson(doc_sets, sensor_id, temp_humi_pres)));
  }
  doc_sets.message.clear();
  doc_sets.state.clear();
  // SGP30 sensor values.
  // eCo2, TVOC
  if (m.sgp30.good()) {
    TvocEco2 tvoc_eco2 = m.sgp30.get();
    std::string sensor_id;
    absolute_sensor_id_from_SensorDescriptor(sensor_id,
                                             tvoc_eco2.sensor_descriptor);
    IotHubClient::pushMessage(
        takeMessageFromJsonDocSets(mapToJson(doc_sets, sensor_id, tvoc_eco2)));
  }
  doc_sets.message.clear();
  doc_sets.state.clear();
  // SCD30 sensor values.
  // co2, Temperature, Relative Humidity
  if (m.scd30.good()) {
    Co2TempHumi co2_temp_humi = m.scd30.get();
    std::string sensor_id;
    absolute_sensor_id_from_SensorDescriptor(sensor_id,
                                             co2_temp_humi.sensor_descriptor);
    IotHubClient::pushMessage(takeMessageFromJsonDocSets(
        mapToJson(doc_sets, sensor_id, co2_temp_humi)));
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
  StaticJsonDocument<1024> json;
  char buf[10] = {'\0'};
  snprintf(buf, 10, "%d%%",
           static_cast<int>(peri.system_power.getBatteryPercentage()));
  json["batteryLevel"] = buf;
  uint32_t upseconds = peri.ticktack.uptime_seconds();
  json["uptime"] = upseconds;
  IotHubClient::pushState(json);
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
    IotHubClient::update(true);
  }
}

//
//
//
void loop() {
  ArduinoOTA.handle();
  delay(1);

  static clock_t before_clock = 0;
  clock_t now_clock = clock();

  if ((now_clock - before_clock) >= CLOCKS_PER_SEC) {
    MeasurementSets m = periodical_measurement_sets(std::time(nullptr));
    Peripherals &peri = Peripherals::getInstance();
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
