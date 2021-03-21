// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef SCD30_SENSOR_HPP
#define SCD30_SENSOR_HPP

#include <SparkFun_SCD30_Arduino_Library.h>
#include <cstdint>
#include <ctime>

namespace Scd30 {
static const uint8_t SMOOTHING_PERIOD_AS_2N = 5; // 2^5 =32
static const uint8_t SMOOTHING_PERIOD = 1 << SMOOTHING_PERIOD_AS_2N;
//
//
//
struct Co2TempHumi {
  const char *sensor_id;
  time_t at;
  uint16_t co2;            // ppm
  float temperature;       // degree celsius
  float relative_humidity; // %RH
};
//
//
//
inline uint32_t calculateAbsoluteHumidity(float temperature, float humidity) {
  float absolute_humidity =
      216.7f * ((humidity / 100.0f) * 6.112f *
                exp((17.62f * temperature) / (243.12f + temperature)) /
                (273.15f + temperature));
  return static_cast<uint32_t>(1000.0f * absolute_humidity);
}
//
//
//
class Sensor {
public:
  static const size_t SENSOR_ID_MAX_LEN = 50;
  Sensor(const char *custom_sensor_id) : sensor_id("") {
    memset(const_cast<char *>(sensor_id), 0, SENSOR_ID_MAX_LEN + 1);
    strncpy(const_cast<char *>(sensor_id), custom_sensor_id, SENSOR_ID_MAX_LEN);
    scd30_healthy = false;
  }
  //
  bool begin() {
    scd30_healthy = scd30.begin();
    if (scd30_healthy) {
      initRing();
    }
    return scd30_healthy;
  }
  //
  Co2TempHumi *sensing(const time_t &measured_at);
  //
  const Co2TempHumi *getLatestCo2TempHumi() {
    return (healthy()) ? &latest : nullptr;
  }
  //
  const Co2TempHumi *getCo2TempHumiWithSmoothing() {
    return (healthy()) ? &smoothed : nullptr;
  }
  //
  void printSensorDetails() {
    if (!healthy()) {
      ESP_LOGE("main", "sensor has problems.");
      return;
    }
    Serial.printf("SCD30 sensor is working.\n");
  }
  //
  bool healthy() { return scd30_healthy; }

private:
  SCD30 scd30;
  bool scd30_healthy;
  //
  const char sensor_id[SENSOR_ID_MAX_LEN + 1];
  //
  uint16_t co2_ring[SMOOTHING_PERIOD];      // ppm
  float temperature_ring[SMOOTHING_PERIOD]; // degree celsius
  float humidity_ring[SMOOTHING_PERIOD];    // %RH
  uint8_t tail_of_ring;
  //
  void initRing() {
    for (int16_t i = 0; i < SMOOTHING_PERIOD; i++) {
      co2_ring[i] = 400;         // 400ppm
      temperature_ring[i] = 0.0; // 0 degree celsius
      humidity_ring[i] = 0.0;    // %RH
    }
    tail_of_ring = 0;
  }
  //
  void pushRing(uint16_t co2, float temperature, float humidity) {
    co2_ring[tail_of_ring] = co2;
    temperature_ring[tail_of_ring] = temperature;
    humidity_ring[tail_of_ring] = humidity;
    tail_of_ring = (tail_of_ring + 1) & (SMOOTHING_PERIOD - 1);
  }
  //
  uint16_t calculateSmoothing_uint16(const uint16_t *ring) {
    uint32_t acc = ring[0];
    for (int16_t i = 1; i < SMOOTHING_PERIOD; i++) {
      acc = acc + ring[i];
    }
    return static_cast<uint16_t>(acc >> SMOOTHING_PERIOD_AS_2N);
  }
  //
  float calculateSmoothing_float(const float *ring) {
    double acc = ring[0];
    for (int16_t i = 1; i < SMOOTHING_PERIOD; i++) {
      acc = acc + ring[i];
    }
    return static_cast<float>(ldexp(acc, -SMOOTHING_PERIOD_AS_2N));
  }
  //
  struct Co2TempHumi latest;
  struct Co2TempHumi smoothed;
};
}; // namespace Scd30

#endif // SCD30_SENSOR_HPP
