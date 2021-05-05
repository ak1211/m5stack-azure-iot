// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef SENSOR_HPP
#define SENSOR_HPP

#include "moving_average.hpp"
#include "value_types.hpp"
#include <Adafruit_BME280.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_SGP30.h>
#include <Adafruit_Sensor.h>
#include <cstdint>
#include <ctime>
#include <string>

enum class HasSensor { Ok, NoSensorFound };

template <class T> class MeasuredValues {
public:
  MeasuredValues(T v) : _good{true}, value{v} {}
  MeasuredValues() : _good{false}, value{} {}
  bool good() const { return _good; }
  bool nothing() const { return !_good; }
  T get() const { return value; }

private:
  bool _good;
  T value;
};

template <class T> class Sensor {
public:
  SensorDescriptor getSensorDescriptor() = 0;
  void printSensorDetails() = 0;
  HasSensor begin() = 0;
  bool active() = 0;
  bool readyToRead(std::time_t now) = 0;
  T read(std::time_t measured_at) = 0;
  T calculateSMA();
};

//
// Bosch BME280 Humidity and Pressure Sensor
//
struct TempHumiPres {
  SensorDescriptor sensor_descriptor;
  std::time_t at;
  DegC temperature;
  PcRH relative_humidity;
  HPa pressure;
};
using Bme280 = MeasuredValues<TempHumiPres>;
template <> class Sensor<Bme280> {
public:
  constexpr static uint8_t INTERVAL = 10;                  // 10 seconds
  constexpr static uint8_t SMA_PERIOD = 1 + 60 / INTERVAL; // 60 minutes
  //
  Sensor(SensorDescriptor custom_sensor_descriptor)
      : sensor_descriptor{custom_sensor_descriptor},
        initialized{false},
        last_measured_at{0} {}
  SensorDescriptor getSensorDescriptor() { return sensor_descriptor; }
  void printSensorDetails();
  HasSensor begin(uint8_t i2c_address);
  bool active() { return initialized; }
  bool readyToRead(std::time_t now);
  Bme280 read(std::time_t measured_at);
  Bme280 calculateSMA();

private:
  SensorDescriptor sensor_descriptor;
  bool initialized;
  std::time_t last_measured_at;
  Adafruit_BME280 bme280;
  SimpleMovingAverage<SMA_PERIOD, float, double> sma_temperature;
  SimpleMovingAverage<SMA_PERIOD, float, double> sma_relative_humidity;
  SimpleMovingAverage<SMA_PERIOD, float, double> sma_pressure;
};

//
// Sensirion SGP30: Air Quality Sensor
//
struct TvocEco2 {
  SensorDescriptor sensor_descriptor;
  std::time_t at;
  Ppm eCo2;
  Ppb tvoc;
  BaselineECo2 eCo2_baseline;
  BaselineTotalVoc tvoc_baseline;
};
using Sgp30 = MeasuredValues<TvocEco2>;
template <> class Sensor<Sgp30> {
public:
  constexpr static uint8_t INTERVAL = 1;                   // 1 seconds
  constexpr static uint8_t SMA_PERIOD = 1 + 60 / INTERVAL; // 60 minutes
  //
  Sensor(SensorDescriptor custom_sensor_descriptor)
      : sensor_descriptor{custom_sensor_descriptor},
        initialized{false},
        last_measured_at{0},
        last_eCo2_baseline{0},
        last_tvoc_baseline{0} {}
  SensorDescriptor getSensorDescriptor() { return sensor_descriptor; }
  void printSensorDetails();
  HasSensor begin();
  bool active() { return initialized; }
  bool readyToRead(std::time_t now);
  Sgp30 read(std::time_t measured_at);
  Sgp30 calculateSMA();
  //
  bool setIAQBaseline(BaselineECo2 eco2_base, BaselineTotalVoc tvoc_base);
  bool setHumidity(MilligramPerCubicMetre absolute_humidity);

private:
  SensorDescriptor sensor_descriptor;
  bool initialized;
  std::time_t last_measured_at;
  BaselineECo2 last_eCo2_baseline;
  BaselineTotalVoc last_tvoc_baseline;
  Adafruit_SGP30 sgp30;
  SimpleMovingAverage<SMA_PERIOD, uint16_t, uint32_t> sma_eCo2;
  SimpleMovingAverage<SMA_PERIOD, uint16_t, uint32_t> sma_tvoc;
};
extern MilligramPerCubicMetre calculateAbsoluteHumidity(DegC temperature,
                                                        PcRH humidity);

//
// Sensirion SCD30: NDIR CO2 and Humidity Sensor
//
struct Co2TempHumi {
  SensorDescriptor sensor_descriptor;
  std::time_t at;
  Ppm co2;
  DegC temperature;
  PcRH relative_humidity;
};
using Scd30 = MeasuredValues<Co2TempHumi>;
template <> class Sensor<Scd30> {
public:
  constexpr static uint8_t INTERVAL = 20;                  // 20 seconds
  constexpr static uint8_t SMA_PERIOD = 1 + 60 / INTERVAL; // 60 minutes
  //
  Sensor(SensorDescriptor custom_sensor_descriptor)
      : sensor_descriptor{custom_sensor_descriptor},
        initialized{false},
        last_measured_at{0} {}
  SensorDescriptor getSensorDescriptor() { return sensor_descriptor; }
  void printSensorDetails();
  HasSensor begin();
  bool active() { return initialized; }
  bool readyToRead(std::time_t now);
  Scd30 read(std::time_t measured_at);
  Scd30 calculateSMA();

private:
  SensorDescriptor sensor_descriptor;
  bool initialized;
  std::time_t last_measured_at;
  Adafruit_SCD30 scd30;
  SimpleMovingAverage<SMA_PERIOD, uint16_t, uint32_t> sma_co2;
  SimpleMovingAverage<SMA_PERIOD, float, double> sma_temperature;
  SimpleMovingAverage<SMA_PERIOD, float, double> sma_relative_humidity;
};

#endif // SENSOR_HPP
