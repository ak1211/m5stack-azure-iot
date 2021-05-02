// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "credentials.h"
#include "iothub_client.hpp"
#include "local_database.hpp"
#include "screen.hpp"
#include "sensor.hpp"
#include "system_status.hpp"
#include <ArduinoOTA.h>
#include <M5Core2.h>
#include <SD.h>
#include <Wifi.h>
#include <ctime>
#include <lwip/apps/sntp.h>

#include <Adafruit_NeoPixel.h>
#include <LovyanGFX.hpp>

static constexpr uint16_t NUMPIXELS = 10;
static constexpr uint16_t GPIO_PIN_NEOPIXEL = 25;
static constexpr uint8_t BME280_I2C_ADDRESS = 0x76;

//
// globals
//
struct SystemProperties {
  Sensor<Bme280> bme280;
  Sensor<Sgp30> sgp30;
  Sensor<Scd30> scd30;
  LocalDatabase localDatabase;
  File data_logging_file;
  Screen screen;
  System::Status status;
  Adafruit_NeoPixel pixels;
};

static SystemProperties system_properties = {
    .bme280 = Sensor<Bme280>(Credentials.sensor_id.bme280),
    .sgp30 = Sensor<Sgp30>(Credentials.sensor_id.sgp30),
    .scd30 = Sensor<Scd30>(Credentials.sensor_id.scd30),
    .localDatabase = LocalDatabase("/sd/measurements.sqlite3"),
    .data_logging_file = File(),
    .screen = Screen(system_properties.localDatabase),
    .status =
        {
            .startup_epoch = clock(),
            .has_WIFI_connection = false,
            .is_freestanding_mode = false,
            .is_data_logging_to_file = false,
        },
    .pixels =
        Adafruit_NeoPixel(NUMPIXELS, GPIO_PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800),
};

static constexpr char data_log_file_name[] = "/data-logging.csv";
static constexpr char header_log_file_name[] = "/header-data-logging.csv";

static constexpr clock_t SUPPRESSION_TIME_OF_FIRST_PUSH =
    CLOCKS_PER_SEC * 120; // 120 seconds = 2 minutes

static constexpr uint16_t NUM_OF_TRY_TO_WIFI_CONNECTION = 50;

static constexpr uint8_t IOTHUB_PUSH_MESSAGE_EVERY_MINUTES = 1; // 1 mimutes
static_assert(IOTHUB_PUSH_MESSAGE_EVERY_MINUTES < 60,
              "IOTHUB_PUSH_MESSAGE_EVERY_MINUTES is lesser than 60 minutes.");

static constexpr uint8_t IOTHUB_PUSH_STATE_EVERY_MINUTES = 15; // 15 minutes
static_assert(IOTHUB_PUSH_STATE_EVERY_MINUTES < 60,
              "IOTHUB_PUSH_STATE_EVERY_MINUTES is lesser than 60 minutes.");

static constexpr uint8_t BME280_SENSING_EVERY_SECONDS = 2; // 2 seconds
static_assert(BME280_SENSING_EVERY_SECONDS < 60,
              "BME280_SENSING_EVERY_SECONDS is lesser than 60 seconds.");

static constexpr uint8_t SGP30_SENSING_EVERY_SECONDS = 1; // 1 seconds
static_assert(SGP30_SENSING_EVERY_SECONDS == 1,
              "SGP30_SENSING_EVERY_SECONDS is shoud be 1 seconds.");

static constexpr uint8_t SCD30_SENSING_EVERY_SECONDS = 2; // 2 seconds
static_assert(SCD30_SENSING_EVERY_SECONDS < 60,
              "SCD30_SENSING_EVERY_SECONDS is lesser than 60 seconds.");

