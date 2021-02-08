// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include <M5Core2.h>
#include <Wifi.h>
#include <ArduinoOTA.h>

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

const char firstNtpServer[] = "time.cloudflare.com";
const char secondNtpServer[] = "ntp.jst.mfeed.ad.jp";
const int32_t asiaTokyoTimeOffset = 60 * 60 * 9; // UTC +09:00

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
void display(const MeasuredValues &v)
{
  lcd.setCursor(0, 0);
  //
  lcd.setFont(&fonts::lgfxJapanGothic_32);
  lcd.printf("%4d-%02d-%02d ", v.at.date.Year, v.at.date.Month, v.at.date.Date);
  lcd.printf("%02d:%02d:%02d\n", v.at.time.Hours, v.at.time.Minutes, v.at.time.Seconds);
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
  lcd.printf("温度 %7.1f ℃\n", v.temp_humi_pres.temperature);
  lcd.printf("湿度 %7.1f ％\n", v.temp_humi_pres.relative_humidity);
  lcd.printf("気圧 %7.1f hPa\n", v.temp_humi_pres.pressure);
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
  // set to local time and date from the NTP server
  //
  RTC_DateTypeDef rtcDate;
  RTC_TimeTypeDef rtcTime;
  {
    configTime(asiaTokyoTimeOffset, 0, firstNtpServer, secondNtpServer);
    struct tm timeInfo;
    if (!getLocalTime(&timeInfo))
    {
      ESP_LOGE("main", "getLocalTime() failed.");
    }
    else
    {
      rtcTime.Hours = timeInfo.tm_hour;
      rtcTime.Minutes = timeInfo.tm_min;
      rtcTime.Seconds = timeInfo.tm_sec;
      M5.Rtc.SetTime(&rtcTime);
      //
      rtcDate.WeekDay = timeInfo.tm_wday;
      rtcDate.Month = timeInfo.tm_mon + 1;
      rtcDate.Date = timeInfo.tm_mday;
      rtcDate.Year = timeInfo.tm_year + 1900;
      M5.Rtc.SetDate(&rtcDate);
    }
  }

  //
  // get time and date from RTC
  //
  M5.Rtc.GetDate(&rtcDate);
  M5.Rtc.GetTime(&rtcTime);

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
    display(MeasuredValues(*val, rtcDate, rtcTime, asiaTokyoTimeOffset));
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
    TempHumiPres *sensed = sensor.sensing();
    if (sensed)
    {
      RTC_DateTypeDef rtcDate;
      RTC_TimeTypeDef rtcTime;
      M5.Rtc.GetDate(&rtcDate);
      M5.Rtc.GetTime(&rtcTime);
      auto v = MeasuredValues(*sensed, rtcDate, rtcTime, asiaTokyoTimeOffset);
      display(v);
      //
      uint32_t duration = rtcTime.Hours * 3600 + rtcTime.Minutes * 60 + rtcTime.Seconds;
      if (duration % PUSH_EVERY_N_SECONDS == 0)
      {
        IotHubClient::push(v);
      }
    }

    IotHubClient::update(true);
  }
}
