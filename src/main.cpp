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

#include "sensor.hpp"
#include "iothub_client.hpp"
#include "credentials.h"

//
// globals
//
static LGFX lcd;
const unsigned long background_color = 0x000000U;
const unsigned long message_text_color = 0xFFFFFFU;

static Sensor sensor(Credentials.sensor_id);
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
void display(const TempHumiPres &v)
{
  lcd.setCursor(0, 0);
  //
  lcd.setFont(&fonts::lgfxJapanGothic_32);
  // time zone offset UTC+9 = asia/tokyo
  time_t tm = v.at + 9 * 60 * 60;
  struct tm local;
  gmtime_r(&tm, &local);
  //
  lcd.printf("%4d-%02d-%02d ", 1900 + local.tm_year, 1 + local.tm_mon, local.tm_mday);
  lcd.printf("%02d:%02d:%02d\n", local.tm_hour, local.tm_min, local.tm_sec);
  //
  lcd.setFont(&fonts::lgfxJapanGothic_28);
  float bus_voltage = M5.Axp.GetVBusVoltage();
  float bus_current = M5.Axp.GetVBusCurrent();
  lcd.printf("VBus: %3.1fV %6.1fmA\n", bus_voltage, bus_current);
  //
  float batt_voltage = M5.Axp.GetBatVoltage();
  float batt_current = M5.Axp.GetBatCurrent();
  lcd.printf("Batt: %3.1fV %6.1fmA\n", batt_voltage, batt_current);
  //
  lcd.setFont(&fonts::lgfxJapanGothic_36);
  lcd.printf("温度 %7.1f ℃\n", v.temperature);
  lcd.printf("湿度 %7.1f ％\n", v.relative_humidity);
  lcd.printf("気圧 %7.1f hPa\n", v.pressure);
  //
  lcd.printf("messageId: %llu\n", IotHubClient::message_id);
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
  lcd.setFont(&fonts::lgfxJapanGothic_32);

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
    TempHumiPres *val;
    while ((val = sensor.begin()) == NULL)
    {
      lcd.print(F("BME280センサが見つかりません。\n"));
    }
    sensor.printSensorDetails();
    lcd.fillScreen(background_color);
    display(*val);
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
    time(&measured_at);
    TempHumiPres *sensed = sensor.sensing(measured_at);
    if (sensed)
    {
      display(*sensed);
      //
      struct tm utc;
      gmtime_r(&measured_at, &utc);
      time_t seconds = utc.tm_hour * 3600 + utc.tm_min * 60 + utc.tm_sec;
      if (seconds % PUSH_EVERY_N_SECONDS == 0)
      {
        IotHubClient::push(*sensed);
      }
    }

    IotHubClient::update(true);
  }
}
