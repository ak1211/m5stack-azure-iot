// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "Sensor.hpp"
#include "Application.hpp"
#include <M5Unified.h>

#include <cmath>
#include <variant>

using namespace std::chrono;

//
// Bosch BME280 Humidity and Pressure Sensor
//
void Sensor::Bme280Device::printSensorDetails() {
  Adafruit_Sensor *temperature = bme280.getTemperatureSensor();
  Adafruit_Sensor *pressure = bme280.getPressureSensor();
  Adafruit_Sensor *humidity = bme280.getHumiditySensor();
  if (temperature && pressure && humidity) {
    temperature->printSensorDetails();
    pressure->printSensorDetails();
    humidity->printSensorDetails();
    return;
  }
  M5_LOGE("BME280 sensor has problems.");
}

//
bool Sensor::Bme280Device::init() {
  initialized = false;
  if (!bme280.begin(i2c_address, &two_wire)) {
    return false;
  }
  setSampling();
  initialized = true;
  return initialized;
}

//
bool Sensor::Bme280Device::readyToRead() {
  return available() && (steady_clock::now() - last_measured_at >= INTERVAL);
}

//
Sensor::MeasuredValue Sensor::Bme280Device::read() {
  if (!available()) {
    M5_LOGE("BME280 sensor inactived.");
    return std::monostate{};
  }
  bme280.takeForcedMeasurement();

  float humidity = bme280.readHumidity();
  float pressure = bme280.readPressure();
  float temperature = bme280.readTemperature();
  if (!std::isfinite(humidity)) {
    M5_LOGE("BME280 sensor: humidity is not finite.");
    goto error_exit;
  }
  if (!std::isfinite(pressure)) {
    M5_LOGE("BME280 sensor: pressure is not finite.");
    goto error_exit;
  }
  if (!std::isfinite(temperature)) {
    M5_LOGE("BME280 sensor: temperature is not finite.");
    goto error_exit;
  }
  //
  {
    auto t = round<CentiDegC>(DegC(temperature));
    auto rh = round<CentiRH>(PctRH(humidity));
    auto pa = round<DeciPa>(Pascal(pressure));
    // successfully
    last_measured_at = steady_clock::now();
    sma_temperature.push_back(t.count());
    sma_relative_humidity.push_back(rh.count());
    sma_pressure.push_back(pa.count());
    return Bme280({
        .sensor_descriptor = getSensorDescriptor(),
        .temperature = t,
        .relative_humidity = rh,
        .pressure = pa,
    });
  }

error_exit:
  if (bme280.init()) {
    setSampling();
    M5_LOGD("BME280 sensor: error to re-initialize.");
  }
  return std::monostate{};
}

//
Sensor::MeasuredValue Sensor::Bme280Device::calculateSMA() {
  if (sma_temperature.ready() && sma_relative_humidity.ready() &&
      sma_pressure.ready()) {
    return Bme280({
        .sensor_descriptor = getSensorDescriptor(),
        .temperature = CentiDegC(sma_temperature.calculate()),
        .relative_humidity = CentiRH(sma_relative_humidity.calculate()),
        .pressure = DeciPa(sma_pressure.calculate()),
    });
  } else {
    return std::monostate{};
  }
}

//
// Sensirion SGP30: Air Quality Sensor
//
void Sensor::Sgp30Device::printSensorDetails() {
  Serial.printf("SGP30 serial number is [0x%x, 0x%x, 0x%x]\n",
                sgp30.serialnumber[0], sgp30.serialnumber[1],
                sgp30.serialnumber[2]);
}
//
bool Sensor::Sgp30Device::init() {
  initialized = false;
  if (!sgp30.begin(&two_wire)) {
    return initialized;
  }
  initialized = true;
  return initialized;
}
//
bool Sensor::Sgp30Device::readyToRead() {
  return available() && (steady_clock::now() - last_measured_at >= INTERVAL);
}
//
Sensor::MeasuredValue Sensor::Sgp30Device::read() {
  if (!available()) {
    M5_LOGE("SGP30 sensor inactived.");
    return std::monostate{};
  }
  if (!sgp30.IAQmeasure()) {
    M5_LOGE("SGP30 sensing failed.");
    return std::monostate{};
  }
  // 稼働時間が 12hour　を超えている状態のときにベースラインを取得する
  constexpr auto half_day = seconds{12 * 60 * 60}; // 43200 seconds
  if (Application::uptime() > half_day) {
    if (uint16_t eco2, tvoc; sgp30.getIAQBaseline(&eco2, &tvoc)) {
      last_eco2_baseline = BaselineECo2(eco2);
      last_tvoc_baseline = BaselineTotalVoc(tvoc);
    }
  }

  // successfully
  last_measured_at = steady_clock::now();
  sma_eCo2.push_back(sgp30.eCO2);
  sma_tvoc.push_back(sgp30.TVOC);
  return Sgp30({
      .sensor_descriptor = getSensorDescriptor(),
      .eCo2 = Ppm(sgp30.eCO2),
      .tvoc = Ppb(sgp30.TVOC),
      .eCo2_baseline = last_eco2_baseline,
      .tvoc_baseline = last_tvoc_baseline,
  });
}