//
//
//
static void write_data_to_log_file(const Bme280 &bme, const Sgp30 &sgp,
                                   const Scd30 &scd) {
  if (bme.nothing()) {
    ESP_LOGE("main", "BME280 sensor has problems.");
    return;
  }
  if (sgp.nothing()) {
    ESP_LOGE("main", "SGP30 sensor has problems.");
    return;
  }
  if (scd.nothing()) {
    ESP_LOGE("main", "SCD30 sensor has problems.");
    return;
  }
  struct tm utc;
  {
    time_t at = bme.get().at;
    gmtime_r(&at, &utc);
  }

  const size_t LENGTH = 1024;
  char *p = (char *)calloc(LENGTH + 1, sizeof(char));
  if (p) {
    size_t i;
    i = 0;
    // first field is date and time
    i += strftime(&p[i], LENGTH - i, "%Y-%m-%dT%H:%M:%SZ", &utc);
    // 2nd field is temperature
    i += snprintf(&p[i], LENGTH - i, ", %6.2f", bme.get().temperature.value);
    // 3nd field is relative_humidity
    i += snprintf(&p[i], LENGTH - i, ", %6.2f",
                  bme.get().relative_humidity.value);
    // 4th field is pressure
    i += snprintf(&p[i], LENGTH - i, ", %7.2f", bme.get().pressure.value);
    // 5th field is TVOC
    i += snprintf(&p[i], LENGTH - i, ", %5d", sgp.get().tvoc.value);
    // 6th field is eCo2
    i += snprintf(&p[i], LENGTH - i, ", %5d", sgp.get().eCo2.value);
    // 7th field is TVOC baseline
    i += snprintf(&p[i], LENGTH - i, ", %5d", sgp.get().tvoc_baseline.value);
    // 8th field is eCo2 baseline
    i += snprintf(&p[i], LENGTH - i, ", %5d", sgp.get().eCo2_baseline.value);
    // 9th field is co2
    i += snprintf(&p[i], LENGTH - i, ", %5d", scd.get().co2.value);
    // 10th field is temperature
    i += snprintf(&p[i], LENGTH - i, ", %6.2f", scd.get().temperature.value);
    // 11th field is relative_humidity
    i += snprintf(&p[i], LENGTH - i, ", %6.2f",
                  scd.get().relative_humidity.value);

    ESP_LOGD("main", "%s", p);

    // write to file
    size_t size = system_properties.data_logging_file.println(p);
    system_properties.data_logging_file.flush();
    ESP_LOGD("main", "wrote size:%u", size);
  } else {
    ESP_LOGE("main", "memory allocation error");
  }

  free(p);
}

//
//
//
static void write_header_to_log_file(File &file) {
  const size_t LENGTH = 1024;
  char *p = (char *)calloc(LENGTH + 1, sizeof(char));
  size_t i;
  i = 0;
  // first field is date and time
  i += snprintf(&p[i], LENGTH - i, "%s", "datetime");
  // 2nd field is temperature
  i += snprintf(&p[i], LENGTH - i, ", %s", "temperature[C]");
  // 3nd field is relative_humidity
  i += snprintf(&p[i], LENGTH - i, ", %s", "humidity[%RH]");
  // 4th field is pressure
  i += snprintf(&p[i], LENGTH - i, ", %s", "pressure[hPa]");
  // 5th field is TVOC
  i += snprintf(&p[i], LENGTH - i, ", %s", "TVOC[ppb]");
  // 6th field is eCo2
  i += snprintf(&p[i], LENGTH - i, ", %s", "eCo2[ppm]");
  // 7th field is TVOC baseline
  i += snprintf(&p[i], LENGTH - i, ", %s", "TVOC baseline");
  // 8th field is eCo2 baseline
  i += snprintf(&p[i], LENGTH - i, ", %s", "eCo2 baseline");
  // 9th field is co2
  i += snprintf(&p[i], LENGTH - i, ", %s", "Co2[ppm]");
  // 10th field is temperature
  i += snprintf(&p[i], LENGTH - i, ", %s", "temperature[C]");
  // 11th field is relative_humidity
  i += snprintf(&p[i], LENGTH - i, ", %s", "humidity[%RH]");

  ESP_LOGD("main", "%s", p);

  // write to file
  size_t size = file.println(p);
  file.flush();
  ESP_LOGD("main", "wrote size:%u", size);

  free(p);
}

//
// callbacks
//

//
//
//
static void sendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result) {
  if (result == IOTHUB_CLIENT_CONFIRMATION_OK) {
    ESP_LOGD("main", "Send Confirmation Callback finished.");
  }
}

