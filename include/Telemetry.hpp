// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "AzIoTSasToken.h"
#include "Sensor.hpp"
#include <mqtt_client.h>
#include <queue>
#include <string>
#include <variant>
extern "C" {
#include <az_core.h>
#include <az_iot.h>
}

// MQTT通信
class Telemetry {
  constexpr static auto MAXIMUM_QUEUE_SIZE = 100;
  using MessageId = uint32_t;
  // 送信用
  using Payload =
      std::variant<Sensor::MeasurementBme280, Sensor::MeasurementSgp30,
                   Sensor::MeasurementScd30, Sensor::MeasurementScd41,
                   Sensor::MeasurementM5Env3>;
  //
  uint32_t message_id{0};
  //
  az_iot_hub_client client{};
  az_iot_hub_client_options client_options{az_iot_hub_client_options_default()};
  std::string config_iothub_fqdn{};
  std::string config_device_id{};
  std::string config_device_key{};
  //
  constexpr static int MQTT_QOS1{1};
  constexpr static int DO_NOT_RETAIN_MSG{0};
  constexpr static int mqtt_port{AZ_IOT_DEFAULT_MQTT_CONNECT_PORT};
  esp_mqtt_client_handle_t mqtt_client{};
  std::string mqtt_broker_uri{};
  std::string mqtt_client_id{};
  std::string mqtt_username{};
  std::array<char, 128> telemetry_topic{};
  //
  std::array<uint8_t, 200> mqtt_password{};
  std::array<char, 128> incoming_data{};
  //
  constexpr static int SAS_TOKEN_DURATION_IN_MINUTES{60};
  std::array<uint8_t, 256> sas_signature_buffer{};
  std::optional<AzIoTSasToken> optAzIoTSasToken{};
  // 送信用バッファ
  std::queue<Payload> sending_fifo_queue{};
  //
  bool mqtt_connected{false};
  //
  static Telemetry *_instance;

public:
  //
  Telemetry() {
    if (_instance) {
      delete _instance;
    }
    _instance = this;
  }
  ~Telemetry() {}
  //
  static Telemetry *getInstance() { return _instance; }
  //
  bool begin(std::string_view iothub_fqdn, std::string_view device_id,
             std::string_view device_key);
  //
  bool loopMqtt();
  // 送信用メッセージに変換する
  template <typename T>
  std::string to_json_message(MessageId messageId, const T &in);
  //
  inline bool pushMessage(const Payload &in) {
    if (sending_fifo_queue.size() < MAXIMUM_QUEUE_SIZE) {
      sending_fifo_queue.push(in);
      return true;
    }
    return false;
  }

private:
  //
  bool initializeIoTHubClient();
  bool initializeMqttClient();
  //
  static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);
};
