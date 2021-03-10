// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include <M5Core2.h>
#include <SD.h>
#include <ArduinoOTA.h>
#include <Wifi.h>
#include <time.h>
#include "lwip/apps/sntp.h"

#define LGFX_M5STACK_CORE2
#include <LovyanGFX.hpp>

#include "bme280_sensor.hpp"
#include "sgp30_sensor.hpp"
#include "scd30_sensor.hpp"
#include "iothub_client.hpp"
#include "credentials.h"

//
// globals
//
static LGFX lcd;
const unsigned long background_color = 0x000000U;
const unsigned long message_text_color = 0xFFFFFFU;

struct SystemProperties
{
  Bme280::Sensor bme280;
  Sgp30::Sensor sgp30;
  Scd30::Sensor scd30;
  File data_logging_file;
  clock_t startup_epoch;
  bool has_WIFI_connection;
  bool is_freestanding_mode;
  bool is_data_logging_to_file;
};

static struct SystemProperties system_propaties = {
    .bme280 = Bme280::Sensor(Credentials.sensor_id.bme280),
    .sgp30 = Sgp30::Sensor(Credentials.sensor_id.sgp30),
    .scd30 = Scd30::Sensor(Credentials.sensor_id.scd30),
    .data_logging_file = File(),
    .startup_epoch = clock(),
    .has_WIFI_connection = false,
    .is_freestanding_mode = false,
    .is_data_logging_to_file = false,
};

static const char *data_log_file_name = "/data-logging.csv";
static const char *header_log_file_name = "/header-data-logging.csv";

static const uint16_t NUM_OF_TRY_TO_WIFI_CONNECTION = 50;

static const uint8_t IOTHUB_PUSH_MESSAGE_EVERY_MINUTES = 1; // 1 mimutes
static_assert(IOTHUB_PUSH_MESSAGE_EVERY_MINUTES < 60, "IOTHUB_PUSH_MESSAGE_EVERY_MINUTES is lesser than 60 minutes.");

static const uint8_t IOTHUB_PUSH_STATE_EVERY_MINUTES = 15; // 15 minutes
static_assert(IOTHUB_PUSH_STATE_EVERY_MINUTES < 60, "IOTHUB_PUSH_STATE_EVERY_MINUTES is lesser than 60 minutes.");

static const uint8_t BME280_SENSING_EVERY_SECONDS = 2; // 2 seconds
static_assert(BME280_SENSING_EVERY_SECONDS < 60, "BME280_SENSING_EVERY_SECONDS is lesser than 60 seconds.");

static const uint8_t SGP30_SENSING_EVERY_SECONDS = 1; // 1 seconds
static_assert(SGP30_SENSING_EVERY_SECONDS == 1, "SGP30_SENSING_EVERY_SECONDS is shoud be 1 seconds.");

static const uint8_t SCD30_SENSING_EVERY_SECONDS = 2; // 2 seconds
static_assert(SCD30_SENSING_EVERY_SECONDS < 60, "SCD30_SENSING_EVERY_SECONDS is lesser than 60 seconds.");

//
static void write_header_to_log_file(File &file);
//
static void sendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result);
static void messageCallback(const char *payLoad, int size);
static int deviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size);
static void connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason);
static void deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size);

//
//
//
void releaseEvent(Event &e)
{
}

//
//
//
struct Uptime
{
  uint16_t days;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
};

struct Uptime uptime()
{
  clock_t time_epoch = clock() - system_propaties.startup_epoch;
  uint32_t seconds = static_cast<uint32_t>(time_epoch / CLOCKS_PER_SEC);
  uint32_t minutes = seconds / 60;
  uint32_t hours = minutes / 60;
  uint32_t days = hours / 24;
  return {
      .days = static_cast<uint16_t>(days),
      .hours = static_cast<uint8_t>(hours % 24),
      .minutes = static_cast<uint8_t>(minutes % 60),
      .seconds = static_cast<uint8_t>(seconds % 60),
  };
}

//
//
//
struct BatteryStatus
{
  float voltage;
  float percentage;
  float current;
};

