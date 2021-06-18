// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "sensor.hpp"
#include "peripherals.hpp"
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
bool Sensor<Bme280>::begin(uint8_t i2c_address) {
  initialized = false;
  if (!bme280.begin(i2c_address)) {
    return initialized;
  }
  setSampling();
  initialized = true;
  return initialized;
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
  float pressure = bme280.readPressure();
  float humidity = bme280.readHumidity();
  if (!std::isfinite(temperature)) {
    ESP_LOGE(TAG, "BME280 sensor: temperature is not finite.");
    goto error;
  }
  if (!std::isfinite(pressure)) {
    ESP_LOGE(TAG, "BME280 sensor: pressure is not finite.");
    goto error;
  }
  if (!std::isfinite(humidity)) {
    ESP_LOGE(TAG, "BME280 sensor: humidity is not finite.");
    goto error;
  }
  //
  {
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

error:
  if (bme280.init()) {
    setSampling();
    ESP_LOGD(TAG, "BME280 sensor: succeessfully re-initialized.");
  }
  return Bme280();
}
//
Bme280 Sensor<Bme280>::calculateSMA(std::time_t measured_at) {
  if (sma_temperature.ready() && sma_relative_humidity.ready() &&
      sma_pressure.ready()) {
    return Bme280({
        .sensor_descriptor = getSensorDescriptor(),
        .at = measured_at,
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
bool Sensor<Sgp30>::begin(MeasuredValues<BaselineECo2> eco2_base,
                          MeasuredValues<BaselineTotalVoc> tvoc_base) {
  initialized = false;
  if (!sgp30.begin()) {
    return initialized;
  }
  if (eco2_base.good() && tvoc_base.good()) {
    // baseline set to SGP30
    bool result =
        setIAQBaseline(static_cast<BaselineECo2>(eco2_base.get().value),
                       static_cast<BaselineTotalVoc>(tvoc_base.get().value));
    if (result) {
      ESP_LOGI(TAG, "SGP30 setIAQBaseline success");
    } else {
      ESP_LOGE(TAG, "SGP30 setIAQBaseline failure");
    }
  } else {
    ESP_LOGI(TAG, "SGP30 built-in Automatic Baseline Correction algorithm.");
  }
  initialized = true;
  return initialized;
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
  // 稼働時間が 12h　を超えている状態のときにベースラインを取得する
  MeasuredValues<BaselineECo2> eco2_base{};
  MeasuredValues<BaselineTotalVoc> tvoc_base{};
  Peripherals &peri = Peripherals::getInstance();
  if (peri.ticktack.available()) {
    uint32_t uptime_seconds = peri.ticktack.uptimeSeconds();
    constexpr uint32_t half_day = 12 * 60 * 60; // 43200 seconds
    if (uptime_seconds > half_day) {
      uint16_t eco2;
      uint16_t tvoc;
      if (sgp30.getIAQBaseline(&eco2, &tvoc)) {
        ESP_LOGD(TAG, "SGP30 baseline sensing.");
        eco2_base =
            MeasuredValues<BaselineECo2>(static_cast<BaselineECo2>(eco2));
        tvoc_base = MeasuredValues<BaselineTotalVoc>(
            static_cast<BaselineTotalVoc>(tvoc));
      } else {
        ESP_LOGE(TAG, "SGP30 sensing failed.");
        return Sgp30();
      }
    }
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
Sgp30 Sensor<Sgp30>::calculateSMA(std::time_t measured_at) {
  if (sma_eCo2.ready() && sma_tvoc.ready()) {
    return Sgp30({
        .sensor_descriptor = last_tvoc_eco2.sensor_descriptor,
        .at = measured_at,
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
bool Sensor<Scd30>::begin() {
  initialized = false;
  if (!scd30.begin()) {
    return initialized;
  }
  initialized = true;
  return initialized;
}
//
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

  float co2 = scd30.CO2;
  float temperature = scd30.temperature;
  float relative_humidity = scd30.relative_humidity;

  if (!std::isfinite(co2)) {
    ESP_LOGE(TAG, "SCD30 sensor: co2 is not finite.");
    goto error;
  }
  if (!std::isfinite(temperature)) {
    ESP_LOGE(TAG, "SCD30 sensor: temperature is not finite.");
    goto error;
  }
  if (!std::isfinite(relative_humidity)) {
    ESP_LOGE(TAG, "SCD30 sensor: relative humidity is not finite.");
    goto error;
  }

  // successfully
  last_measured_at = measured_at;
  sma_co2.push_back(static_cast<uint16_t>(co2));
  sma_temperature.push_back(temperature);
  sma_relative_humidity.push_back(relative_humidity);
  return Scd30({
      .sensor_descriptor = getSensorDescriptor(),
      .at = measured_at,
      .co2 = Ppm(static_cast<uint16_t>(co2)),
      .temperature = DegC(temperature),
      .relative_humidity = PcRH(relative_humidity),
  });

error:
  ESP_LOGD(TAG, "reset SCD30 sensor.");
  scd30.reset();
  return Scd30();
}
//
Scd30 Sensor<Scd30>::calculateSMA(std::time_t measured_at) {
  if (sma_co2.ready() && sma_temperature.ready() &&
      sma_relative_humidity.ready()) {
    return Scd30({
        .sensor_descriptor = getSensorDescriptor(),
        .at = measured_at,
        .co2 = Ppm(sma_co2.calculate()),
        .temperature = DegC(sma_temperature.calculate()),
        .relative_humidity = PcRH(sma_relative_humidity.calculate()),
    });
  } else {
    return Scd30();
  }
}