//
Sensor::MeasuredValue Sensor::Sgp30Device::calculateSMA() {
  if (sma_eCo2.ready() && sma_tvoc.ready()) {
    return Sgp30({
        .sensor_descriptor = getSensorDescriptor(),
        .eCo2 = Ppm(sma_eCo2.calculate()),
        .tvoc = Ppb(sma_tvoc.calculate()),
        .eCo2_baseline = last_eco2_baseline,
        .tvoc_baseline = last_tvoc_baseline,
    });
  } else {
    return std::monostate{};
  }
}

//
bool Sensor::Sgp30Device::setIAQBaseline(BaselineECo2 eco2_base,
                                         BaselineTotalVoc tvoc_base) {
  auto result = sgp30.setIAQBaseline(eco2_base.value, tvoc_base.value);
  if (result) {
    M5_LOGI("SGP30 setIAQBaseline success");
  } else {
    M5_LOGE("SGP30 setIAQBaseline failure");
  }
  return result;
}

//
bool Sensor::Sgp30Device::setHumidity(
    MilligramPerCubicMetre absolute_humidity) {
  return sgp30.setHumidity(absolute_humidity.value);
}

//
MilligramPerCubicMetre calculateAbsoluteHumidity(DegC temperature,
                                                 PctRH humidity) {
  const auto tCelsius = temperature.count();
  float absolute_humidity =
      216.7f * ((humidity.count() / 100.0f) * 6.112f *
                std::exp((17.62f * tCelsius) / (243.12f + tCelsius)) /
                (273.15f + tCelsius));
  return MilligramPerCubicMetre(
      static_cast<uint32_t>(1000.0f * absolute_humidity));
}

//
// Sensirion SCD30: NDIR CO2 and Temperature and Humidity Sensor
//
void Sensor::Scd30Device::printSensorDetails() {
  Adafruit_Sensor *temperature = scd30.getTemperatureSensor();
  Adafruit_Sensor *humidity = scd30.getHumiditySensor();
  if (temperature && humidity) {
    temperature->printSensorDetails();
    humidity->printSensorDetails();
  } else {
    M5_LOGE("SCD30 sensor has problems.");
  }
}

//
bool Sensor::Scd30Device::init() {
  initialized = false;
  if (!scd30.begin(97U, &two_wire)) {
    return initialized;
  }
  initialized = true;
  return initialized;
}

