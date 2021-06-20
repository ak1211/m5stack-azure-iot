// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef IOTHUB_CLIENT_HPP
#define IOTHUB_CLIENT_HPP

#include "sensor.hpp"
#include "ticktack.hpp"
#include <Arduinojson.h>
#include <Esp32MQTTClient.h>
#include <string>

//
//
//
class IotHubClient {
public:
  constexpr static size_t MESSAGE_MAX_LEN = JSON_STRING_SIZE(1024);
  IotHubClient();
  ~IotHubClient();
  //
  bool begin(const std::string &connstr);
  inline void check(bool hasDelay) { Esp32MQTTClient_Check(hasDelay); }
  bool pushState(JsonDocument &v);
  bool pushMessage(JsonDocument &v);
  //
  bool pushTempHumiPres(const std::string &sensor_id,
                        const TempHumiPres &input);
  bool pushTvocEco2(const std::string &sensor_id, const TvocEco2 &input);
  bool pushCo2TempHumi(const std::string &sensor_id, const Co2TempHumi &input);

private:
  static uint32_t message_id;
  std::string connection_string;
};

#endif // IOTHUB_CLIENT_HPP
