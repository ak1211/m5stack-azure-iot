// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "AzIoTSasToken.h"
#include "Sensor.hpp"
#include <mqtt_client.h>
#include <string>
#include <variant>
#include <vector>
extern "C" {
#include <az_core.h>
#include <az_iot.h>
}

// MQTT通信
class Telemetry {
  static Telemetry *_instance;

private:
  constexpr static size_t MAX_SEND_FIFO_BUFFER_SIZE{64};
  using MessageId = uint32_t;
  // 送信用
  using Payload =
      std::variant<Sensor::MeasurementBme280, Sensor::MeasurementSgp30,
                   Sensor::MeasurementScd30, Sensor::MeasurementScd41,
                   Sensor::MeasurementM5Env3>;
  //
  az_iot_hub_client client{};
  az_iot_hub_client_options client_options{az_iot_hub_client_options_default()};
  std::string config_iothub_fqdn{};
  std::string config_device_id{};
  std::string config_device_key{};
  //
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
  std::vector<std::pair<std::optional<int32_t>, std::string>>
      sending_fifo_vect{};
  //
  bool mqtt_connected{false};
  //

public:
  //
  Telemetry() {
    if (_instance) {
      delete _instance;
    }
    _instance = this;
  }
  virtual ~Telemetry() { _instance = nullptr; }
  //
  static Telemetry *getInstance() { return _instance; }
  //
  bool begin(std::string_view iothub_fqdn, std::string_view device_id,
             std::string_view device_key);
  //
  bool loopMqtt();
  //
  bool pushMessage(const Payload &in);
  // 送信用メッセージに変換する
  template <typename T> std::string to_json_message(const T &in);

private:
  //
  bool initializeIoTHubClient();
  bool initializeMqttClient();
  //
  static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event);
  static std::string to_absolute_sensor_id(SensorDescriptor descriptor);
};
