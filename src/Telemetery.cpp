// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "Application.hpp"
#include "AzIoTSasToken.h"
#include "Sensor.hpp"
#include "Telemetry.hpp"
#include "Time.hpp"
#include "credentials.h"
#include <Arduinojson.h>
#include <chrono>
#include <cmath>
#include <ctime>
#include <esp_sntp.h>
#include <mqtt_client.h>
#include <optional>
#include <queue>
#include <sstream>
#include <string>

#include <M5Unified.h>

// Azure IoT SDK for C includes
extern "C" {
#include <az_core.h>
#include <az_iot.h>
#include <azure_ca.h>
}
using namespace std::chrono;
using namespace std::string_literals;

Telemetry *Telemetry::_instance{nullptr};

//
static std::string to_absolute_sensor_id(SensorDescriptor descriptor) {
  return std::string(Credentials.device_id) + "-"s + descriptor.str();
}
// 送信用メッセージに変換する
template <>
std::string Telemetry::to_json_message<Sensor::MeasurementBme280>(
    MessageId messageId, const Sensor::MeasurementBme280 &in) {
  JsonDocument doc;
  doc["messageId"] = messageId;
  doc["sensorId"] = to_absolute_sensor_id(in.second.sensor_descriptor);
  doc["measuredAt"] = Time::isoformatUTC(in.first);
  doc["temperature"] = DegC(in.second.temperature).count();
  doc["humidity"] = PctRH(in.second.relative_humidity).count();
  doc["pressure"] = HectoPa(in.second.pressure).count();
  std::string output;
  serializeJson(doc, output);
  return output;
}
template <>
std::string Telemetry::to_json_message<Sensor::MeasurementM5Env3>(
    MessageId messageId, const Sensor::MeasurementM5Env3 &in) {
  JsonDocument doc;
  doc["messageId"] = messageId;
  doc["sensorId"] = to_absolute_sensor_id(in.second.sensor_descriptor);
  doc["measuredAt"] = Time::isoformatUTC(in.first);
  doc["temperature"] = DegC(in.second.temperature).count();
  doc["humidity"] = PctRH(in.second.relative_humidity).count();
  doc["pressure"] = HectoPa(in.second.pressure).count();
  std::string output;
  serializeJson(doc, output);
  return output;
}
template <>
std::string Telemetry::to_json_message<Sensor::MeasurementSgp30>(
    MessageId messageId, const Sensor::MeasurementSgp30 &in) {
  JsonDocument doc;
  doc["messageId"] = messageId;
  doc["sensorId"] = to_absolute_sensor_id(in.second.sensor_descriptor);
  doc["measuredAt"] = Time::isoformatUTC(in.first);
  doc["tvoc"] = in.second.tvoc.value;
  doc["eCo2"] = in.second.eCo2.value;
  if (in.second.tvoc_baseline.has_value()) {
    doc["tvoc_baseline"] = in.second.tvoc_baseline->value;
  }
  if (in.second.eCo2_baseline.has_value()) {
    doc["eCo2_baseline"] = in.second.eCo2_baseline->value;
  }
  std::string output;
  serializeJson(doc, output);
  return output;
}
template <>
std::string Telemetry::to_json_message<Sensor::MeasurementScd30>(
    MessageId messageId, const Sensor::MeasurementScd30 &in) {
  JsonDocument doc;
  doc["messageId"] = messageId;
  doc["sensorId"] = to_absolute_sensor_id(in.second.sensor_descriptor);
  doc["measuredAt"] = Time::isoformatUTC(in.first);
  doc["co2"] = in.second.co2.value;
  doc["temperature"] = DegC(in.second.temperature).count();
  doc["humidity"] = PctRH(in.second.relative_humidity).count();
  std::string output;
  serializeJson(doc, output);
  return output;
}
template <>
std::string Telemetry::to_json_message<Sensor::MeasurementScd41>(
    MessageId messageId, const Sensor::MeasurementScd41 &in) {
  JsonDocument doc;
  doc["messageId"] = messageId;
  doc["sensorId"] = to_absolute_sensor_id(in.second.sensor_descriptor);
  doc["measuredAt"] = Time::isoformatUTC(in.first);
  doc["co2"] = in.second.co2.value;
  doc["temperature"] = DegC(in.second.temperature).count();
  doc["humidity"] = PctRH(in.second.relative_humidity).count();
  std::string output;
  serializeJson(doc, output);
  return output;
}