inline bool isBatteryCharging(struct BatteryStatus &stat)
{
  return (stat.current > 0.00f);
}

inline bool isBatteryDischarging(struct BatteryStatus &stat)
{
  return (stat.current < 0.00f);
}

struct BatteryStatus getBatteryStatus()
{
  float batt_v = M5.Axp.GetBatVoltage();
  float batt_full_v = 4.18f;
  float shutdown_v = 3.00f;
  float indicated_v = batt_v - shutdown_v;
  float fullscale_v = batt_full_v - shutdown_v;
  float batt_percentage = 100.00f * indicated_v / fullscale_v;
  float batt_current = M5.Axp.GetBatCurrent(); // mA
  return {
      .voltage = batt_v,
      .percentage = min(batt_percentage, 100.0f),
      .current = batt_current,
  };
}

//
//
//
void display(time_t time,
             const Bme280::TempHumiPres *bme,
             const Sgp30::TvocEco2 *sgp,
             const Scd30::Co2TempHumi *scd)
{
  // time zone offset UTC+9 = asia/tokyo
  time_t local_time = time + 9 * 60 * 60;
  struct tm local;
  gmtime_r(&local_time, &local);
  //
  lcd.setCursor(0, 0);
  lcd.setFont(&fonts::FreeSans9pt7b);
  if (system_propaties.has_WIFI_connection)
  {
    lcd.setTextColor(TFT_GREEN, background_color);
    lcd.printf("  Wifi");
  }
  else
  {
    lcd.setTextColor(TFT_RED, background_color);
    lcd.printf("NoWifi");
  }
  lcd.setTextColor(message_text_color, background_color);
  //
  if (system_propaties.is_freestanding_mode)
  {
    lcd.printf("/FREESTAND");
  }
  else
  {
    lcd.printf("/CONNECTED");
  }
  //
  {
    struct Uptime up = uptime();
    lcd.printf(" up %2d days, %2d:%2d\n", up.days, up.hours, up.minutes);
  }
  //
  auto batt_info = getBatteryStatus();
  char sign = ' ';
  if (isBatteryCharging(batt_info))
  {
    sign = '+';
  }
  else if (isBatteryDischarging(batt_info))
  {
    sign = '-';
  }
  else
  {
    sign = ' ';
  }
  lcd.printf("Batt %4.0f%% %4.2fV %c%5.3fA",
             batt_info.percentage,
             batt_info.voltage,
             sign,
             abs(batt_info.current / 1000.0f));
  lcd.print("\n");
  //
  {
    char date_time[50] = "";
    strftime(date_time, sizeof(date_time), "%Y-%m-%d %H:%M:%SJST", &local);
    lcd.setFont(&fonts::lgfxJapanGothic_20);
    lcd.printf("%s\n", date_time);
  }
  //
  lcd.setFont(&fonts::lgfxJapanGothic_20);
  if (bme)
  {
    lcd.printf("温度 %6.1f ℃\n", bme->temperature);
    lcd.printf("湿度 %6.1f ％\n", bme->relative_humidity);
    lcd.printf("気圧 %6.1f hPa\n", bme->pressure);
  }
  if (sgp)
  {
    lcd.printf("eCO2 %6d ppm\n", sgp->eCo2);
    lcd.printf("TVOC %6d ppb\n", sgp->tvoc);
  }
  if (scd)
  {
    lcd.printf("CO2 %6d ppm\n", scd->co2);
    lcd.printf("温度 %6.1f ℃\n", scd->temperature);
    lcd.printf("湿度 %6.1f ％\n", scd->relative_humidity);
  }
}

