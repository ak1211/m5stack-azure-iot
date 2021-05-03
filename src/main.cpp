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

//
// globals
//
constexpr static clock_t SUPPRESSION_SECONDS_OF_FIRST_PUSH =
    120; // 120 seconds = 2 minutes

constexpr static uint16_t NUM_OF_TRY_TO_WIFI_CONNECTION = 50;

constexpr static uint8_t IOTHUB_PUSH_MESSAGE_EVERY_MINUTES = 1; // 1 mimutes
static_assert(IOTHUB_PUSH_MESSAGE_EVERY_MINUTES < 60,
              "IOTHUB_PUSH_MESSAGE_EVERY_MINUTES is lesser than 60 minutes.");

constexpr static uint8_t IOTHUB_PUSH_STATE_EVERY_MINUTES = 15; // 15 minutes
static_assert(IOTHUB_PUSH_STATE_EVERY_MINUTES < 60,
              "IOTHUB_PUSH_STATE_EVERY_MINUTES is lesser than 60 minutes.");

constexpr static uint8_t BME280_SENSING_EVERY_SECONDS = 2; // 2 seconds
static_assert(BME280_SENSING_EVERY_SECONDS < 60,
              "BME280_SENSING_EVERY_SECONDS is lesser than 60 seconds.");

constexpr static uint8_t SGP30_SENSING_EVERY_SECONDS = 1; // 1 seconds
static_assert(SGP30_SENSING_EVERY_SECONDS == 1,
              "SGP30_SENSING_EVERY_SECONDS is shoud be 1 seconds.");

constexpr static uint8_t SCD30_SENSING_EVERY_SECONDS = 2; // 2 seconds
static_assert(SCD30_SENSING_EVERY_SECONDS < 60,
              "SCD30_SENSING_EVERY_SECONDS is lesser than 60 seconds.");

//
// callbacks
//

