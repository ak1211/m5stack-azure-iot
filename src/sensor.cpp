// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "sensor.hpp"
#include <cmath>

constexpr static const char *TAG = "SensorModule";

//
// Bosch BME280 Humidity and Pressure Sensor
//
//
void Sensor<Bme280>::printSensorDetails() {
  Adafruit_Sensor *temperature = bme280.getTemperatureSensor();
  Adafruit_Sensor *pressure = bme280.getPressureSensor();
  Adafruit_Sensor *humidity = bme280.getHumiditySensor();
  if (temperature && pressure && humidity) {
    temperature->printSensorDetails();
    pressure->printSensorDetails();
    humidity->printSensorDetails();
  } else {
    ESP_LOGE(TAG, "BME280 sensor has problems.");
  }
}
//
HasSensor Sensor<Bme280>::begin(std::time_t now, uint8_t i2c_address) {
  startup_time = now;
  if (!bme280.begin(i2c_address)) {
    return HasSensor::NoSensorFound;
  }
  // weather monitoring
  // forced mode, 1x temperature / 1x humidity / 1x pressure oversampling,
  // filter off
  bme280.setSampling(Adafruit_BME280::MODE_FORCED,
                     Adafruit_BME280::SAMPLING_X1, // temperature
                     Adafruit_BME280::SAMPLING_X1, // pressure
                     Adafruit_BME280::SAMPLING_X1, // humidity
                     Adafruit_BME280::FILTER_OFF);
  initialized = true;
  return HasSensor::Ok;
}
//
double Sensor<Bme280>::uptime(std::time_t now) {
  return difftime(now, startup_time);
}
//
bool Sensor<Bme280>::readyToRead(std::time_t now) {
  return active() && (difftime(now, last_measured_at) >= INTERVAL);
}
//
Bme280 Sensor<Bme280>::read(std::time_t measured_at) {
  if (!active()) {
    ESP_LOGE(TAG, "BME280 sensor inactived.");
    return Bme280();
  }
  bme280.takeForcedMeasurement();

  float temperature = bme280.readTemperature();
  if (!std::isfinite(temperature)) {
    ESP_LOGE(TAG, "BME280 sensor: temperature is not finite.");
    return Bme280();
  }
  float pressure = bme280.readPressure();
  if (!std::isfinite(pressure)) {
    ESP_LOGE(TAG, "BME280 sensor: pressure is not finite.");
    return Bme280();
  }
  float humidity = bme280.readHumidity();
  if (!std::isfinite(humidity)) {
    ESP_LOGE(TAG, "BME280 sensor: humidity is not finite.");
    return Bme280();
  }
  //
  DegC temp = DegC(temperature);
  HPa pres = HPa(pressure / 100.0f); // pascal to hecto-pascal
  PcRH humi = PcRH(humidity);
  //
  last_measured_at = measured_at;
  sma_temperature.push_back(temp.value);
  sma_relative_humidity.push_back(humi.value);
  sma_pressure.push_back(pres.value);
  return Bme280({
      .sensor_descriptor = getSensorDescriptor(),
      .at = measured_at,
      .temperature = temp,
      .relative_humidity = humi,
      .pressure = pres,
  });
}
//
Bme280 Sensor<Bme280>::calculateSMA() {
  if (sma_temperature.ready() && sma_relative_humidity.ready() &&
      sma_pressure.ready()) {
    return Bme280({
        .sensor_descriptor = getSensorDescriptor(),
        .at = last_measured_at,
        .temperature = DegC(sma_temperature.calculate()),
        .relative_humidity = PcRH(sma_relative_humidity.calculate()),
        .pressure = HPa(sma_pressure.calculate()),
    });
  } else {
    return Bme280();
  }
}

