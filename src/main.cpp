// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include <M5Core2.h>
#include <ArduinoOTA.h>
#include <Wifi.h>
#include <time.h>
#include "lwip/apps/sntp.h"

#define LGFX_M5STACK_CORE2
#include <LovyanGFX.hpp>

#include "bme280_sensor.hpp"
#include "sgp30_sensor.hpp"
#include "iothub_client.hpp"
#include "credentials.h"

//
// globals
//
static const uint16_t NUM_OF_TRY_TO_WIFI_CONNECTION = 50;
static bool has_WIFI_connection = false;

static LGFX lcd;
const unsigned long background_color = 0x000000U;
const unsigned long message_text_color = 0xFFFFFFU;

static Bme280::Sensor bme280_sensor(Credentials.sensor_id.bme280);
static Sgp30::Sensor sgp30_sensor(Credentials.sensor_id.sgp30);

static const uint8_t IOTHUB_PUSH_MESSAGE_EVERY_MINUTES = 1; // 1 mimutes
static_assert(IOTHUB_PUSH_MESSAGE_EVERY_MINUTES < 60, "IOTHUB_PUSH_MESSAGE_EVERY_MINUTES is lesser than 60 minutes.");

static const uint8_t IOTHUB_PUSH_STATE_EVERY_MINUTES = 15; // 15 minutes
static_assert(IOTHUB_PUSH_STATE_EVERY_MINUTES < 60, "IOTHUB_PUSH_STATE_EVERY_MINUTES is lesser than 60 minutes.");

static const uint8_t SENSING_EVERY_SECONDS = 2; // 2 seconds
static_assert(SENSING_EVERY_SECONDS < 60, "SENSING_EVERY_SECONDS is lesser than 60 seconds.");

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
struct BatteryStatus
{
  float voltage;
  float percentage;
  float current;
  bool charging, discharging;
};

//
//
//
struct BatteryStatus get_battery_status()
{
  float batt_v = floor(10.0f * M5.Axp.GetBatVoltage()) / 10.0f;
  float full_v = 4.2f;
  float shutdown_v = 3.0f;
  float indicated_v = batt_v - shutdown_v;
  float fullscale_v = full_v - shutdown_v;
  float batt_current = M5.Axp.GetBatCurrent(); // mA
  bool charging, discharging;
  if (batt_current < 0.0f)
  {
    batt_current = -batt_current;
    charging = false;
    discharging = true;
  }
  else if (batt_current > 0.0f)
  {
    charging = true;
    discharging = false;
  }
  else
  {
    charging = false;
    discharging = false;
  }
  return {
      .voltage = batt_v,
      .percentage = 100.0f * indicated_v / fullscale_v,
      .current = batt_current,
      .charging = charging,
      .discharging = discharging,
  };
}

//
//
//
void display(time_t time, const Bme280::TempHumiPres *bme, const Sgp30::TvocEco2 *sgp)
{
  // time zone offset UTC+9 = asia/tokyo
  time_t local_time = time + 9 * 60 * 60;
  struct tm local;
  gmtime_r(&local_time, &local);
  //
  lcd.setCursor(0, 0);
  lcd.setFont(&fonts::lgfxJapanGothic_28);
  //
  lcd.printf("%4d-%02d-%02d ", 1900 + local.tm_year, 1 + local.tm_mon, local.tm_mday);
  lcd.printf("%02d:%02d:%02d\n", local.tm_hour, local.tm_min, local.tm_sec);
  //
  lcd.setFont(&fonts::lgfxJapanGothic_24);
  auto batt_info = get_battery_status();

  lcd.printf("%3.0f%% %3.1fV %4.0fmA %6s\n",
             batt_info.percentage,
             batt_info.voltage,
             batt_info.current,
             batt_info.charging ? "CHARGE" : batt_info.discharging ? "DISCHG"
                                                                   : "");
  //
  lcd.setFont(&fonts::lgfxJapanGothic_28);
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
  //
  if (has_WIFI_connection)
  {
    lcd.printf("messageId: %u\n", IotHubClient::message_id);
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
  has_WIFI_connection = false;
  for (int16_t n = 1; n <= NUM_OF_TRY_TO_WIFI_CONNECTION; n++)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      has_WIFI_connection = true;
      break;
    }
    else
    {
      delay(500);
      lcd.print(F("."));
    }
  }
  if (has_WIFI_connection)
  {
    lcd.println(F("Wifi connected"));
    lcd.println(F("IP address: "));
    lcd.println(WiFi.localIP());
  }
  else
  {
    lcd.println(F("Wifi is NOT connected"));
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
  if (has_WIFI_connection)
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
    Bme280::TempHumiPres *bme = bme280_sensor.begin();
    Sgp30::TvocEco2 *sgp = sgp30_sensor.begin();
    do
    {
      if (!bme)
      {
        lcd.print(F("BME280センサが見つかりません。\n"));
        delay(100);
        bme = bme280_sensor.begin();
      }
      if (!sgp)
      {
        lcd.print(F("SGP30センサが見つかりません。\n"));
        delay(100);
        sgp = sgp30_sensor.begin();
      }
    } while (!(bme && sgp));
    lcd.fillScreen(background_color);
    assert(bme);
    assert(sgp);
    sgp30_sensor.printSensorDetails();
    bme280_sensor.printSensorDetails();
    //
    time_t now = time(NULL);
    display(now, bme, sgp);
  }
}

