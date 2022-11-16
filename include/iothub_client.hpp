// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once

#include "sensor.hpp"
#include "ticktack.hpp"
#include <Arduinojson.h>
#include <array>
#include <mqtt_client.h>
#include <string>

// Azure IoT SDK for C includes
extern "C" {
#include <az_core.h>
#include <az_iot.h>
}
#include <AzIoTSasToken.h>

//
//
//
class IotHubClient final {
  static uint32_t message_id;
  //
  static az_iot_hub_client client;
  static az_iot_hub_client_options client_options;
  std::string config_iothub_fqdn;
  std::string config_device_id;
  std::string config_device_key;
  //
  constexpr static int MQTT_QOS1{1};
  constexpr static int DO_NOT_RETAIN_MSG{0};
  constexpr static int mqtt_port{AZ_IOT_DEFAULT_MQTT_CONNECT_PORT};
  static esp_mqtt_client_handle_t mqtt_client;
  std::string mqtt_broker_uri;
  std::string mqtt_client_id;
  std::string mqtt_username;
  std::array<char, 128> telemetry_topic;
  //
  static std::array<uint8_t, 200> mqtt_password;
  static std::array<char, 128> incoming_data;
  //
  constexpr static int SAS_TOKEN_DURATION_IN_MINUTES{60};
  std::array<uint8_t, 256> sas_signature_buffer;
  std::optional<AzIoTSasToken> optSasToken;
  //
  static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);
  //
  bool initializeIoTHubClient();
  bool initializeMqttClient();

public:
  constexpr static std::size_t MESSAGE_MAX_LEN = JSON_STRING_SIZE(1024);
  //
  bool begin(std::string_view iothub_fqdn, std::string_view device_id,
             std::string_view device_key);
  void check();
  bool pushMessage(JsonDocument &v);
  //
  bool pushTempHumiPres(const std::string &sensor_id,
                        const TempHumiPres &input);
  bool pushTvocEco2(const std::string &sensor_id, const TvocEco2 &input);
  bool pushCo2TempHumi(const std::string &sensor_id, const Co2TempHumi &input);
};
