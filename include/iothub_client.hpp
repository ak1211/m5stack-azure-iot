// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef IOTHUB_CLIENT_HPP
#define IOTHUB_CLIENT_HPP

#include "sensor.hpp"
#include <Arduinojson.h>
#include <Esp32MQTTClient.h>
#include <string>

// iso8601 format.
// "2021-02-11T00:56:00.000+00:00"
//  12345678901234567890123456789
struct Iso8601FormatField {
  char at[29 + 1];
};
static_assert(sizeof(Iso8601FormatField) == 30, "something went Wrong");

constexpr static size_t MESSAGE_MAX_LEN = JSON_STRING_SIZE(1024);

using IoTHubMessageJson = StaticJsonDocument<MESSAGE_MAX_LEN>;

inline IoTHubMessageJson &mapToJson(IoTHubMessageJson &output,
                                    const std::string &sensor_id,
                                    const TempHumiPres &input) {
  struct Iso8601FormatField at_field = {};
  struct tm utc;

  gmtime_r(&input.at, &utc);
  //
  strftime(at_field.at, sizeof(at_field), "%Y-%m-%dT%H:%M:%SZ", &utc);
  //
  output["sensorId"] = sensor_id;
  output["measuredAt"] = at_field.at;
  output["temperature"] = input.temperature.value;
  output["humidity"] = input.relative_humidity.value;
  output["pressure"] = input.pressure.value;
  //
  return output;
}

inline IoTHubMessageJson &mapToJson(IoTHubMessageJson &output,
                                    const std::string &sensor_id,
                                    const TvocEco2 &input) {
  struct Iso8601FormatField at_field = {};
  struct tm utc;

  gmtime_r(&input.at, &utc);
  //
  strftime(at_field.at, sizeof(at_field), "%Y-%m-%dT%H:%M:%SZ", &utc);
  //
  output["sensorId"] = sensor_id;
  output["measuredAt"] = at_field.at;
  output["tvoc"] = input.tvoc.value;
  output["eCo2"] = input.eCo2.value;
  if (input.tvoc_baseline.good()) {
    output["tvoc_baseline"] = input.tvoc_baseline.get().value;
  }
  if (input.eCo2_baseline.good()) {
    output["eCo2_baseline"] = input.eCo2_baseline.get().value;
  }
  //
  return output;
}

inline IoTHubMessageJson &mapToJson(IoTHubMessageJson &output,
                                    const std::string &sensor_id,
                                    const Co2TempHumi &input) {
  struct Iso8601FormatField at_field = {};
  struct tm utc;

  gmtime_r(&input.at, &utc);
  //
  strftime(at_field.at, sizeof(at_field), "%Y-%m-%dT%H:%M:%SZ", &utc);
  //
  output["sensorId"] = sensor_id;
  output["measuredAt"] = at_field.at;
  output["co2"] = input.co2.value;
  output["temperature"] = input.temperature.value;
  output["humidity"] = input.relative_humidity.value;
  //
  return output;
}

//
//
//
class IotHubClient {
public:
  IotHubClient();
  ~IotHubClient();
  //
  bool begin(const std::string &connstr);
  JsonDocument *pushMessage(JsonDocument &v);
  JsonDocument *pushState(JsonDocument &v);
  inline void update(bool hasDelay) { Esp32MQTTClient_Check(hasDelay); }

private:
  static uint32_t message_id;
  std::string connection_string;
};

#undef AT_FIELD_MAX_LENGTH

#endif // IOTHUB_CLIENT_HPP