//
//
//
static void periodical_sensing(const time_t &measured_at)
{
  bme280_sensor.sensing(measured_at);
  sgp30_sensor.sensing(measured_at);
}

//
//
//
static void periodical_push_message()
{
  JsonDocSets doc_sets = {};

  // get the "latest" BME280 sensor values.
  // Temperature, Relative Humidity, Pressure
  const Bme280::TempHumiPres *bme280_sensed = bme280_sensor.getLatestTempHumiPres();
  if (bme280_sensed)
  {
    // calculate the Aboslute Humidity from Temperature and Relative Humidity
    uint32_t absolute_humidity = Sgp30::calculateAbsoluteHumidity(bme280_sensed->temperature, bme280_sensed->relative_humidity);
    ESP_LOGI("main", "absolute humidity: %d", absolute_humidity);
    // set "Absolute Humidity" to the SGP30 sensor.
    if (!sgp30_sensor.setHumidity(absolute_humidity))
    {
      ESP_LOGE("main", "setHumidity error.");
    }
    IotHubClient::pushMessage(
        takeMessageFromJsonDocSets(
            mapToJson(doc_sets, *bme280_sensed)));
  }
  doc_sets.message.clear();
  doc_sets.state.clear();
  // get the "smoothed" SGP30 sensor values.
  // eCo2, TVOC
  const Sgp30::TvocEco2 *sgp30_smoothed = sgp30_sensor.getTvocEco2WithSmoothing();
  if (sgp30_smoothed)
  {
    IotHubClient::pushMessage(
        takeMessageFromJsonDocSets(
            mapToJson(doc_sets, *sgp30_smoothed)));
  }
}

//
//
//
static void periodical_push_state()
{
  JsonDocSets doc_sets = {};
  struct BatteryStatus batt_state = get_battery_status();

  // get the "smoothed" SGP30 sensor values.
  // eCo2, TVOC
  const Sgp30::TvocEco2 *sgp30_smoothed = sgp30_sensor.getTvocEco2WithSmoothing();
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

  if (utc.tm_sec % SENSING_EVERY_SECONDS == 0)
  {
    periodical_sensing(measured_at);
  }
  display(measured_at, bme280_sensor.getLatestTempHumiPres(), sgp30_sensor.getTvocEco2WithSmoothing());
  //
  if (has_WIFI_connection)
  {
    if (utc.tm_sec == 0 && utc.tm_min % IOTHUB_PUSH_MESSAGE_EVERY_MINUTES == 0)
    {
      periodical_push_message();
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

  if (strcmp(methodName, "start") == 0)
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
  ESP_LOGI("main", "%s", payLoad);

  StaticJsonDocument<MESSAGE_MAX_LEN> json;
  DeserializationError error = deserializeJson(json, payLoad);
  if (error)
  {
    ESP_LOGE("main", "%s", error.f_str());
    return;
  }
  //
  const char *updatedAt = json["reported"]["sgp30_baseline"]["updatedAt"];
  if (updatedAt)
  {
    ESP_LOGI("main", "%s", updatedAt);
    // set baseline
    uint16_t tvoc_baseline = json["reported"]["sgp30_baseline"]["tvoc"].as<uint16_t>();
    uint16_t eCo2_baseline = json["reported"]["sgp30_baseline"]["eCo2"].as<uint16_t>();
    ESP_LOGI("main", "eCo2:%d, TVOC:%d", eCo2_baseline, tvoc_baseline);
    int ret = sgp30_sensor.setIAQBaseline(eCo2_baseline, tvoc_baseline);
    ESP_LOGI("main", "setIAQBaseline():%d", ret);
  }
}