//
//
//
static void messageCallback(const char *payLoad, int size) {
  ESP_LOGI("main", "Message callback:%s", payLoad);
}

//
//
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
//
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
//
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
//
//
void btnAEvent(Event &e) {
  system_properties.screen.prev();

  auto bme280_sensed = system_properties.bme280.valuesWithSMA();
  auto sgp30_sensed = system_properties.sgp30.valuesWithSMA();
  auto scd30_sensed = system_properties.scd30.valuesWithSMA();
  system_properties.screen.update(system_properties.status, time(nullptr),
                                  bme280_sensed, sgp30_sensed, scd30_sensed);
}

//
//
//
void btnBEvent(Event &e) {
  system_properties.screen.home();

  auto bme280_sensed = system_properties.bme280.valuesWithSMA();
  auto sgp30_sensed = system_properties.sgp30.valuesWithSMA();
  auto scd30_sensed = system_properties.scd30.valuesWithSMA();
  system_properties.screen.update(system_properties.status, time(nullptr),
                                  bme280_sensed, sgp30_sensed, scd30_sensed);
}

//
//
//
void btnCEvent(Event &e) {
  system_properties.screen.next();

  auto bme280_sensed = system_properties.bme280.valuesWithSMA();
  auto sgp30_sensed = system_properties.sgp30.valuesWithSMA();
  auto scd30_sensed = system_properties.scd30.valuesWithSMA();
  system_properties.screen.update(system_properties.status, time(nullptr),
                                  bme280_sensed, sgp30_sensed, scd30_sensed);
}

//
//
//
void releaseEvent(Event &e) { system_properties.screen.releaseEvent(e); }