//
//
//
void setup()
{
  //
  // initializing M5Stack and UART, I2C, Touch, RTC, etc. peripherals.
  //
  M5.begin(true, true, true, true);

  switch (SD.cardType())
  {
  case CARD_MMC: /* fallthrough */
  case CARD_SD:  /* fallthrough */
  case CARD_SDHC:
  {
    File f = SD.open(header_log_file_name, FILE_WRITE);
    write_header_to_log_file(f);
    f.close();
    system_propaties.data_logging_file = SD.open(data_log_file_name, FILE_APPEND);
    system_propaties.is_data_logging_to_file = true;
    break;
  }
  case CARD_NONE: /* fallthrough */
  case CARD_UNKNOWN:
    system_propaties.is_data_logging_to_file = false;
    break;
  }

  //
  // register the button hook
  //
  M5.Buttons.addHandler(releaseEvent, E_RELEASE);

  //
  // initializing LovyanGFX
  //
  lcd.init();
  lcd.setTextColor(message_text_color, background_color);
  lcd.setCursor(0, 0);
  lcd.setFont(&fonts::lgfxJapanGothic_20);

  //
  // connect to Wifi network
  //
  lcd.println(F("Wifi Connecting..."));
  WiFi.begin(Credentials.wifi_ssid, Credentials.wifi_password);
  system_propaties.has_WIFI_connection = false;
  for (int16_t n = 1; n <= NUM_OF_TRY_TO_WIFI_CONNECTION; n++)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      system_propaties.has_WIFI_connection = true;
      break;
    }
    else
    {
      delay(500);
      lcd.print(F("."));
    }
  }
  if (system_propaties.has_WIFI_connection)
  {
    lcd.println(F("Wifi connected"));
    lcd.println(F("IP address: "));
    lcd.println(WiFi.localIP());
  }
  else
  {
    lcd.println(F("Wifi is NOT connected... freestanding."));
    system_propaties.is_freestanding_mode = true;
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

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() {
        Serial.println("\nEnd");
      })
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
  if (!system_propaties.is_freestanding_mode)
  {
    IotHubClient::init(sendConfirmationCallback,
                       messageCallback,
                       deviceMethodCallback,
                       deviceTwinCallback,
                       connectionStatusCallback);
  }

  //
  // set to Real Time Clock
  //
  if (sntp_enabled())
  {
    ESP_LOGI("main", "sntp enabled.");
    //
    time_t tm_now;
    struct tm utc;
    time(&tm_now);
    ESP_LOGI("main", "tm_now: %d", tm_now);
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
  }
  else
  {
    ESP_LOGI("main", "sntp disabled.");
  }

  //
  // get time and date from RTC
  //
  RTC_DateTypeDef rtcDate;
  RTC_TimeTypeDef rtcTime;
  M5.Rtc.GetDate(&rtcDate);
  M5.Rtc.GetTime(&rtcTime);
  ESP_LOGI("main", "RTC \"%04d-%02d-%02dT%02d:%02d:%02dZ\"",
           rtcDate.Year,
           rtcDate.Month,
           rtcDate.Date,
           rtcTime.Hours,
           rtcTime.Minutes,
           rtcTime.Seconds);

  //
  // initializing sensor
  //
  {
    bool bme = system_propaties.bme280.begin();
    bool sgp = system_propaties.sgp30.begin();
    bool scd = system_propaties.scd30.begin();
    do
    {
      if (!bme)
      {
        lcd.print(F("BME280センサが見つかりません。\n"));
        delay(100);
        bme = system_propaties.bme280.begin();
      }
      if (!sgp)
      {
        lcd.print(F("SGP30センサが見つかりません。\n"));
        delay(100);
        sgp = system_propaties.sgp30.begin();
      }
      if (!scd)
      {
        lcd.print(F("SCD30センサが見つかりません。\n"));
        delay(100);
        scd = system_propaties.scd30.begin();
      }
    } while (!(bme && sgp && scd));
    lcd.fillScreen(background_color);
    assert(bme);
    assert(sgp);
    assert(scd);
    system_propaties.scd30.printSensorDetails();
    system_propaties.sgp30.printSensorDetails();
    system_propaties.bme280.printSensorDetails();
    //
    time_t now = time(NULL);
    display(now, nullptr, nullptr, nullptr);
  }

  //
  // start up
  //
  system_propaties.startup_epoch = clock();
}