//
bool Sensor::Scd30Device::readyToRead() {
  return available() && (steady_clock::now() - last_measured_at >= INTERVAL) &&
         scd30.dataReady();
}
//
Sensor::MeasuredValue Sensor::Scd30Device::read() {
  if (!available()) {
    M5_LOGE("SCD30 sensor inactived.");
    return std::monostate{};
  }
  if (!scd30.dataReady()) {
    M5_LOGE("SCD30 sensor is not ready.");
    return std::monostate{};
  }
  if (!scd30.read()) {
    M5_LOGE("SCD30 sensing failed.");
    return std::monostate{};
  }

  float co2 = scd30.CO2;
  float temperature = scd30.temperature;
  float relative_humidity = scd30.relative_humidity;

  if (!std::isfinite(co2)) {
    M5_LOGE("SCD30 sensor: co2 is not finite.");
    scd30.reset();
    return std::monostate{};
  }
  if (!std::isfinite(temperature)) {
    M5_LOGE("SCD30 sensor: temperature is not finite.");
    scd30.reset();
    return std::monostate{};
  }
  if (!std::isfinite(relative_humidity)) {
    M5_LOGE("SCD30 sensor: relative humidity is not finite.");
    scd30.reset();
    return std::monostate{};
  }

  // successfully
  last_measured_at = steady_clock::now();
  CentiDegC tCelcius = CentiDegC(static_cast<int16_t>(100.0f * temperature));
  CentiRH mRH = CentiRH(static_cast<int16_t>(100.0f * relative_humidity));
  sma_co2.push_back(static_cast<uint16_t>(co2));
  sma_temperature.push_back(tCelcius.count());
  sma_relative_humidity.push_back(mRH.count());
  return Scd30({
      .sensor_descriptor = getSensorDescriptor(),
      .co2 = Ppm(static_cast<uint16_t>(co2)),
      .temperature = tCelcius,
      .relative_humidity = mRH,
  });
}

//
Sensor::MeasuredValue Sensor::Scd30Device::calculateSMA() {
  if (sma_co2.ready() && sma_temperature.ready() &&
      sma_relative_humidity.ready()) {
    return Scd30({
        .sensor_descriptor = getSensorDescriptor(),
        .co2 = Ppm(sma_co2.calculate()),
        .temperature = CentiDegC(sma_temperature.calculate()),
        .relative_humidity = CentiRH(sma_relative_humidity.calculate()),
    });
  } else {
    return std::monostate{};
  }
}

//
// Sensirion SCD41: PASens CO2 and Temperature and Humidity Sensor
//
void Sensor::Scd41Device::printSensorDetails() {
  uint16_t serial0;
  uint16_t serial1;
  uint16_t serial2;
  if (auto error = scd4x.getSerialNumber(serial0, serial1, serial2); error) {
    char errorMessage[256];
    M5_LOGE("Error trying to execute getSerialNumber(): ");
    errorToString(error, errorMessage, std::size(errorMessage));
    M5_LOGE("%s", errorMessage);
    return;
  }
  Serial.printf("SCD41 serial number is [0x%x, 0x%x, 0x%x]\n", serial0, serial1,
                serial2);
}

//
bool Sensor::Scd41Device::init() {

  initialized = false;
  scd4x.begin(two_wire);
  // stop potentially previously started measurement
  if (auto error = scd4x.stopPeriodicMeasurement(); error) {
    char errorMessage[256]{};
    errorToString(error, errorMessage, std::size(errorMessage));
    M5_LOGE("%s", errorMessage);
    return false;
  }
  // Start Measurement
  if (auto error = scd4x.startPeriodicMeasurement(); error) {
    char errorMessage[256]{};
    errorToString(error, errorMessage, std::size(errorMessage));
    M5_LOGE("%s", errorMessage);
    return false;
  }

  initialized = true;
  return initialized;
}

//
Sensor::Scd41Device::SensorStatus Sensor::Scd41Device::getSensorStatus() {
  bool dataReadyFlag{false};
  if (auto error = scd4x.getDataReadyFlag(dataReadyFlag); error) {
    char errorMessage[256];
    M5_LOGE("Error trying to execute getDataReadyStatus(): ");
    errorToString(error, errorMessage, std::size(errorMessage));
    M5_LOGE("%s", errorMessage);
    return SensorStatus::DataNotReady;
  }
  return dataReadyFlag ? SensorStatus::DataReady : SensorStatus::DataNotReady;
}

//
bool Sensor::Scd41Device::readyToRead() {
  return available() && (steady_clock::now() - last_measured_at >= INTERVAL) &&
         getSensorStatus() == SensorStatus::DataReady;
}

