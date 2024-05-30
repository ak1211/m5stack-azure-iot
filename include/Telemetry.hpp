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
#include <vector>
extern "C" {
#include <az_core.h>
#include <az_iot.h>
}

#include <M5Unified.h>

// MQTT通信
class Telemetry {
  constexpr static int32_t MQTT_PORT{AZ_IOT_DEFAULT_MQTT_CONNECT_PORT};
  constexpr static size_t MAX_SEND_FIFO_BUFFER_SIZE{500};
  constexpr static int32_t SAS_TOKEN_DURATION_IN_MINUTES{60};
  using MessageId = int32_t;
  // 送信用
  using Payload =
      std::variant<Sensor::MeasurementBme280, Sensor::MeasurementSgp30,
                   Sensor::MeasurementScd30, Sensor::MeasurementScd41,
                   Sensor::MeasurementM5Env3>;
  //
  struct Configuration {
    std::string iothub_fqdn{};
    std::string device_id{};
    std::string device_key{};
  };
  //
  struct EspMqttClientDeleter {
    void operator()(esp_mqtt_client *ptr) const;
  };
  using EspMqttClientPointerUnique =
      std::unique_ptr<esp_mqtt_client, EspMqttClientDeleter>;
  //

private:
  //
  Configuration config{};
  //
  az_iot_hub_client iot_hub_client{};
  az_iot_hub_client_options iot_hub_client_options{
      az_iot_hub_client_options_default()};
  //
  EspMqttClientPointerUnique mqtt_client;
  //
  std::string mqtt_broker_uri{};
  std::string mqtt_client_id{};
  std::string mqtt_username{};
  //
  std::array<char, 256> incoming_data{};
  //
  std::array<uint8_t, 256> sas_signature_buffer{};
  std::array<uint8_t, 256> sas_token_buffer{};
  std::optional<AzIoTSasToken> optAzIoTSasToken{};
  // 送信用FIFO待ち行列
  std::queue<Payload> _sending_fifo_buffer{};
  // 送信メッセージの実体を送信が終わるまで保持するコンテナ
  std::unordered_map<MessageId, std::string> _sent_messages{};
  //
  bool _mqtt_connected{false};

public:
  virtual ~Telemetry() { terminate(); }
  //
  bool begin(std::string_view iothub_fqdn, std::string_view device_id,
             std::string_view device_key);
  //
  bool reconnect();
  //
  bool terminate();
  //
  bool isConnected() const { return _mqtt_connected; };
  //
  bool task_handler();
  //
  bool enqueue(Payload in) {
    if (_sending_fifo_buffer.size() >= MAX_SEND_FIFO_BUFFER_SIZE) {
      M5_LOGE("FIFO buffer size limit reached.");
      return false;
    } else {
      _sending_fifo_buffer.push(std::move(in));
      return true;
    }
  }

private:
  //
  bool initializeIoTHubClient();
  bool initializeMqttClient();
  //
  static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);
  static std::string to_absolute_sensor_id(const std::string &device_id,
                                           SensorDescriptor descriptor);
  // 送信用メッセージに変換する
  template <typename T> std::string to_json_message(const T &in);
};