// When developing for your own Arduino-based platform,
// please follow the format '(ard;<platform>)'.
#define AZURE_SDK_CLIENT_USER_AGENT "c%2F" AZ_SDK_VERSION_STRING "(ard;esp32)"

//
//
//
esp_err_t Telemetry::mqtt_event_handler(esp_mqtt_event_handle_t event) {
  Telemetry *telemetry = static_cast<Telemetry *>(event->user_context);
  if (telemetry == nullptr) {
    M5_LOGE("user context had null");
    return ESP_OK;
  }
  switch (event->event_id) {
  case MQTT_EVENT_ERROR:
    M5_LOGI("MQTT event MQTT_EVENT_ERROR");
    break;
  case MQTT_EVENT_CONNECTED:
    M5_LOGI("MQTT event MQTT_EVENT_CONNECTED");
    telemetry->mqtt_connected = true;
    if (telemetry->mqtt_client == nullptr) {
      M5_LOGE("mqtt_client had null.");
      break;
    }
    if (auto r = esp_mqtt_client_subscribe(
            telemetry->mqtt_client, AZ_IOT_HUB_CLIENT_C2D_SUBSCRIBE_TOPIC, 1);
        r == -1) {
      M5_LOGE("Could not subscribe for cloud-to-device messages.");
    } else {
      M5_LOGI("Subscribed for cloud-to-device messages; message id:%d", r);
    }
    break;
  case MQTT_EVENT_DISCONNECTED:
    M5_LOGI("MQTT event MQTT_EVENT_DISCONNECTED");
    telemetry->mqtt_connected = false;
    break;
  case MQTT_EVENT_SUBSCRIBED:
    M5_LOGI("MQTT event MQTT_EVENT_SUBSCRIBED");
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    M5_LOGI("MQTT event MQTT_EVENT_UNSUBSCRIBED");
    break;
  case MQTT_EVENT_PUBLISHED:
    M5_LOGI("MQTT event MQTT_EVENT_PUBLISHED");
    break;
  case MQTT_EVENT_DATA:
    M5_LOGI("MQTT event MQTT_EVENT_DATA");
    telemetry->incoming_data.fill('\0');
    std::copy_n(event->topic, event->topic_len,
                telemetry->incoming_data.begin());
    M5_LOGI("[RECEIVE] Topic: %s", telemetry->incoming_data.data());
    telemetry->incoming_data.fill('\0');
    std::copy_n(event->data, event->data_len, telemetry->incoming_data.begin());
    M5_LOGI("[RECEIVE] Data: %s", telemetry->incoming_data.data());
    break;
  case MQTT_EVENT_BEFORE_CONNECT:
    M5_LOGI("MQTT event MQTT_EVENT_BEFORE_CONNECT");
    break;
  default:
    M5_LOGE("MQTT event UNKNOWN");
    break;
  }

  return ESP_OK;
}

//
//
//
bool Telemetry::initializeIoTHubClient() {
  if (az_result_failed(az_iot_hub_client_init(
          &client,
          az_span_create((uint8_t *)config_iothub_fqdn.c_str(),
                         config_iothub_fqdn.length()),
          az_span_create((uint8_t *)config_device_id.c_str(),
                         config_device_id.length()),
          &client_options))) {
    M5_LOGE("Failed initializing Azure IoT Hub client");
    return false;
  }

  {
    std::array<char, 128> buffer{};
    if (az_result_failed(az_iot_hub_client_get_client_id(
            &client, buffer.data(), buffer.size() - 1, nullptr))) {
      M5_LOGE("Failed getting client id");
      return false;
    }
    mqtt_client_id = std::string{buffer.data()};
  }

  {
    std::array<char, 128> buffer{};
    if (az_result_failed(az_iot_hub_client_get_user_name(
            &client, buffer.data(), buffer.size() - 1, NULL))) {
      M5_LOGE("Failed to get MQTT clientId, return code");
      return false;
    }
    mqtt_username = std::string{buffer.data()};
  }

  M5_LOGI("Client ID: %s", mqtt_client_id.c_str());
  M5_LOGI("Username: %s", mqtt_username.c_str());
  return true;
}