//
// Sensirion SGP30: Air Quality Sensor
//
void Sensor<Sgp30>::printSensorDetails() {
  Serial.printf("SGP30 serial number is [0x%x, 0x%x, 0x%x]\n",
                sgp30.serialnumber[0], sgp30.serialnumber[1],
                sgp30.serialnumber[2]);
}
//
HasSensor Sensor<Sgp30>::begin(std::time_t now,
                               std::time_t eco2_base_measured_at,      //
                               MeasuredValues<BaselineECo2> eco2_base, //
                               std::time_t tvoc_base_measured_at,      //
                               MeasuredValues<BaselineTotalVoc> tvoc_base) {
  startup_time = now;
  if (!sgp30.begin()) {
    return HasSensor::NoSensorFound;
  }
  if (eco2_base.good() && tvoc_base.good()) {
    std::time_t at = min(eco2_base_measured_at, tvoc_base_measured_at);
    double elapsed_time = difftime(now, at);
    // 7日間有効
    if (elapsed_time < 7.0f * 24.0f * 60.0f * 60.0f) {
      // baseline set to SGP30
      bool result =
          setIAQBaseline(eco2_base.get().value, tvoc_base.get().value);
      ESP_LOGD(TAG, "SGP30 setIAQBaseline result is %d.", result);
    } else {
      ESP_LOGD(TAG, "SGP30 built-in Automatic Baseline Correction algorithm.");
    }
  } else {
    ESP_LOGD(TAG, "SGP30 built-in Automatic Baseline Correction algorithm.");
  }
  initialized = true;
  return HasSensor::Ok;
}
//
double Sensor<Sgp30>::uptime(std::time_t now) {
  return difftime(now, startup_time);
}
//
bool Sensor<Sgp30>::readyToRead(std::time_t now) {
  return active() && (difftime(now, last_tvoc_eco2.at) >= INTERVAL);
}
//
Sgp30 Sensor<Sgp30>::read(std::time_t measured_at) {
  if (!active()) {
    ESP_LOGE(TAG, "SGP30 sensor inactived.");
    return Sgp30();
  }
  if (!sgp30.IAQmeasure()) {
    ESP_LOGE(TAG, "SGP30 sensing failed.");
    return Sgp30();
  }
  // 稼働時間が 12h　を超えた場合のみベースラインを取得する
  auto eco2_base = MeasuredValues<BaselineECo2>();
  auto tvoc_base = MeasuredValues<BaselineTotalVoc>();
  if (uptime(measured_at) >= 12.0f * 60.0f * 60.0f) {
    uint16_t eco2;
    uint16_t tvoc;
    if (!sgp30.getIAQBaseline(&eco2, &tvoc)) {
      ESP_LOGE(TAG, "SGP30 sensing failed.");
      return Sgp30();
    }
    eco2_base = MeasuredValues<BaselineECo2>(eco2);
    tvoc_base = MeasuredValues<BaselineTotalVoc>(tvoc);
  }

  // successfully
  sma_eCo2.push_back(sgp30.eCO2);
  sma_tvoc.push_back(sgp30.TVOC);
  last_tvoc_eco2 = {
      .sensor_descriptor = getSensorDescriptor(),
      .at = measured_at,
      .eCo2 = Ppm(sgp30.eCO2),
      .tvoc = Ppb(sgp30.TVOC),
      .eCo2_baseline = eco2_base,
      .tvoc_baseline = tvoc_base,
  };
  return Sgp30(last_tvoc_eco2);
}
//
Sgp30 Sensor<Sgp30>::calculateSMA() {
  if (sma_eCo2.ready() && sma_tvoc.ready()) {
    return Sgp30({
        .sensor_descriptor = last_tvoc_eco2.sensor_descriptor,
        .at = last_tvoc_eco2.at,
        .eCo2 = Ppm(sma_eCo2.calculate()),
        .tvoc = Ppb(sma_tvoc.calculate()),
        .eCo2_baseline = last_tvoc_eco2.eCo2_baseline,
        .tvoc_baseline = last_tvoc_eco2.tvoc_baseline,
    });
  } else {
    return Sgp30();
  }
}
//
bool Sensor<Sgp30>::setIAQBaseline(BaselineECo2 eco2_base,
                                   BaselineTotalVoc tvoc_base) {
  return sgp30.setIAQBaseline(eco2_base.value, tvoc_base.value);
}
//
bool Sensor<Sgp30>::setHumidity(MilligramPerCubicMetre absolute_humidity) {
  return sgp30.setHumidity(absolute_humidity.value);
}
//
MilligramPerCubicMetre calculateAbsoluteHumidity(DegC temperature,
                                                 PcRH humidity) {
  float absolute_humidity =
      216.7f *
      ((humidity.value / 100.0f) * 6.112f *
       exp((17.62f * temperature.value) / (243.12f + temperature.value)) /
       (273.15f + temperature.value));
  return MilligramPerCubicMetre(
      static_cast<uint32_t>(1000.0f * absolute_humidity));
}

//
// Sensirion SCD30: NDIR CO2 and Humidity Sensor
//
void Sensor<Scd30>::printSensorDetails() {
  Adafruit_Sensor *temperature = scd30.getTemperatureSensor();
  Adafruit_Sensor *humidity = scd30.getHumiditySensor();
  if (temperature && humidity) {
    temperature->printSensorDetails();
    humidity->printSensorDetails();
  } else {
    ESP_LOGE(TAG, "SCD30 sensor has problems.");
  }
}
//
HasSensor Sensor<Scd30>::begin(std::time_t now) {
  startup_time = now;
  if (!scd30.begin()) {
    return HasSensor::NoSensorFound;
  }
  initialized = true;
  return HasSensor::Ok;
}
//
double Sensor<Scd30>::uptime(std::time_t now) {
  return difftime(now, startup_time);
} //
bool Sensor<Scd30>::readyToRead(std::time_t now) {
  return active() && (difftime(now, last_measured_at) >= INTERVAL) &&
         scd30.dataReady();
}
//
Scd30 Sensor<Scd30>::read(std::time_t measured_at) {
  if (!active()) {
    ESP_LOGE(TAG, "SCD30 sensor inactived.");
    return Scd30();
  }
  if (!scd30.dataReady()) {
    ESP_LOGE(TAG, "SCD30 sensor is not ready.");
    return Scd30();
  }
  if (!scd30.read()) {
    ESP_LOGE(TAG, "SCD30 sensing failed.");
    return Scd30();
  }
  // successfully
  last_measured_at = measured_at;
  sma_co2.push_back(scd30.CO2);
  sma_temperature.push_back(scd30.temperature);
  sma_relative_humidity.push_back(scd30.relative_humidity);
  return Scd30({
      .sensor_descriptor = getSensorDescriptor(),
      .at = measured_at,
      .co2 = Ppm(scd30.CO2),
      .temperature = DegC(scd30.temperature),
      .relative_humidity = PcRH(scd30.relative_humidity),
  });
}
//
Scd30 Sensor<Scd30>::calculateSMA() {
  if (sma_co2.ready() && sma_temperature.ready() &&
      sma_relative_humidity.ready()) {
    return Scd30({
        .sensor_descriptor = getSensorDescriptor(),
        .at = last_measured_at,
        .co2 = Ppm(sma_co2.calculate()),
        .temperature = DegC(sma_temperature.calculate()),
        .relative_humidity = PcRH(sma_relative_humidity.calculate()),
    });
  } else {
    return Scd30();
  }
}