//
//
//
static void write_data_to_log_file(const Bme280::TempHumiPres *bme,
                                   const Sgp30::TvocEco2 *sgp,
                                   const Scd30::Co2TempHumi *scd)
{
  if (!bme)
  {
    ESP_LOGI("main", "BME280 sensor has problems.");
    return;
  }
  else if (!sgp)
  {
    ESP_LOGI("main", "SGP30 sensor has problems.");
    return;
  }
  else if (!scd)
  {
    ESP_LOGI("main", "SCD30 sensor has problems.");
    return;
  }
  struct tm utc;
  gmtime_r(&bme->at, &utc);

  const size_t LENGTH = 1024;
  char *p = (char *)calloc(LENGTH + 1, sizeof(char));
  if (p)
  {
    size_t i;
    i = 0;
    // first field is date and time
    i += strftime(&p[i], LENGTH - i, "%Y-%m-%dT%H:%M:%SZ", &utc);
    // 2nd field is temperature
    i += snprintf(&p[i], LENGTH - i, ", %6.2f", bme->temperature);
    // 3nd field is relative_humidity
    i += snprintf(&p[i], LENGTH - i, ", %6.2f", bme->relative_humidity);
    // 4th field is pressure
    i += snprintf(&p[i], LENGTH - i, ", %7.2f", bme->pressure);
    // 5th field is TVOC
    i += snprintf(&p[i], LENGTH - i, ", %5d", sgp->tvoc);
    // 6th field is eCo2
    i += snprintf(&p[i], LENGTH - i, ", %5d", sgp->eCo2);
    // 7th field is TVOC baseline
    i += snprintf(&p[i], LENGTH - i, ", %5d", sgp->tvoc_baseline);
    // 8th field is eCo2 baseline
    i += snprintf(&p[i], LENGTH - i, ", %5d", sgp->eCo2_baseline);
    // 9th field is co2
    i += snprintf(&p[i], LENGTH - i, ", %5d", scd->co2);
    // 10th field is temperature
    i += snprintf(&p[i], LENGTH - i, ", %6.2f", scd->temperature);
    // 11th field is relative_humidity
    i += snprintf(&p[i], LENGTH - i, ", %6.2f", scd->relative_humidity);

    ESP_LOGI("main", "%s", p);

    // write to file
    size_t size = system_propaties.data_logging_file.println(p);
    system_propaties.data_logging_file.flush();
    ESP_LOGI("main", "wrote size:%u", size);
  }
  else
  {
    ESP_LOGE("main", "memory allocation error");
  }

  free(p);
}

//
//
//
static void write_header_to_log_file(File &file)
{
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

  ESP_LOGI("main", "%s", p);

  // write to file
  size_t size = file.println(p);
  file.flush();
  ESP_LOGI("main", "wrote size:%u", size);

  free(p);
}

//
//
//
static void periodical_push_message(
    const Bme280::TempHumiPres *bme280_sensed,
    const Sgp30::TvocEco2 *sgp30_sensed,
    const Scd30::Co2TempHumi *scd30_sensed)
{
  JsonDocSets doc_sets = {};

  // BME280 sensor values.
  // Temperature, Relative Humidity, Pressure
  if (bme280_sensed)
  {
    // calculate the Aboslute Humidity from Temperature and Relative Humidity
    uint32_t absolute_humidity = Sgp30::calculateAbsoluteHumidity(bme280_sensed->temperature, bme280_sensed->relative_humidity);
    ESP_LOGI("main", "absolute humidity: %d", absolute_humidity);
    // set "Absolute Humidity" to the SGP30 sensor.
    if (!system_propaties.sgp30.setHumidity(absolute_humidity))
    {
      ESP_LOGE("main", "setHumidity error.");
    }
    IotHubClient::pushMessage(
        takeMessageFromJsonDocSets(
            mapToJson(doc_sets, *bme280_sensed)));
  }
  doc_sets.message.clear();
  doc_sets.state.clear();
  // SGP30 sensor values.
  // eCo2, TVOC
  if (sgp30_sensed)
  {
    IotHubClient::pushMessage(
        takeMessageFromJsonDocSets(
            mapToJson(doc_sets, *sgp30_sensed)));
  }
  doc_sets.message.clear();
  doc_sets.state.clear();
  // SCD30 sensor values.
  // co2, Temperature, Relative Humidity
  if (scd30_sensed)
  {
    IotHubClient::pushMessage(
        takeMessageFromJsonDocSets(
            mapToJson(doc_sets, *scd30_sensed)));
  }
  //
  write_data_to_log_file(bme280_sensed, sgp30_sensed, scd30_sensed);
}