//
//
//
bool Telemetry::initializeMqttClient() {
  // Using SAS
  if (auto azIoTSasToken =
          AzIoTSasToken{
              &client,
              az_span_create(
                  reinterpret_cast<uint8_t *>(config_device_key.data()),
                  config_device_key.length()),
              az_span_create(sas_signature_buffer.data(),
                             sas_signature_buffer.size()),
              az_span_create(mqtt_password.data(), mqtt_password.size())};
      azIoTSasToken.Generate(SAS_TOKEN_DURATION_IN_MINUTES) != 0) {
    M5_LOGE("Failed generating SAS token");
    optAzIoTSasToken = std::nullopt;
    return false;
  } else {
    optAzIoTSasToken = std::make_optional(azIoTSasToken);
  }

  esp_mqtt_client_config_t mqtt_config{0};

  mqtt_config.uri = mqtt_broker_uri.c_str();
  mqtt_config.port = mqtt_port;
  mqtt_config.client_id = mqtt_client_id.c_str();
  mqtt_config.username = mqtt_username.c_str();

  // Using SAS key
  mqtt_config.password =
      reinterpret_cast<char *>(az_span_ptr(optAzIoTSasToken->Get()));

  mqtt_config.keepalive = 60;
  mqtt_config.disable_clean_session = 0;
  mqtt_config.disable_auto_reconnect = false;
  mqtt_config.event_handle = mqtt_event_handler;
  mqtt_config.user_context = this;
  mqtt_config.cert_pem = reinterpret_cast<const char *>(ca_pem);

  if (mqtt_client) {
    esp_mqtt_client_destroy(mqtt_client);
    mqtt_client = nullptr;
  }
  mqtt_client = esp_mqtt_client_init(&mqtt_config);

  if (mqtt_client == nullptr) {
    M5_LOGE("Failed creating mqtt client");
    return false;
  }

  esp_err_t start_result = esp_mqtt_client_start(mqtt_client);

  if (start_result != ESP_OK) {
    M5_LOGE("Could not start mqtt client; error code:%d", start_result);
    return false;
  } else {
    M5_LOGI("MQTT client started");
    return true;
  }
}

//
//
//
bool Telemetry::begin(std::string_view iothub_fqdn, std::string_view device_id,
                      std::string_view device_key) {
  //
  client_options = az_iot_hub_client_options_default();
  client_options.user_agent = AZ_SPAN_FROM_STR(AZURE_SDK_CLIENT_USER_AGENT);
  //
  config_iothub_fqdn = std::string(iothub_fqdn);
  config_device_id = std::string(device_id);
  config_device_key = std::string(device_key);
  mqtt_broker_uri = std::string("mqtts://" + config_iothub_fqdn);
  //
  return (initializeIoTHubClient() && initializeMqttClient());
}

//
//
//
bool Telemetry::loopMqtt() {
  if (mqtt_client == nullptr) {
    return false;
  }
  if (mqtt_connected == false) {
    if (esp_mqtt_client_destroy(mqtt_client) != ESP_OK) {
      M5_LOGE("esp_mqtt_client_destroy failure.");
    }
    mqtt_client = nullptr;
    return false;
  }
  if (optAzIoTSasToken.has_value() && optAzIoTSasToken->IsExpired()) {
    M5_LOGI("SAS token expired; reconnecting with a new one.");
    if (mqtt_client && esp_mqtt_client_destroy(mqtt_client) != ESP_OK) {
      M5_LOGE("esp_mqtt_client_destroy failure.");
    }
    mqtt_client = nullptr;
    return initializeMqttClient();
  }
  // The topic could be obtained just once during setup,
  // however if properties are used the topic need to be generated again to
  // reflect the current values of the properties.
  if (az_result_failed(az_iot_hub_client_telemetry_get_publish_topic(
          &client, nullptr, telemetry_topic.data(), telemetry_topic.size(),
          nullptr))) {
    M5_LOGE("Failed az_iot_hub_client_telemetry_get_publish_topic");
    return false;
  }
  // 送信するべき測定値があれば送信する
  if (!sending_fifo_queue.empty()) {
    std::string payload = std::visit(
        [this](const auto &x) {
          return Telemetry::to_json_message(message_id, x);
        },
        sending_fifo_queue.front());
    M5_LOGD("[SEND] %s", payload.c_str());
    auto count = esp_mqtt_client_publish(mqtt_client, telemetry_topic.data(),
                                         payload.c_str(), 0, MQTT_QOS1,
                                         DO_NOT_RETAIN_MSG);
    if (count < 0) {
      M5_LOGE("Failed publishing");
      return false;
    } else {
      //  Count up message_id
      message_id = message_id + 1;
      // IoT Coreへ送信した測定値をFIFOから消す
      sending_fifo_queue.pop();
      return true;
    }
  }
  return true;
}