//
//
//
void setup() {
  //
  // initializing M5Stack and UART, I2C, Touch, RTC, etc. peripherals.
  //
  M5.begin(true, true, true, true);
  system_properties.pixels.begin();
  system_properties.pixels.setBrightness(64);
  for (int i = 0; i < system_properties.pixels.numPixels(); ++i) {
    system_properties.pixels.setPixelColor(i,
                                           Adafruit_NeoPixel::Color(0, 0, 0));
  }
  system_properties.pixels.show();
  //
  // initializing the local database
  //
  system_properties.localDatabase.beginDb();

  switch (SD.cardType()) {
  case CARD_MMC: /* fallthrough */
  case CARD_SD:  /* fallthrough */
  case CARD_SDHC: {
    File f = SD.open(header_log_file_name, FILE_WRITE);
    write_header_to_log_file(f);
    f.close();
    system_properties.data_logging_file =
        SD.open(data_log_file_name, FILE_APPEND);
    system_properties.status.is_data_logging_to_file = true;
    break;
  }
  case CARD_NONE: /* fallthrough */
  case CARD_UNKNOWN:
    system_properties.status.is_data_logging_to_file = false;
    break;
  }

  //
  // register the button hook
  //
  M5.BtnA.addHandler(btnAEvent, E_RELEASE);
  M5.BtnB.addHandler(btnBEvent, E_RELEASE);
  M5.BtnC.addHandler(btnCEvent, E_RELEASE);
  M5.background.addHandler(releaseEvent, E_RELEASE);

  //
  // initializing screen
  //
  system_properties.screen.begin();

  //
  // connect to Wifi network
  //
  Screen::lcd.println(F("Wifi Connecting..."));
  WiFi.begin(Credentials.wifi_ssid, Credentials.wifi_password);
  system_properties.status.has_WIFI_connection = false;
  for (int16_t n = 1; n <= NUM_OF_TRY_TO_WIFI_CONNECTION; n++) {
    if (WiFi.status() == WL_CONNECTED) {
      system_properties.status.has_WIFI_connection = true;
      break;
    } else {
      delay(500);
      Screen::lcd.print(F("."));
    }
  }
  if (system_properties.status.has_WIFI_connection) {
    Screen::lcd.println(F("Wifi connected"));
    Screen::lcd.println(F("IP address: "));
    Screen::lcd.println(WiFi.localIP());
  } else {
    Screen::lcd.println(F("Wifi is NOT connected... freestanding."));
    system_properties.status.is_freestanding_mode = true;
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

  //
  // initializing IotHub client
  //
  if (!system_properties.status.is_freestanding_mode) {
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
  // initializing sensor
  //
  {
    HasSensor bme = system_properties.bme280.begin(BME280_I2C_ADDRESS);
    HasSensor sgp = system_properties.sgp30.begin();
    HasSensor scd = system_properties.scd30.begin();
    do {
      if (bme == HasSensor::NoSensorFound) {
        Screen::lcd.print(F("BME280センサが見つかりません。\n"));
        delay(100);
        bme = system_properties.bme280.begin(BME280_I2C_ADDRESS);
      }
      if (sgp == HasSensor::NoSensorFound) {
        Screen::lcd.print(F("SGP30センサが見つかりません。\n"));
        delay(100);
        sgp = system_properties.sgp30.begin();
      }
      if (scd == HasSensor::NoSensorFound) {
        Screen::lcd.print(F("SCD30センサが見つかりません。\n"));
        delay(100);
        scd = system_properties.scd30.begin();
      }
    } while (!(bme == HasSensor::Ok && sgp == HasSensor::Ok &&
               scd == HasSensor::Ok));
    Screen::lcd.clear();
    /*
    system_properties.scd30.printSensorDetails();
    system_properties.sgp30.printSensorDetails();
    system_properties.bme280.printSensorDetails();
    */
    //
    system_properties.screen.update(system_properties.status, time(nullptr),
                                    Bme280(), Sgp30(), Scd30());
  }

  //
  // start up
  //
  system_properties.status.startup_epoch = clock();
}

//
//
//
static void periodical_push_message(const Bme280 &bme280_sensed,
                                    const Sgp30 &sgp30_sensed,
                                    const Scd30 &scd30_sensed) {
  JsonDocSets doc_sets = {};

  // BME280 sensor values.
  // Temperature, Relative Humidity, Pressure
  if (bme280_sensed.good()) {
    // calculate the Aboslute Humidity from Temperature and Relative Humidity
    MilligramPerCubicMetre absolute_humidity = calculateAbsoluteHumidity(
        bme280_sensed.get().temperature, bme280_sensed.get().relative_humidity);
    ESP_LOGI("main", "absolute humidity: %d", absolute_humidity.value);
    // set "Absolute Humidity" to the SGP30 sensor.
    if (!system_properties.sgp30.setHumidity(absolute_humidity)) {
      ESP_LOGE("main", "setHumidity error.");
    }
    IotHubClient::pushMessage(
        takeMessageFromJsonDocSets(mapToJson(doc_sets, bme280_sensed.get())));
  }
  doc_sets.message.clear();
  doc_sets.state.clear();
  // SGP30 sensor values.
  // eCo2, TVOC
  if (sgp30_sensed.good()) {
    IotHubClient::pushMessage(
        takeMessageFromJsonDocSets(mapToJson(doc_sets, sgp30_sensed.get())));
  }
  doc_sets.message.clear();
  doc_sets.state.clear();
  // SCD30 sensor values.
  // co2, Temperature, Relative Humidity
  if (scd30_sensed.good()) {
    IotHubClient::pushMessage(
        takeMessageFromJsonDocSets(mapToJson(doc_sets, scd30_sensed.get())));
  }
}

//
//
//
static void periodical_push_state() {
  JsonDocSets doc_sets = {};
  auto batt_state = System::getBatteryStatus();

  // get the "smoothed" SGP30 sensor values.
  // eCo2, TVOC
  Sgp30 sgp30_smoothed = system_properties.sgp30.valuesWithSMA();
  if (sgp30_smoothed.good()) {
    auto json =
        takeStateFromJsonDocSets(mapToJson(doc_sets, sgp30_smoothed.get()));
    char buf[10];
    snprintf(buf, 10, "%d%%", static_cast<int>(batt_state.percentage));
    json["batteryLevel"] = buf;
    IotHubClient::pushState(json);
  }
}

//
//
//
struct Measurements {
  time_t measured_at;
  Bme280 bme280_sensed;
  Sgp30 sgp30_sensed;
  Scd30 scd30_sensed;
};

static struct Measurements periodical_measurements() {
  time_t measured_at;
  time(&measured_at);
  struct tm utc;
  gmtime_r(&measured_at, &utc);

  if (system_properties.bme280.ready() &&
      utc.tm_sec % BME280_SENSING_EVERY_SECONDS == 0) {
    system_properties.bme280.measure(measured_at);
  }
  if (system_properties.sgp30.ready() &&
      utc.tm_sec % SGP30_SENSING_EVERY_SECONDS == 0) {
    system_properties.sgp30.measure(measured_at);
  }
  if (system_properties.scd30.ready() &&
      utc.tm_sec % SCD30_SENSING_EVERY_SECONDS == 0) {
    system_properties.scd30.measure(measured_at);
  }
  return {measured_at, system_properties.bme280.valuesWithSMA(),
          system_properties.sgp30.valuesWithSMA(),
          system_properties.scd30.valuesWithSMA()};
}

//
//
//
static void periodical_send_to_iothub(struct Measurements &m) {
  struct tm utc;
  gmtime_r(&m.measured_at, &utc);
  //
  bool allowToPushMessage =
      utc.tm_sec == 0 && utc.tm_min % IOTHUB_PUSH_MESSAGE_EVERY_MINUTES == 0;
  bool allowToPushState =
      utc.tm_sec == 0 && utc.tm_min % IOTHUB_PUSH_STATE_EVERY_MINUTES == 0;
  //
  if ((clock() - system_properties.status.startup_epoch) <=
      SUPPRESSION_TIME_OF_FIRST_PUSH) {
    // Discarding this measurements
    allowToPushMessage = false;
    allowToPushState = false;
  }
  //
  // insert to local database
  //
  if (allowToPushMessage) {
    write_data_to_log_file(m.bme280_sensed, m.sgp30_sensed, m.scd30_sensed);
    //
    if (m.bme280_sensed.good()) {
      system_properties.localDatabase.insert(m.bme280_sensed.get());
    }
    if (m.sgp30_sensed.good()) {
      system_properties.localDatabase.insert(m.sgp30_sensed.get());
    }
    if (m.scd30_sensed.good()) {
      system_properties.localDatabase.insert(m.scd30_sensed.get());
    }
  }
  //
  if (!system_properties.status.is_freestanding_mode) {
    if (allowToPushMessage) {
      periodical_push_message(m.bme280_sensed, m.sgp30_sensed, m.scd30_sensed);
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
  M5.update();
  delay(1);

  static clock_t before_clock = 0;
  clock_t now_clock = clock();

  if ((now_clock - before_clock) >= CLOCKS_PER_SEC) {
    auto m = periodical_measurements();
    uint32_t c = Adafruit_NeoPixel::Color(0, 0, 0);
    if (m.scd30_sensed.good()) {
      Ppm co2 = m.scd30_sensed.get().co2;
      if (0 <= co2.value && co2.value < 499) {
        // blue
        c = Adafruit_NeoPixel::Color(21, 21, 88);
      } else if (500 <= co2.value && co2.value < 899) {
        // bluegreen
        c = Adafruit_NeoPixel::Color(21, 88, 88);
      } else if (900 <= co2.value && co2.value < 1199) {
        // green
        c = Adafruit_NeoPixel::Color(0, 204, 0);
      } else if (1200 <= co2.value && co2.value < 1699) {
        // yellow
        c = Adafruit_NeoPixel::Color(204, 204, 0);
      } else if (1700 <= co2.value && co2.value < 2499) {
        // pink
        c = Adafruit_NeoPixel::Color(204, 0, 102);
      } else {
        // red
        c = Adafruit_NeoPixel::Color(204, 0, 0);
      }
      for (int i = 0; i < system_properties.pixels.numPixels(); ++i) {
        system_properties.pixels.setPixelColor(i, c);
      }
      system_properties.pixels.show();
    }
    system_properties.screen.update(system_properties.status, m.measured_at,
                                    m.bme280_sensed, m.sgp30_sensed,
                                    m.scd30_sensed);
    periodical_send_to_iothub(m);
    before_clock = now_clock;
  }
}