//
//
//
static void periodical_push_state()
{
  JsonDocSets doc_sets = {};
  struct BatteryStatus batt_state = getBatteryStatus();

  // get the "smoothed" SGP30 sensor values.
  // eCo2, TVOC
  const Sgp30::TvocEco2 *sgp30_smoothed = system_propaties.sgp30.getTvocEco2WithSmoothing();
  if (sgp30_smoothed)
  {
    auto json = takeStateFromJsonDocSets(mapToJson(doc_sets, *sgp30_smoothed));
    char buf[10];
    snprintf(buf, 10, "%d%%", static_cast<int>(batt_state.percentage));
    json["batteryLevel"] = buf;
    IotHubClient::pushState(json);
  }
}

//
//
//
static void periodical_update()
{
  time_t measured_at;
  time(&measured_at);
  struct tm utc;
  gmtime_r(&measured_at, &utc);

  if (utc.tm_sec % BME280_SENSING_EVERY_SECONDS == 0)
  {
    system_propaties.bme280.sensing(measured_at);
  }
  if (utc.tm_sec % SGP30_SENSING_EVERY_SECONDS == 0)
  {
    system_propaties.sgp30.sensing(measured_at);
  }
  if (utc.tm_sec % SCD30_SENSING_EVERY_SECONDS == 0)
  {
    system_propaties.scd30.sensing(measured_at);
  }
  auto bme280_sensed = system_propaties.bme280.getLatestTempHumiPres();
  auto sgp30_sensed = system_propaties.sgp30.getTvocEco2WithSmoothing();
  auto scd30_sensed = system_propaties.scd30.getCo2TempHumiWithSmoothing();
  display(measured_at, bme280_sensed, sgp30_sensed, scd30_sensed);
  //
  if (!system_propaties.is_freestanding_mode)
  {
    if (utc.tm_sec == 0 && utc.tm_min % IOTHUB_PUSH_MESSAGE_EVERY_MINUTES == 0)
    {
      periodical_push_message(bme280_sensed, sgp30_sensed, scd30_sensed);
    }
    if (utc.tm_sec == 0 && utc.tm_min % IOTHUB_PUSH_STATE_EVERY_MINUTES == 0)
    {
      periodical_push_state();
    }
    IotHubClient::update(true);
  }
}

//
//
//
void loop()
{
  ArduinoOTA.handle();
  M5.update();
  delay(1);

  static clock_t before_clock = 0;
  clock_t now_clock = clock();

  if ((now_clock - before_clock) >= CLOCKS_PER_SEC)
  {
    periodical_update();
    before_clock = now_clock;
  }
}

//
//
//
static void sendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
  if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
  {
    ESP_LOGI("main", "Send Confirmation Callback finished.");
  }
}

//
//
//
static void messageCallback(const char *payLoad, int size)
{
  ESP_LOGI("main", "Message callback:%s", payLoad);
}

