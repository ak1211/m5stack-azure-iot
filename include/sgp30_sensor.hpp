// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef SGP30_SENSOR_HPP
#define SGP30_SENSOR_HPP

#include <cstdint>
#include <ctime>
#include <Adafruit_SGP30.h>

namespace Sgp30
{
  static const uint8_t SMOOTHING_PERIOD_AS_2N = 6; // 2^6 = 64
  static const uint8_t SMOOTHING_PERIOD = 1 << SMOOTHING_PERIOD_AS_2N;
  //
  //
  //
  struct TvocEco2
  {
    const char *sensor_id;
    time_t at;
    uint16_t tvoc;          // ppb
    uint16_t eCo2;          // ppm
    uint16_t tvoc_baseline; // ppb
    uint16_t eCo2_baseline; // ppm
  };
  //
  //
  //
  inline uint32_t calculateAbsoluteHumidity(float temperature, float humidity)
  {
    float absolute_humidity = 216.7f *
                              ((humidity / 100.0f) * 6.112f *
                               exp((17.62f * temperature) / (243.12f + temperature)) /
                               (273.15f + temperature));
    return static_cast<uint32_t>(1000.0f * absolute_humidity);
  }
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
      sgp30_healthy = false;
    }
    //
    bool begin();
    //
    TvocEco2 *sensing(const time_t &measured_at);
    //
    const TvocEco2 *getLatestTvocEco2()
    {
      return (healthy()) ? &latest : nullptr;
    }
    //
    const TvocEco2 *getTvocEco2WithSmoothing()
    {
      return (healthy()) ? &smoothed : nullptr;
    }
    //
    void printSensorDetails()
    {
      if (!healthy())
      {
        ESP_LOGE("main", "sensor has problems.");
        return;
      }
      Serial.printf("SGP30 serial number is [0x%x, 0x%x, 0x%x]\n", sgp30.serialnumber[0], sgp30.serialnumber[1], sgp30.serialnumber[2]);
    }
    //
    bool healthy()
    {
      return sgp30_healthy;
    }
    //
    bool setIAQBaseline(uint16_t eco2_base, uint16_t tvoc_base)
    {
      return sgp30.setIAQBaseline(eco2_base, tvoc_base);
    }
    //
    bool setHumidity(uint32_t absolute_humidity)
    {
      return sgp30.setHumidity(absolute_humidity);
    }

  private:
    Adafruit_SGP30 sgp30;
    bool sgp30_healthy;
    //
    const char sensor_id[SENSOR_ID_MAX_LEN + 1];
    //
    uint16_t tvoc_ring[SMOOTHING_PERIOD]; // ppb
    uint16_t eCo2_ring[SMOOTHING_PERIOD]; // ppm
    uint8_t tail_of_ring;
    //
    void initRing()
    {
      for (int16_t i = 0; i < SMOOTHING_PERIOD; i++)
      {
        tvoc_ring[i] = 0;   //0ppb
        eCo2_ring[i] = 400; //400ppm
      }
      tail_of_ring = 0;
    }
    //
    void pushRing(uint16_t tvoc, uint16_t eCo2)
    {
      tvoc_ring[tail_of_ring] = tvoc;
      eCo2_ring[tail_of_ring] = eCo2;
      tail_of_ring = (tail_of_ring + 1) & (SMOOTHING_PERIOD - 1);
    }
    //
    uint16_t calculateSmoothing(const uint16_t *ring)
    {
      uint32_t acc = ring[0];
      for (int16_t i = 1; i < SMOOTHING_PERIOD; i++)
      {
        acc = acc + ring[i];
      }
      return static_cast<uint16_t>(acc >> SMOOTHING_PERIOD_AS_2N);
    }
    //
    struct TvocEco2 latest;
    struct TvocEco2 smoothed;
  };
}; // namespace Sgp30

#endif // SGP30_SENSOR_HPP
