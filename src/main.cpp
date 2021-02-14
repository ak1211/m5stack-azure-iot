// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include <M5Core2.h>
#include <ArduinoOTA.h>
#include <Wifi.h>
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
static LGFX lcd;
const unsigned long background_color = 0x000000U;
const unsigned long message_text_color = 0xFFFFFFU;

static Bme280::Sensor bme280_sensor(Credentials.sensor_id.bme280);
static Sgp30::Sensor sgp30_sensor(Credentials.sensor_id.sgp30);

static const uint32_t PUSH_EVERY_N_SECONDS = 1 * 60;

//
//
//
void releaseEvent(Event &e)
{
}

//
//
//
void display(const Bme280::TempHumiPres *bme, const Sgp30::TvocEco2 *sgp)
{
  // time zone offset UTC+9 = asia/tokyo
  time_t tm;
  if (bme)
    tm = bme->at + 9 * 60 * 60;
  else if (sgp)
    tm = sgp->at + 9 * 60 * 60;
  else
    return;
  struct tm local;
  gmtime_r(&tm, &local);
  //
  lcd.setCursor(0, 0);
  lcd.setFont(&fonts::lgfxJapanGothic_28);
  //
  lcd.printf("%4d-%02d-%02d ", 1900 + local.tm_year, 1 + local.tm_mon, local.tm_mday);
  lcd.printf("%02d:%02d:%02d\n", local.tm_hour, local.tm_min, local.tm_sec);
  //
  lcd.setFont(&fonts::lgfxJapanGothic_24);
  float bat_voltage = M5.Axp.GetBatVoltage();
  float bat_current = M5.Axp.GetBatCurrent();
  float full_voltage = 4.2;
  float shutdown_voltage = 3.1;
  float bat_level = (bat_voltage - shutdown_voltage) / (full_voltage - shutdown_voltage);
  lcd.printf("%3.0f%% ", 100.0 * bat_level);
  lcd.printf(" %3.1fV", bat_voltage);
  if (bat_current < 0.0)
  {
    lcd.printf(" %4.0fmA(DIS)\n", -bat_current);
  }
  else
  {
    lcd.printf(" %4.0fmA(CHG)\n", bat_current);
  }
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
  lcd.printf("messageId: %u\n", IotHubClient::message_id);
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
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    lcd.print(F("."));
  }
  lcd.println(F("Wifi Connected"));
  lcd.println(F("IP address: "));
  lcd.println(WiFi.localIP());

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
  IotHubClient::init();

  //
  // set to Real Time Clock
  //
  if (sntp_enabled())
  {
    ESP_LOGI("main", "sntp enabled.");
    //
    time_t tm_now, tm_ret;
    struct tm utc;
    tm_ret = time(&tm_now);
    ESP_LOGI("main", "tm_ret: %d", tm_ret);
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
    Bme280::TempHumiPres *bme;
    while ((bme = bme280_sensor.begin()) == NULL)
    {
      lcd.print(F("BME280センサが見つかりません。\n"));
    }
    bme280_sensor.printSensorDetails();
    lcd.fillScreen(background_color);
    //
    Sgp30::TvocEco2 *sgp;
    while ((sgp = sgp30_sensor.begin()) == NULL)
    {
      lcd.print(F("SGP30センサが見つかりません。\n"));
    }
    sgp30_sensor.printSensorDetails();
    //
    display(bme, sgp);
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

  static uint16_t count = 0;
  if (++count > 1000)
  {
    count = 0;
    //
    time_t measured_at;
    //
    time(&measured_at);
    bme280_sensor.sensing(measured_at);
    sgp30_sensor.sensing(measured_at);
    //
    const Bme280::TempHumiPres *bme280_sensed = bme280_sensor.getLatestTempHumiPres();
    const Sgp30::TvocEco2 *sgp30_smoothed = sgp30_sensor.getTvocEco2WithSmoothing();
    //
    struct tm utc;
    gmtime_r(&measured_at, &utc);
    time_t seconds = utc.tm_hour * 3600 + utc.tm_min * 60 + utc.tm_sec;
    if (seconds % PUSH_EVERY_N_SECONDS == 0)
    {
      StaticJsonDocument<MESSAGE_MAX_LEN> doc;
      if (bme280_sensed)
      {
        IotHubClient::push(mapToJson(doc, *bme280_sensed));
      }
      doc.clear();
      if (sgp30_smoothed)
      {
        IotHubClient::push(mapToJson(doc, *sgp30_smoothed));
      }
    }
    //
    display(bme280_sensed, sgp30_smoothed);
    IotHubClient::update(true);
  }
}
