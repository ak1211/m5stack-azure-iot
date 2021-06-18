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

template <class T> class Sensor {
public:
  SensorDescriptor getSensorDescriptor() = 0;
  void printSensorDetails() = 0;
  bool begin() = 0;
  bool active() = 0;
  bool readyToRead(std::time_t now) = 0;
  T read(std::time_t measured_at) = 0;
  T calculateSMA(std::time_t measured_at);
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
  constexpr static uint8_t INTERVAL = 5;
  constexpr static uint8_t SMA_PERIOD = 1 + 120 / INTERVAL; // 120 seconds
  //
  Sensor(SensorDescriptor custom_sensor_descriptor)
      : sensor_descriptor{custom_sensor_descriptor},
        initialized{false},
        last_measured_at{0} {}
  SensorDescriptor getSensorDescriptor() { return sensor_descriptor; }
  void printSensorDetails();
  bool begin(uint8_t i2c_address);
  bool active() { return initialized; }
  bool readyToRead(std::time_t now);
  Bme280 read(std::time_t measured_at);
  Bme280 calculateSMA(std::time_t measured_at);
  //
  inline void setSampling() {
    bme280.setSampling(Adafruit_BME280::MODE_NORMAL,
                       Adafruit_BME280::SAMPLING_X1, // temperature
                       Adafruit_BME280::SAMPLING_X1, // pressure
                       Adafruit_BME280::SAMPLING_X1, // humidity
                       Adafruit_BME280::FILTER_OFF,
                       Adafruit_BME280::STANDBY_MS_1000);
  }

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
  MeasuredValues<BaselineECo2> eCo2_baseline;
  MeasuredValues<BaselineTotalVoc> tvoc_baseline;
};
using Sgp30 = MeasuredValues<TvocEco2>;
template <> class Sensor<Sgp30> {
public:
  constexpr static uint8_t INTERVAL = 3;
  constexpr static uint8_t SMA_PERIOD = 1 + 120 / INTERVAL; // 120 seconds
  //
  Sensor(SensorDescriptor custom_sensor_descriptor)
      : sensor_descriptor{custom_sensor_descriptor},
        initialized{false},
        last_tvoc_eco2{} {}
  SensorDescriptor getSensorDescriptor() { return sensor_descriptor; }
  void printSensorDetails();
  bool begin(MeasuredValues<BaselineECo2> eco2_base,
             MeasuredValues<BaselineTotalVoc> tvoc_base);
  bool active() { return initialized; }
  bool readyToRead(std::time_t now);
  Sgp30 read(std::time_t measured_at);
  Sgp30 calculateSMA(std::time_t measured_at);
  //
  bool setIAQBaseline(BaselineECo2 eco2_base, BaselineTotalVoc tvoc_base);
  bool setHumidity(MilligramPerCubicMetre absolute_humidity);

private:
  SensorDescriptor sensor_descriptor;
  bool initialized;
  TvocEco2 last_tvoc_eco2;
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
  constexpr static uint8_t INTERVAL = 7;
  constexpr static uint8_t SMA_PERIOD = 1 + 120 / INTERVAL; // 120 seconds
  //
  Sensor(SensorDescriptor custom_sensor_descriptor)
      : sensor_descriptor{custom_sensor_descriptor},
        initialized{false},
        last_measured_at{0} {}
  SensorDescriptor getSensorDescriptor() { return sensor_descriptor; }
  void printSensorDetails();
  bool begin();
  double uptime(std::time_t now);
  bool active() { return initialized; }
  bool readyToRead(std::time_t now);
  Scd30 read(std::time_t measured_at);
  Scd30 calculateSMA(std::time_t measured_at);

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