//
//
//
static int deviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size)
{
  ESP_LOGI("main", "Try to invoke method %s", methodName);
  const char *responseMessage = "\"Successfully invoke device method\"";
  int result = 200;

  if (strcmp(methodName, "calibration") == 0)
  {
    ESP_LOGI("main", "Start calibrate the sensor");
    if (!system_propaties.sgp30.begin())
    {
      responseMessage = "\"calibration failed\"";
      result = 503;
    }
  }
  else if (strcmp(methodName, "start") == 0)
  {
    ESP_LOGI("main", "Start sending temperature and humidity data");
    //    messageSending = true;
  }
  else if (strcmp(methodName, "stop") == 0)
  {
    ESP_LOGI("main", "Stop sending temperature and humidity data");
    //    messageSending = false;
  }
  else
  {
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
static void connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason)
{
  switch (reason)
  {
  case IOTHUB_CLIENT_CONNECTION_EXPIRED_SAS_TOKEN:
    // SASトークンの有効期限切れ。
    ESP_LOGI("main", "SAS token expired.");
    if (result == IOTHUB_CLIENT_CONNECTION_UNAUTHENTICATED)
    {
      //
      // Info: >>>Connection status: timeout
      // Info: >>>Re-connect.
      // Info: Initializing SNTP
      // assertion "Operating mode must not be set while SNTP client is running" failed: file "/home/runner/work/esp32-arduino-lib-builder/esp32-arduino-lib-builder/esp-idf/components/lwip/lwip/src/apps/sntp/sntp.c", line 600, function: sntp_setoperatingmode
      // abort() was called at PC 0x401215bf on core 1
      //
      // Esp32MQTTClient 側で再接続時に以上のログが出てabortするので,
      // この時点で SNTPを停止しておくことで abort を回避する。
      ESP_LOGI("main", "SAS token expired, stop the SNTP.");
      sntp_stop();
    }
    break;
  case IOTHUB_CLIENT_CONNECTION_DEVICE_DISABLED:
    ESP_LOGI("main", "IOTHUB_CLIENT_CONNECTION_DEVICE_DISABLED");
    break;
  case IOTHUB_CLIENT_CONNECTION_BAD_CREDENTIAL:
    ESP_LOGI("main", "IOTHUB_CLIENT_CONNECTION_BAD_CREDENTIAL");
    break;
  case IOTHUB_CLIENT_CONNECTION_RETRY_EXPIRED:
    ESP_LOGI("main", "IOTHUB_CLIENT_CONNECTION_RETRY_EXPIRED");
    break;
  case IOTHUB_CLIENT_CONNECTION_NO_NETWORK:
    ESP_LOGI("main", "IOTHUB_CLIENT_CONNECTION_NO_NETWORK");
    break;
  case IOTHUB_CLIENT_CONNECTION_COMMUNICATION_ERROR:
    ESP_LOGI("main", "IOTHUB_CLIENT_CONNECTION_COMMUNICATION_ERROR");
    break;
  case IOTHUB_CLIENT_CONNECTION_OK:
    ESP_LOGI("main", "IOTHUB_CLIENT_CONNECTION_OK");
    break;
  }
} //
//
//
static void deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size)
{
  switch (updateState)
  {
  case DEVICE_TWIN_UPDATE_COMPLETE:
    ESP_LOGI("main", "device_twin_update_complete");
    break;

  case DEVICE_TWIN_UPDATE_PARTIAL:
    ESP_LOGI("main", "device_twin_update_partial");
    break;
  }

  // Display Twin message.
  char *buff = (char *)calloc(size + 1, sizeof(char));
  if (!buff)
  {
    ESP_LOGE("main", "memory allocation error");
    return;
  }

  strncpy(buff, (const char *)payLoad, size);
  ESP_LOGI("main", "%s", buff);

  StaticJsonDocument<MESSAGE_MAX_LEN> json;
  DeserializationError error = deserializeJson(json, buff);
  if (error)
  {
    ESP_LOGE("main", "%s", error.f_str());
    return;
  }
  free(buff);
  /*
  //
  const char *updatedAt = json["reported"]["sgp30_baseline"]["updatedAt"];
  if (updatedAt)
  {
    ESP_LOGI("main", "%s", updatedAt);
    // set baseline
    uint16_t tvoc_baseline = json["reported"]["sgp30_baseline"]["tvoc"].as<uint16_t>();
    uint16_t eCo2_baseline = json["reported"]["sgp30_baseline"]["eCo2"].as<uint16_t>();
    ESP_LOGI("main", "eCo2:%d, TVOC:%d", eCo2_baseline, tvoc_baseline);
    int ret = sgp30.setIAQBaseline(eCo2_baseline, tvoc_baseline);
    ESP_LOGI("main", "setIAQBaseline():%d", ret);
  }
  */
}
