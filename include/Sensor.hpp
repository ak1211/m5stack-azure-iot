// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "SimpleMovingAverage.hpp"
#include "value_types.hpp"
#include <Adafruit_BME280.h>
#include <Adafruit_SCD30.h>
#include <Adafruit_SGP30.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_Sensor.h>
#include <QMP6988.h>
#include <SensirionI2CScd4x.h>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <variant>

namespace Sensor {
//
// 各センサーの測定値
//
struct Bme280;
struct Sgp30;
struct Scd30;
struct Scd41;
struct M5Env3;
using MeasuredValue =
    std::variant<std::monostate, Bme280, Sgp30, Scd30, Scd41, M5Env3>;

// Value Objects
struct Bme280 {
  SensorDescriptor sensor_descriptor;
  CentiDegC temperature;
  CentiRH relative_humidity;
  DeciPa pressure;
  bool operator==(const Bme280 &other) const noexcept {
    return (sensor_descriptor == other.sensor_descriptor &&
            temperature == other.temperature &&
            relative_humidity == other.relative_humidity &&
            pressure == other.pressure);
  }
  bool operator!=(const Bme280 &other) const noexcept {
    return !(*this == other);
  }
};
struct Sgp30 {
  SensorDescriptor sensor_descriptor;
  Ppm eCo2;
  Ppb tvoc;
  std::optional<BaselineECo2> eCo2_baseline;
  std::optional<BaselineTotalVoc> tvoc_baseline;
  bool operator==(const Sgp30 &other) const noexcept {
    return (sensor_descriptor == other.sensor_descriptor &&
            eCo2 == other.eCo2 && tvoc == other.tvoc &&
            eCo2_baseline == other.eCo2_baseline &&
            tvoc_baseline == other.tvoc_baseline);
  }
  bool operator!=(const Sgp30 &other) const noexcept {
    return !(*this == other);
  }
};
struct Scd30 {
  SensorDescriptor sensor_descriptor;
  Ppm co2;
  CentiDegC temperature;
  CentiRH relative_humidity;
  bool operator==(const Scd30 &other) const noexcept {
    return (sensor_descriptor == other.sensor_descriptor && co2 == other.co2 &&
            temperature == other.temperature &&
            relative_humidity == other.relative_humidity);
  }
  bool operator!=(const Scd30 &other) const noexcept {
    return !(*this == other);
  }
};
struct Scd41 {
  SensorDescriptor sensor_descriptor;
  Ppm co2;
  CentiDegC temperature;
  CentiRH relative_humidity;
  bool operator==(const Scd41 &other) const noexcept {
    return (sensor_descriptor == other.sensor_descriptor && co2 == other.co2 &&
            temperature == other.temperature &&
            relative_humidity == other.relative_humidity);
  }
  bool operator!=(const Scd41 &other) const noexcept {
    return !(*this == other);
  }
};
struct M5Env3 {
  SensorDescriptor sensor_descriptor;
  CentiDegC temperature;
  CentiRH relative_humidity;
  DeciPa pressure;
  bool operator==(const M5Env3 &other) const noexcept {
    return (sensor_descriptor == other.sensor_descriptor &&
            temperature == other.temperature &&
            relative_humidity == other.relative_humidity &&
            pressure == other.pressure);
  }
  bool operator!=(const M5Env3 &other) const noexcept {
    return !(*this == other);
  }
};

//
// device driver
//
struct Device {
  virtual SensorDescriptor getSensorDescriptor() const noexcept = 0;
  virtual void printSensorDetails() = 0;
  virtual bool init() = 0;
  virtual bool active() const noexcept = 0;
  virtual bool readyToRead() noexcept = 0;
  virtual MeasuredValue read() = 0;
  virtual MeasuredValue calculateSMA() noexcept = 0;
};

// Bosch BME280: Temperature and Humidity and Pressure Sensor
class Bme280Device : public Device {
  constexpr static auto INTERVAL = std::chrono::seconds{12};
  constexpr static auto SMA_PERIOD =
      std::chrono::seconds{61} / INTERVAL; // reads per min
  //
  const SensorDescriptor sensor_descriptor;
  const uint8_t i2c_address;
  bool initialized{false};
  std::chrono::steady_clock::time_point last_measured_at{};
  Adafruit_BME280 bme280;
  SimpleMovingAverage<SMA_PERIOD, CentiDegC::rep, int32_t> sma_temperature{};
  SimpleMovingAverage<SMA_PERIOD, CentiRH::rep, int32_t>
      sma_relative_humidity{};
  SimpleMovingAverage<SMA_PERIOD, DeciPa::rep, int32_t> sma_pressure{};

public:
  Bme280Device(const SensorDescriptor &custom_sensor_descriptor,
               uint8_t i2c_address)
      : sensor_descriptor{custom_sensor_descriptor},
        i2c_address{i2c_address},
        initialized{false},
        last_measured_at{} {}
  // delte copy constructor / copy assignment operator
  Bme280Device(Bme280Device &&) = delete;
  Bme280Device &operator=(const Bme280Device &) = delete;
  //
  SensorDescriptor getSensorDescriptor() const noexcept override {
    return sensor_descriptor;
  }
  void printSensorDetails() override;
  bool init() override;
  bool active() const noexcept override { return initialized; }
  bool readyToRead() noexcept override;
  MeasuredValue read() override;
  MeasuredValue calculateSMA() noexcept override;
  //
  void setSampling() noexcept {
    bme280.setSampling(Adafruit_BME280::MODE_NORMAL,
                       Adafruit_BME280::SAMPLING_X1, // temperature
                       Adafruit_BME280::SAMPLING_X1, // pressure
                       Adafruit_BME280::SAMPLING_X1, // humidity
                       Adafruit_BME280::FILTER_OFF,
                       Adafruit_BME280::STANDBY_MS_1000);
  }
};

// Sensirion SGP30: Air Quality Sensor
class Sgp30Device : public Device {
  constexpr static auto INTERVAL = std::chrono::seconds{12};
  constexpr static auto SMA_PERIOD =
      std::chrono::seconds{61} / INTERVAL; // reads per min
  //
  const SensorDescriptor sensor_descriptor;
  bool initialized{false};
  std::chrono::steady_clock::time_point last_measured_at{};
  std::optional<BaselineECo2> last_eco2_baseline{std::nullopt};
  std::optional<BaselineTotalVoc> last_tvoc_baseline{std::nullopt};
  Adafruit_SGP30 sgp30;
  SimpleMovingAverage<SMA_PERIOD, uint16_t, uint32_t> sma_eCo2{};
  SimpleMovingAverage<SMA_PERIOD, uint16_t, uint32_t> sma_tvoc{};

public:
  Sgp30Device(const SensorDescriptor &custom_sensor_descriptor)
      : sensor_descriptor{custom_sensor_descriptor},
        initialized{false},
        last_eco2_baseline{std::nullopt},
        last_tvoc_baseline{std::nullopt} {}
  // delte copy constructor / copy assignment operator
  Sgp30Device(Sgp30Device &&) = delete;
  Sgp30Device &operator=(const Sgp30Device &) = delete;
  //
  SensorDescriptor getSensorDescriptor() const noexcept override {
    return sensor_descriptor;
  }
  void printSensorDetails() override;
  bool init() override;
  bool active() const noexcept override { return initialized; }
  bool readyToRead() noexcept override;
  MeasuredValue read() override;
  MeasuredValue calculateSMA() noexcept override;
  //
  bool setIAQBaseline(BaselineECo2 eco2_base,
                      BaselineTotalVoc tvoc_base) noexcept;
  bool setHumidity(MilligramPerCubicMetre absolute_humidity) noexcept;
};

// Sensirion SCD30: NDIR CO2 and Temperature and Humidity Sensor
class Scd30Device : public Device {
  constexpr static auto INTERVAL = std::chrono::seconds{12};
  constexpr static auto SMA_PERIOD =
      std::chrono::seconds{61} / INTERVAL; // reads per min
  //
  const SensorDescriptor &sensor_descriptor;
  bool initialized{false};
  std::chrono::steady_clock::time_point last_measured_at{};
  Adafruit_SCD30 scd30;
  SimpleMovingAverage<SMA_PERIOD, uint16_t, uint32_t> sma_co2{};
  SimpleMovingAverage<SMA_PERIOD, int16_t, int32_t> sma_temperature{};
  SimpleMovingAverage<SMA_PERIOD, uint16_t, uint32_t> sma_relative_humidity{};

public:
  Scd30Device(const SensorDescriptor &custom_sensor_descriptor)
      : sensor_descriptor{custom_sensor_descriptor},
        initialized{false},
        last_measured_at{} {}
  // delte copy constructor / copy assignment operator
  Scd30Device(Scd30Device &&) = delete;
  Scd30Device &operator=(const Scd30Device &) = delete;
  //
  SensorDescriptor getSensorDescriptor() const noexcept override {
    return sensor_descriptor;
  }
  void printSensorDetails() override;
  bool init() override;
  bool active() const noexcept override { return initialized; }
  bool readyToRead() noexcept override;
  MeasuredValue read() override;
  MeasuredValue calculateSMA() noexcept override;
};

// Sensirion SCD41: PASens CO2 and Temperature and Humidity Sensor
class Scd41Device : public Device {
  constexpr static auto INTERVAL = std::chrono::seconds{12};
  constexpr static auto SMA_PERIOD =
      std::chrono::seconds{61} / INTERVAL; // reads per min
  //
  const SensorDescriptor &sensor_descriptor;
  bool initialized{false};
  std::chrono::steady_clock::time_point last_measured_at{};
  SensirionI2CScd4x scd4x;
  SimpleMovingAverage<SMA_PERIOD, uint16_t, uint32_t> sma_co2{};
  SimpleMovingAverage<SMA_PERIOD, int16_t, int32_t> sma_temperature{};
  SimpleMovingAverage<SMA_PERIOD, uint16_t, uint32_t> sma_relative_humidity{};

public:
  Scd41Device(const SensorDescriptor &custom_sensor_descriptor)
      : sensor_descriptor{custom_sensor_descriptor},
        initialized{false},
        last_measured_at{} {}
  // delte copy constructor / copy assignment operator
  Scd41Device(Scd41Device &&) = delete;
  Scd41Device &operator=(const Scd41Device &) = delete;
  //
  SensorDescriptor getSensorDescriptor() const noexcept override {
    return sensor_descriptor;
  }
  void printSensorDetails() override;
  bool init() override;
  bool active() const noexcept override { return initialized; }
  bool readyToRead() noexcept override;
  enum class SensorStatus { DataNotReady, DataReady };
  SensorStatus getSensorStatus() noexcept;
  MeasuredValue read() override;
  MeasuredValue calculateSMA() noexcept override;
};

// M5Stack ENV.iii unit: Temperature and Humidity and Pressure Sensor
class M5Env3Device : public Device {
  constexpr static auto INTERVAL = std::chrono::seconds{12};
  constexpr static auto SMA_PERIOD =
      std::chrono::seconds{61} / INTERVAL; // reads per min
  constexpr static auto ENV3_I2C_ADDRESS_SHT31 = uint8_t{0x44};
  //
  const SensorDescriptor sensor_descriptor;
  bool initialized{false};
  std::chrono::steady_clock::time_point last_measured_at{};
  Adafruit_SHT31 sht31;
  QMP6988 qmp6988;
  SimpleMovingAverage<SMA_PERIOD, CentiDegC::rep, int32_t> sma_temperature{};
  SimpleMovingAverage<SMA_PERIOD, CentiRH::rep, int32_t>
      sma_relative_humidity{};
  SimpleMovingAverage<SMA_PERIOD, DeciPa::rep, int32_t> sma_pressure{};

public:
  M5Env3Device(const SensorDescriptor &custom_sensor_descriptor)
      : sensor_descriptor{custom_sensor_descriptor},
        initialized{false},
        last_measured_at{} {}
  // delte copy constructor / copy assignment operator
  M5Env3Device(M5Env3Device &&) = delete;
  M5Env3Device &operator=(const M5Env3Device &) = delete;
  //
  SensorDescriptor getSensorDescriptor() const noexcept override {
    return sensor_descriptor;
  }
  void printSensorDetails() override;
  bool init() override;
  bool active() const noexcept override { return initialized; }
  bool readyToRead() noexcept override;
  MeasuredValue read() override;
  MeasuredValue calculateSMA() noexcept override;
};

} // namespace Sensor