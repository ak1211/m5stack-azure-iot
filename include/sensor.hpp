// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef SENSOR_HPP
#define SENSOR_HPP

#include <Arduino.h>
#include <cstdint>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

//
//
//
struct TempHumiPres
{
  const char *sensor_id;
  float temperature;       // degree celsius
  float relative_humidity; // %
  float pressure;          // hectopascal
};

//
//
//
class MeasuredValues
{
public:
  MeasuredValues(TempHumiPres v, RTC_DateTypeDef d, RTC_TimeTypeDef t, int32_t ofs)
  {
    temp_humi_pres = v;
    at.date = d;
    at.time = t;
    at.time_offset = ofs;
  }
  //
  struct
  {
    RTC_DateTypeDef date;
    RTC_TimeTypeDef time;
    int32_t time_offset;
  } at;
  TempHumiPres temp_humi_pres;

  //
  int iso8601Timestamp(char *buf, size_t bufsize) const
  {
    if (at.time_offset > 0)
    {
      div_t a = div(at.time_offset, 3600);
      return snprintf(buf, bufsize,
                      "%04d-%02d-%02d"
                      "T"
                      "%02d:%02d:%02d"
                      "+"
                      "%02d:%02d",
                      at.date.Year,
                      at.date.Month,
                      at.date.Date,
                      at.time.Hours,
                      at.time.Minutes,
                      at.time.Seconds,
                      a.quot,
                      a.rem);
    }
    else if (at.time_offset < 0)
    {
      div_t a = div(-at.time_offset, 3600);
      return snprintf(buf, bufsize,
                      "%04d-%02d-%02d"
                      "T"
                      "%02d:%02d:%02d"
                      "-"
                      "%02d:%02d",
                      at.date.Year,
                      at.date.Month,
                      at.date.Date,
                      at.time.Hours,
                      at.time.Minutes,
                      at.time.Seconds,
                      a.quot,
                      a.rem);
    }
    else
    {
      return snprintf(buf, bufsize,
                      "%04d-%02d-%02d"
                      "T"
                      "%02d:%02d:%02d"
                      "Z",
                      at.date.Year,
                      at.date.Month,
                      at.date.Date,
                      at.time.Hours,
                      at.time.Minutes,
                      at.time.Seconds);
    }
  }
};

//
//
//
class Sensor
{
public:
  static const size_t SENSOR_ID_MAX_LEN = 50;
  Sensor(const char *custom_sensor_id) : sensor_id("")
  {
    memset(const_cast<char *>(sensor_id), 0, SENSOR_ID_MAX_LEN + 1);
    strncpy(const_cast<char *>(sensor_id), custom_sensor_id, SENSOR_ID_MAX_LEN);
    bme280_temperature = bme280_pressure = bme280_humidity = NULL;
  }
  //
  TempHumiPres *begin(uint8_t i2c_address = 0x76);
  //
  TempHumiPres *sensing();
  //
  TempHumiPres *getLatestTempHumiPres()
  {
    return (healthy()) ? &latest : NULL;
  }
  //
  void printSensorDetails()
  {
    if (!healthy())
    {
      ESP_LOGE("main", "sensor has problems.");
      return;
    }
    //
    bme280_temperature->printSensorDetails();
    bme280_pressure->printSensorDetails();
    bme280_humidity->printSensorDetails();
  }
  //
  bool healthy()
  {
    return (bme280_temperature && bme280_pressure && bme280_humidity) ? true : false;
  }

private:
  Adafruit_BME280 bme280;
  Adafruit_Sensor *bme280_temperature;
  Adafruit_Sensor *bme280_pressure;
  Adafruit_Sensor *bme280_humidity;
  //
  const char sensor_id[SENSOR_ID_MAX_LEN + 1];
  struct TempHumiPres latest;
};

#endif // SENSOR_HPP