//
static void sendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result) {
  if (result == IOTHUB_CLIENT_CONFIRMATION_OK) {
    ESP_LOGD("main", "Send Confirmation Callback finished.");
  }
}
//
static void messageCallback(const char *payLoad, int size) {
  ESP_LOGI("main", "Message callback:%s", payLoad);
}
//
static int deviceMethodCallback(const char *methodName,
                                const unsigned char *payload, int size,
                                unsigned char **response, int *response_size) {
  ESP_LOGI("main", "Try to invoke method %s", methodName);
  const char *responseMessage = "\"Successfully invoke device method\"";
  int result = 200;

  if (strcmp(methodName, "calibration") == 0) {
    /*
    ESP_LOGI("main", "Start calibrate the sensor");
    if (!system_properties.sgp30.begin()) {
      responseMessage = "\"calibration failed\"";
      result = 503;
    }
    */
  } else if (strcmp(methodName, "start") == 0) {
    ESP_LOGI("main", "Start sending temperature and humidity data");
    //    messageSending = true;
  } else if (strcmp(methodName, "stop") == 0) {
    ESP_LOGI("main", "Stop sending temperature and humidity data");
    //    messageSending = false;
  } else {
    ESP_LOGI("main", "No method %s found", methodName);
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
    ESP_LOGD("main", "SAS token expired.");
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
      ESP_LOGD("main", "SAS token expired, stop the SNTP.");
      sntp_stop();
    }
    break;
  case IOTHUB_CLIENT_CONNECTION_DEVICE_DISABLED:
    ESP_LOGE("main", "IOTHUB_CLIENT_CONNECTION_DEVICE_DISABLED");
    break;
  case IOTHUB_CLIENT_CONNECTION_BAD_CREDENTIAL:
    ESP_LOGE("main", "IOTHUB_CLIENT_CONNECTION_BAD_CREDENTIAL");
    break;
  case IOTHUB_CLIENT_CONNECTION_RETRY_EXPIRED:
    ESP_LOGE("main", "IOTHUB_CLIENT_CONNECTION_RETRY_EXPIRED");
    break;
  case IOTHUB_CLIENT_CONNECTION_NO_NETWORK:
    ESP_LOGE("main", "IOTHUB_CLIENT_CONNECTION_NO_NETWORK");
    break;
  case IOTHUB_CLIENT_CONNECTION_COMMUNICATION_ERROR:
    ESP_LOGE("main", "IOTHUB_CLIENT_CONNECTION_COMMUNICATION_ERROR");
    break;
  case IOTHUB_CLIENT_CONNECTION_OK:
    ESP_LOGD("main", "IOTHUB_CLIENT_CONNECTION_OK");
    break;
  }
}
//
static void deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState,
                               const unsigned char *payLoad, int size) {
  switch (updateState) {
  case DEVICE_TWIN_UPDATE_COMPLETE:
    ESP_LOGD("main", "device_twin_update_complete");
    break;

  case DEVICE_TWIN_UPDATE_PARTIAL:
    ESP_LOGD("main", "device_twin_update_partial");
    break;
  }

  // Display Twin message.
  char *buff = (char *)calloc(size + 1, sizeof(char));
  if (!buff) {
    ESP_LOGE("main", "memory allocation error");
    return;
  }

  strncpy(buff, (const char *)payLoad, size);
  ESP_LOGI("main", "%s", buff);

  StaticJsonDocument<MESSAGE_MAX_LEN> json;
  DeserializationError error = deserializeJson(json, buff);
  if (error) {
    ESP_LOGE("main", "%s", error.f_str());
    return;
  }
  free(buff);
  //
  /*
  const char *updatedAt = json["reported"]["sgp30_baseline"]["updatedAt"];
  if (updatedAt) {
    ESP_LOGI("main", "%s", updatedAt);
    // set baseline
    uint16_t tvoc_baseline =
        json["reported"]["sgp30_baseline"]["tvoc"].as<uint16_t>();
    uint16_t eCo2_baseline =
        json["reported"]["sgp30_baseline"]["eCo2"].as<uint16_t>();
    ESP_LOGD("main", "eCo2:%d, TVOC:%d", eCo2_baseline, tvoc_baseline);
    int ret =
        system_properties.sgp30.setIAQBaseline(eCo2_baseline, tvoc_baseline);
    ESP_LOGD("main", "setIAQBaseline():%d", ret);
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
    ESP_LOGD("main", "sntp enabled.");
    //
    time_t tm_now;
    struct tm utc;
    time(&tm_now);
    ESP_LOGD("main", "tm_now: %d", tm_now);
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
    ESP_LOGD("main", "sntp disabled.");
    //
    // get time and date from RTC
    //
    RTC_DateTypeDef rtcDate;
    RTC_TimeTypeDef rtcTime;
    M5.Rtc.GetDate(&rtcDate);
    M5.Rtc.GetTime(&rtcTime);
    ESP_LOGD("main", "RTC \"%04d-%02d-%02dT%02d:%02d:%02dZ\"", rtcDate.Year,
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
  time_t measured_at;
  Bme280 bme280;
  Sgp30 sgp30;
  Scd30 scd30;
};

static struct MeasurementSets periodical_measurement_sets() {
  time_t measured_at;
  time(&measured_at);
  struct tm utc;
  gmtime_r(&measured_at, &utc);

  Peripherals &peri = Peripherals::getInstance();

  if (peri.bme280.ready() && utc.tm_sec % BME280_SENSING_EVERY_SECONDS == 0) {
    peri.bme280.measure(measured_at);
  }
  if (peri.sgp30.ready() && utc.tm_sec % SGP30_SENSING_EVERY_SECONDS == 0) {
    peri.sgp30.measure(measured_at);
  }
  if (peri.scd30.ready() && utc.tm_sec % SCD30_SENSING_EVERY_SECONDS == 0) {
    peri.scd30.measure(measured_at);
  }
  return {.measured_at = measured_at,
          .bme280 = peri.bme280.valuesWithSMA(),
          .sgp30 = peri.sgp30.valuesWithSMA(),
          .scd30 = peri.scd30.valuesWithSMA()};
}

//
//
//
static void periodical_push_message(const MeasurementSets &m) {
  JsonDocSets doc_sets = {};

  // BME280 sensor values.
  // Temperature, Relative Humidity, Pressure
  if (m.bme280.good()) {
    Peripherals &peri = Peripherals::getInstance();
    // calculate the Aboslute Humidity from Temperature and Relative Humidity
    MilligramPerCubicMetre absolute_humidity = calculateAbsoluteHumidity(
        m.bme280.get().temperature, m.bme280.get().relative_humidity);
    ESP_LOGI("main", "absolute humidity: %d", absolute_humidity.value);
    // set "Absolute Humidity" to the SGP30 sensor.
    if (!peri.sgp30.setHumidity(absolute_humidity)) {
      ESP_LOGE("main", "setHumidity error.");
    }
    IotHubClient::pushMessage(
        takeMessageFromJsonDocSets(mapToJson(doc_sets, m.bme280.get())));
  }
  doc_sets.message.clear();
  doc_sets.state.clear();
  // SGP30 sensor values.
  // eCo2, TVOC
  if (m.sgp30.good()) {
    IotHubClient::pushMessage(
        takeMessageFromJsonDocSets(mapToJson(doc_sets, m.sgp30.get())));
  }
  doc_sets.message.clear();
  doc_sets.state.clear();
  // SCD30 sensor values.
  // co2, Temperature, Relative Humidity
  if (m.scd30.good()) {
    IotHubClient::pushMessage(
        takeMessageFromJsonDocSets(mapToJson(doc_sets, m.scd30.get())));
  }
}

//
//
//
static void periodical_push_state(const MeasurementSets &m) {
  JsonDocSets doc_sets = {};
  auto batt_state = System::getBatteryStatus();

  if (m.sgp30.good()) {
    auto json = takeStateFromJsonDocSets(mapToJson(doc_sets, m.sgp30.get()));
    char buf[10];
    memset(buf, '\0', sizeof(buf));
    snprintf(buf, 10, "%d%%", static_cast<int>(batt_state.percentage));
    json["batteryLevel"] = buf;
    IotHubClient::pushState(json);
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
  if (peri.ticktack.uptime_seconds() <= SUPPRESSION_SECONDS_OF_FIRST_PUSH) {
    // Discarding this measurements
    allowToPushMessage = false;
    allowToPushState = false;
  }
  //
  // insert to local database
  //
  if (allowToPushMessage) {
    peri.data_logging_file.write_data_to_log_file(m.bme280, m.sgp30, m.scd30);
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
    if (allowToPushMessage) {
      periodical_push_message(m);
    }
    if (allowToPushState) {
      periodical_push_state(m);
    }
    IotHubClient::update(true);
  }
}

//
//
//
void loop() {
  ArduinoOTA.handle();

  static clock_t before_clock = 0;
  clock_t now_clock = clock();

  if ((now_clock - before_clock) >= CLOCKS_PER_SEC) {
    MeasurementSets m = periodical_measurement_sets();
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
