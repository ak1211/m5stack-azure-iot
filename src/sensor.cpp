// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "Sensor.hpp"
#include "Application.hpp"
#include "Time.hpp"
#include <M5Core2.h>
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
  ESP_LOGE(MAIN, "BME280 sensor has problems.");
}
//
bool Sensor::Bme280Device::init() {
  initialized = false;
  if (!bme280.begin(i2c_address)) {
    return false;
  }
  setSampling();
  initialized = true;
  return initialized;
}
//
bool Sensor::Bme280Device::readyToRead() noexcept {
  return active() && (steady_clock::now() - last_measured_at >= INTERVAL);
}
//
Sensor::MeasuredValue Sensor::Bme280Device::read() {
  if (!active()) {
    ESP_LOGE(MAIN, "BME280 sensor inactived.");
    return std::monostate{};
  }
  bme280.takeForcedMeasurement();

  float humidity = bme280.readHumidity();
  float pressure = bme280.readPressure();
  float temperature = bme280.readTemperature();
  if (!std::isfinite(humidity)) {
    ESP_LOGE(MAIN, "BME280 sensor: humidity is not finite.");
    goto error;
  }
  if (!std::isfinite(pressure)) {
    ESP_LOGE(MAIN, "BME280 sensor: pressure is not finite.");
    goto error;
  }
  if (!std::isfinite(temperature)) {
    ESP_LOGE(MAIN, "BME280 sensor: temperature is not finite.");
    goto error;
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

error:
  if (bme280.init()) {
    setSampling();
    ESP_LOGD(MAIN, "BME280 sensor: error to re-initialize.");
  }
  return std::monostate{};
}
//
Sensor::MeasuredValue Sensor::Bme280Device::calculateSMA() noexcept {
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
  if (!sgp30.begin()) {
    return initialized;
  }
  initialized = true;
  return initialized;
}
//
bool Sensor::Sgp30Device::readyToRead() noexcept {
  return active() && (steady_clock::now() - last_measured_at >= INTERVAL);
}
//
Sensor::MeasuredValue Sensor::Sgp30Device::read() {
  if (!active()) {
    ESP_LOGE(MAIN, "SGP30 sensor inactived.");
    return std::monostate{};
  }
  if (!sgp30.IAQmeasure()) {
    ESP_LOGE(MAIN, "SGP30 sensing failed.");
    return std::monostate{};
  }
  // 稼働時間が 12hour　を超えている状態のときにベースラインを取得する
  constexpr auto half_day = seconds{12 * 60 * 60}; // 43200 seconds
  if (Time::uptime() > half_day) {
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
Sensor::MeasuredValue Sensor::Sgp30Device::calculateSMA() noexcept {
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
                                         BaselineTotalVoc tvoc_base) noexcept {
  auto result = sgp30.setIAQBaseline(eco2_base.value, tvoc_base.value);
  if (result) {
    ESP_LOGI(MAIN, "SGP30 setIAQBaseline success");
  } else {
    ESP_LOGE(MAIN, "SGP30 setIAQBaseline failure");
  }
  return result;
}
//
bool Sensor::Sgp30Device::setHumidity(
    MilligramPerCubicMetre absolute_humidity) noexcept {
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
    ESP_LOGE(MAIN, "SCD30 sensor has problems.");
  }
}
//
bool Sensor::Scd30Device::init() {
  initialized = false;
  if (!scd30.begin()) {
    return initialized;
  }
  initialized = true;
  return initialized;
}
//
bool Sensor::Scd30Device::readyToRead() noexcept {
  return active() && (steady_clock::now() - last_measured_at >= INTERVAL) &&
         scd30.dataReady();
}
//
Sensor::MeasuredValue Sensor::Scd30Device::read() {
  if (!active()) {
    ESP_LOGE(MAIN, "SCD30 sensor inactived.");
    return std::monostate{};
  }
  if (!scd30.dataReady()) {
    ESP_LOGE(MAIN, "SCD30 sensor is not ready.");
    return std::monostate{};
  }
  if (!scd30.read()) {
    ESP_LOGE(MAIN, "SCD30 sensing failed.");
    return std::monostate{};
  }

  float co2 = scd30.CO2;
  float temperature = scd30.temperature;
  float relative_humidity = scd30.relative_humidity;

  if (!std::isfinite(co2)) {
    ESP_LOGE(MAIN, "SCD30 sensor: co2 is not finite.");
    ESP_LOGD(MAIN, "reset SCD30 sensor.");
    scd30.reset();
    return std::monostate{};
  }
  if (!std::isfinite(temperature)) {
    ESP_LOGE(MAIN, "SCD30 sensor: temperature is not finite.");
    ESP_LOGD(MAIN, "reset SCD30 sensor.");
    scd30.reset();
    return std::monostate{};
  }
  if (!std::isfinite(relative_humidity)) {
    ESP_LOGE(MAIN, "SCD30 sensor: relative humidity is not finite.");
    ESP_LOGD(MAIN, "reset SCD30 sensor.");
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
Sensor::MeasuredValue Sensor::Scd30Device::calculateSMA() noexcept {
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
    ESP_LOGE(MAIN, "Error trying to execute getSerialNumber(): ");
    errorToString(error, errorMessage, std::size(errorMessage));
    ESP_LOGE(MAIN, "%s", errorMessage);
    return;
  }
  Serial.printf("SCD41 serial number is [0x%x, 0x%x, 0x%x]\n", serial0, serial1,
                serial2);
}
//
bool Sensor::Scd41Device::init() {
  uint16_t result = 0;

  initialized = false;
  scd4x.begin(Wire);
  // stop potentially previously started measurement
  if (result = scd4x.stopPeriodicMeasurement(); result) {
    ESP_LOGE(MAIN, "Error trying to execute stopPeriodicMeasurement(): ");
    goto error;
  }
  // Start Measurement
  if (auto error = scd4x.startPeriodicMeasurement(); error) {
    ESP_LOGE(MAIN, "Error trying to execute startPeriodicMeasurement(): ");
    goto error;
  }

  initialized = true;
  return initialized;
error:
  char errorMessage[256]{};
  errorToString(result, errorMessage, std::size(errorMessage));
  ESP_LOGE(MAIN, "%s", errorMessage);
  return false;
}
//
Sensor::Scd41Device::SensorStatus
Sensor::Scd41Device::getSensorStatus() noexcept {
  constexpr uint16_t BITMASK = 0b0000011111111111;
  uint16_t status = 0;
  auto error = scd4x.getDataReadyStatus(status);
  if (error) {
    char errorMessage[256];
    ESP_LOGE(MAIN, "Error trying to execute getDataReadyStatus(): ");
    errorToString(error, errorMessage, std::size(errorMessage));
    ESP_LOGE(MAIN, "%s", errorMessage);
    return SensorStatus::DataNotReady;
  }
  if (status & BITMASK == 0) {
    return SensorStatus::DataNotReady;
  } else {
    return SensorStatus::DataReady;
  }
}
//
bool Sensor::Scd41Device::readyToRead() noexcept {
  return active() && (steady_clock::now() - last_measured_at >= INTERVAL) &&
         getSensorStatus() == SensorStatus::DataReady;
}
//
Sensor::MeasuredValue Sensor::Scd41Device::read() {
  if (!active()) {
    ESP_LOGE(MAIN, "SCD41 sensor inactived.");
    return std::monostate{};
  }
  if (getSensorStatus() == SensorStatus::DataNotReady) {
    ESP_LOGE(MAIN, "SCD41 sensor is not ready.");
    return std::monostate{};
  }
  uint16_t co2 = 0;
  float temperature = 0.0f;
  float relative_humidity = 0.0f;
  if (auto error = scd4x.readMeasurement(co2, temperature, relative_humidity);
      error) {
    char errorMessage[256];
    ESP_LOGE(MAIN, "Error trying to execute readMeasurement(): ");
    errorToString(error, errorMessage, std::size(errorMessage));
    ESP_LOGE(MAIN, "%s", errorMessage);
    return std::monostate{};
  } else if (co2 == 0) {
    ESP_LOGE(MAIN, "Invalid sample detected, skipping.");
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
Sensor::MeasuredValue Sensor::Scd41Device::calculateSMA() noexcept {
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
  if (!sht31.begin(ENV3_I2C_ADDRESS_SHT31)) {
    return false;
  }
  if (sht31.isHeaterEnabled()) {
    ESP_LOGE(MAIN, "M5Env3-SHT31 sensor heater ENABLED.");
  } else {
    ESP_LOGE(MAIN, "M5Env3-SHT31 sensor heater DISABLED.");
  }
  if (qmp6988.init() == 0) {
    return false;
  }
  initialized = true;
  return initialized;
}
//
bool Sensor::M5Env3Device::readyToRead() noexcept {
  return active() && (steady_clock::now() - last_measured_at >= INTERVAL);
}
//
Sensor::MeasuredValue Sensor::M5Env3Device::read() {
  if (!active()) {
    ESP_LOGE(MAIN, "M5Env3 sensor inactived.");
    return std::monostate{};
  }

  //
  float temperature = 0, humidity = 0;
  float pressure = 0;
  sht31.readBoth(&temperature, &humidity);
  if (!std::isfinite(temperature)) {
    ESP_LOGE(MAIN, "SHT31 sensor: temperature is not finite.");
    goto error_sht31;
  }
  if (!std::isfinite(humidity)) {
    ESP_LOGE(MAIN, "SHT31 sensor: humidity is not finite.");
    goto error_sht31;
  }
  pressure = qmp6988.calcPressure();
  if (pressure == 0.0f) {
    goto error_qmp6988;
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
    return M5Env3({
        .sensor_descriptor = getSensorDescriptor(),
        .temperature = t,
        .relative_humidity = rh,
        .pressure = pa,
    });
  }

error_sht31:
  if (sht31.begin(ENV3_I2C_ADDRESS_SHT31)) {
    ESP_LOGD(MAIN, "SHT31 sensor: error to re-initialize.");
  }
  return std::monostate{};

error_qmp6988:
  if (qmp6988.init()) {
    ESP_LOGD(MAIN, "QMP6988 sensor: error to re-initialize.");
  }
  return std::monostate{};
}
//
Sensor::MeasuredValue Sensor::M5Env3Device::calculateSMA() noexcept {
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