//
Sensor::MeasuredValue Sensor::Scd41Device::read() {
  if (!available()) {
    M5_LOGE("SCD41 sensor inactived.");
    return std::monostate{};
  }
  if (getSensorStatus() == SensorStatus::DataNotReady) {
    M5_LOGE("SCD41 sensor is not ready.");
    return std::monostate{};
  }
  uint16_t co2 = 0;
  float temperature = 0.0f;
  float relative_humidity = 0.0f;
  if (auto error = scd4x.readMeasurement(co2, temperature, relative_humidity);
      error) {
    char errorMessage[256];
    M5_LOGE("Error trying to execute readMeasurement(): ");
    errorToString(error, errorMessage, std::size(errorMessage));
    M5_LOGE("%s", errorMessage);
    return std::monostate{};
  } else if (co2 == 0) {
    M5_LOGE("Invalid sample detected, skipping.");
    return std::monostate{};
  }

  // successfully
  last_measured_at = steady_clock::now();
  CentiDegC tCelcius = CentiDegC(static_cast<int16_t>(100.0f * temperature));
  CentiRH mRH = CentiRH(static_cast<int16_t>(100.0f * relative_humidity));
  sma_co2.push_back(static_cast<uint16_t>(co2));
  sma_temperature.push_back(tCelcius.count());
  sma_relative_humidity.push_back(mRH.count());
  return Scd41({
      .sensor_descriptor = getSensorDescriptor(),
      .co2 = Ppm(static_cast<uint16_t>(co2)),
      .temperature = tCelcius,
      .relative_humidity = mRH,
  });
}

//
Sensor::MeasuredValue Sensor::Scd41Device::calculateSMA() {
  if (sma_co2.ready() && sma_temperature.ready() &&
      sma_relative_humidity.ready()) {
    return Scd41({
        .sensor_descriptor = getSensorDescriptor(),
        .co2 = Ppm(sma_co2.calculate()),
        .temperature = CentiDegC(sma_temperature.calculate()),
        .relative_humidity = CentiRH(sma_relative_humidity.calculate()),
    });
  } else {
    return std::monostate{};
  }
}

//
// M5Stack ENV.iii unit: Temperature and Humidity and Pressure Sensor
//
void Sensor::M5Env3Device::printSensorDetails() {}

//
bool Sensor::M5Env3Device::init() {
  initialized = false;
  if (!sht31.begin(&two_wire, ENV3_I2C_ADDRESS_SHT31, sda_pin, scl_pin,
                   two_wire.getClock())) {
    return false;
  }
  if (!qmp6988.begin(&two_wire, ENV3_I2C_ADDRESS_QMP6988, sda_pin, scl_pin,
                     two_wire.getClock())) {
    return false;
  }
  initialized = true;
  return true;
}

//
bool Sensor::M5Env3Device::readyToRead() {
  return available() && (steady_clock::now() - last_measured_at >= INTERVAL);
}

//
Sensor::MeasuredValue Sensor::M5Env3Device::read() {
  if (!available()) {
    M5_LOGE("M5Env3 sensor inactived.");
    return std::monostate{};
  }
  //
  if (sht31.update() == false) {
    sht31.begin(&two_wire, ENV3_I2C_ADDRESS_SHT31, sda_pin, scl_pin);
    M5_LOGD("SHT31 sensor: error to re-initialize.");
    return std::monostate{};
  }
  if (qmp6988.update() == false) {
    qmp6988.begin(&two_wire, ENV3_I2C_ADDRESS_QMP6988, sda_pin, scl_pin);
    M5_LOGD("QMP6988 sensor: error to re-initialize.");
    return std::monostate{};
  }
  //
  {
    auto t = round<CentiDegC>(DegC(sht31.cTemp));
    auto rh = round<CentiRH>(PctRH(sht31.humidity));
    auto pa = round<DeciPa>(Pascal(qmp6988.pressure));
    // successfully
    last_measured_at = steady_clock::now();
    sma_temperature.push_back(t.count());
    sma_relative_humidity.push_back(rh.count());
    sma_pressure.push_back(pa.count());
    return M5Env3({
        .sensor_descriptor = getSensorDescriptor(),
        .temperature = t,
        .relative_humidity = rh,
        .pressure = pa,
    });
  }
}

//
Sensor::MeasuredValue Sensor::M5Env3Device::calculateSMA() {
  if (sma_temperature.ready() && sma_relative_humidity.ready() &&
      sma_pressure.ready()) {
    return M5Env3({
        .sensor_descriptor = getSensorDescriptor(),
        .temperature = CentiDegC(sma_temperature.calculate()),
        .relative_humidity = CentiRH(sma_relative_humidity.calculate()),
        .pressure = DeciPa(sma_pressure.calculate()),
    });
  } else {
    return std::monostate{};
  }
}