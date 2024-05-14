// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "Application.hpp"
#include "AzIoTSasToken.h"
#include "Sensor.hpp"
#include "Telemetry.hpp"
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
#include <unordered_map>

#include <M5Unified.h>

// Azure IoT SDK for C includes
extern "C" {
#include <az_core.h>
#include <az_iot.h>
#include <azure_ca.h>
}
using namespace std::chrono;
using namespace std::string_literals;

//
std::string Telemetry::to_absolute_sensor_id(SensorDescriptor descriptor) {
  return std::string(Credentials.device_id) + "-"s + descriptor.str();
}

// 送信用メッセージに変換する
template <>
std::string Telemetry::to_json_message<Sensor::MeasurementBme280>(
    const Sensor::MeasurementBme280 &in) {
  JsonDocument doc;
  doc["sensorId"] = to_absolute_sensor_id(in.second.sensor_descriptor);
  doc["measuredAt"] = Application::isoformatUTC(in.first);
  doc["temperature"] = DegC(in.second.temperature).count();
  doc["humidity"] = PctRH(in.second.relative_humidity).count();
  doc["pressure"] = HectoPa(in.second.pressure).count();
  std::string output;
  serializeJson(doc, output);
  return output;
}

//
template <>
std::string Telemetry::to_json_message<Sensor::MeasurementM5Env3>(
    const Sensor::MeasurementM5Env3 &in) {
  JsonDocument doc;
  doc["sensorId"] = to_absolute_sensor_id(in.second.sensor_descriptor);
  doc["measuredAt"] = Application::isoformatUTC(in.first);
  doc["temperature"] = DegC(in.second.temperature).count();
  doc["humidity"] = PctRH(in.second.relative_humidity).count();
  doc["pressure"] = HectoPa(in.second.pressure).count();
  std::string output;
  serializeJson(doc, output);
  return output;
}

//
template <>
std::string Telemetry::to_json_message<Sensor::MeasurementSgp30>(
    const Sensor::MeasurementSgp30 &in) {
  JsonDocument doc;
  doc["sensorId"] = to_absolute_sensor_id(in.second.sensor_descriptor);
  doc["measuredAt"] = Application::isoformatUTC(in.first);
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

//
template <>
std::string Telemetry::to_json_message<Sensor::MeasurementScd30>(
    const Sensor::MeasurementScd30 &in) {
  JsonDocument doc;
  doc["sensorId"] = to_absolute_sensor_id(in.second.sensor_descriptor);
  doc["measuredAt"] = Application::isoformatUTC(in.first);
  doc["co2"] = in.second.co2.value;
  doc["temperature"] = DegC(in.second.temperature).count();
  doc["humidity"] = PctRH(in.second.relative_humidity).count();
  std::string output;
  serializeJson(doc, output);
  return output;
}

//
template <>
std::string Telemetry::to_json_message<Sensor::MeasurementScd41>(
    const Sensor::MeasurementScd41 &in) {
  JsonDocument doc;
  doc["sensorId"] = to_absolute_sensor_id(in.second.sensor_descriptor);
  doc["measuredAt"] = Application::isoformatUTC(in.first);
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
esp_err_t Telemetry::mqtt_event_handler(esp_mqtt_event_handle_t event) {
  if (event->user_context == nullptr) {
    M5_LOGE("user context had null");
    return ESP_OK;
  }
  Telemetry &telemetry = *static_cast<Telemetry *>(event->user_context);
  switch (event->event_id) {
  case MQTT_EVENT_ERROR:
    M5_LOGD("MQTT event MQTT_EVENT_ERROR");
    break;
  case MQTT_EVENT_CONNECTED:
    M5_LOGD("MQTT event MQTT_EVENT_CONNECTED");
    telemetry._mqtt_connected = true;
    if (telemetry.mqtt_client == nullptr) {
      M5_LOGE("mqtt_client had null.");
      break;
    }
    if (auto r = esp_mqtt_client_subscribe(
            telemetry.mqtt_client, AZ_IOT_HUB_CLIENT_C2D_SUBSCRIBE_TOPIC, 1);
        r == -1) {
      M5_LOGE("Could not subscribe for cloud-to-device messages.");
    } else {
      M5_LOGD("Subscribed for cloud-to-device messages; message id:%d", r);
    }
    break;
  case MQTT_EVENT_DISCONNECTED:
    M5_LOGD("MQTT event MQTT_EVENT_DISCONNECTED");
    telemetry._mqtt_connected = false;
    break;
  case MQTT_EVENT_SUBSCRIBED:
    M5_LOGD("MQTT event MQTT_EVENT_SUBSCRIBED");
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    M5_LOGD("MQTT event MQTT_EVENT_UNSUBSCRIBED");
    break;
  case MQTT_EVENT_PUBLISHED:
    M5_LOGD("MQTT event MQTT_EVENT_PUBLISHED; message id:%d", event->msg_id);
    if (event->msg_id >= 0) {
      if (auto itr = telemetry._sent_messages.find(event->msg_id);
          itr != telemetry._sent_messages.end()) {
        M5_LOGD("[PUBLISHED]:%s", itr->second.c_str());
      } else {
        M5_LOGE("PUBLISHED message ID is not found");
      }
      // 実際にMQTT送信が終わったので不要になったメッセージを消す
      telemetry._sent_messages.erase(event->msg_id);
    }
    break;
  case MQTT_EVENT_DATA:
    M5_LOGI("MQTT event MQTT_EVENT_DATA");
    telemetry.incoming_data.fill('\0');
    std::copy_n(event->topic, event->topic_len,
                telemetry.incoming_data.begin());
    M5_LOGI("[RECEIVE] Topic: %s", telemetry.incoming_data.data());
    telemetry.incoming_data.fill('\0');
    std::copy_n(event->data, event->data_len, telemetry.incoming_data.begin());
    M5_LOGI("[RECEIVE] Data: %s", telemetry.incoming_data.data());
    break;
  case MQTT_EVENT_BEFORE_CONNECT:
    M5_LOGD("MQTT event MQTT_EVENT_BEFORE_CONNECT");
    break;
  default:
    M5_LOGD("MQTT event UNKNOWN");
    break;
  }

  return ESP_OK;
}

//
bool Telemetry::initializeIoTHubClient() {
  if (az_result_failed(az_iot_hub_client_init(
          &iot_hub_client,
          az_span_create((uint8_t *)config.iothub_fqdn.data(),
                         config.iothub_fqdn.length()),
          az_span_create((uint8_t *)config.device_id.data(),
                         config.device_id.length()),
          &iot_hub_client_options))) {
    M5_LOGE("Failed initializing Azure IoT Hub client");
    return false;
  }

  {
    std::array<char, 128> buffer{};
    if (az_result_failed(az_iot_hub_client_get_client_id(
            &iot_hub_client, buffer.data(), buffer.size() - 1, nullptr))) {
      M5_LOGE("Failed getting client id");
      return false;
    }
    mqtt_client_id = std::string{buffer.data()};
  }

  {
    std::array<char, 128> buffer{};
    if (az_result_failed(az_iot_hub_client_get_user_name(
            &iot_hub_client, buffer.data(), buffer.size() - 1, NULL))) {
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
bool Telemetry::initializeMqttClient() {
  // Using SAS
  if (auto azIoTSasToken =
          AzIoTSasToken{
              &iot_hub_client,
              az_span_create(
                  reinterpret_cast<uint8_t *>(config.device_key.data()),
                  config.device_key.length()),
              az_span_create(sas_signature_buffer.data(),
                             sas_signature_buffer.size()),
              az_span_create(sas_token_buffer.data(), sas_token_buffer.size())};
      azIoTSasToken.Generate(SAS_TOKEN_DURATION_IN_MINUTES) != 0) {
    M5_LOGE("Failed generating SAS token");
    optAzIoTSasToken = std::nullopt;
    return false;
  } else {
    optAzIoTSasToken = std::make_optional(azIoTSasToken);
  }

  esp_mqtt_client_config_t mqtt_config{0};

  mqtt_config.uri = mqtt_broker_uri.c_str();
  mqtt_config.port = MQTT_PORT;
  mqtt_config.client_id = mqtt_client_id.c_str();
  mqtt_config.username = mqtt_username.c_str();

  // Using SAS key
  if (optAzIoTSasToken.has_value()) {
    auto &azIoTSasToken = optAzIoTSasToken.value();
    mqtt_config.password =
        reinterpret_cast<char *>(az_span_ptr(azIoTSasToken.Get()));
  } else {
    mqtt_config.password = "";
  }

  mqtt_config.keepalive = 180;
  mqtt_config.disable_clean_session = 0;
  mqtt_config.disable_auto_reconnect = false;
  mqtt_config.event_handle = mqtt_event_handler;
  mqtt_config.user_context = this;
  mqtt_config.cert_pem = reinterpret_cast<const char *>(ca_pem);

  if (mqtt_client) {
    esp_mqtt_client_destroy(mqtt_client);
  }
  _mqtt_connected = false;
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
bool Telemetry::begin(std::string_view iothub_fqdn, std::string_view device_id,
                      std::string_view device_key) {
  //
  iot_hub_client_options = az_iot_hub_client_options_default();
  iot_hub_client_options.user_agent =
      AZ_SPAN_FROM_STR(AZURE_SDK_CLIENT_USER_AGENT);
  //
  config.iothub_fqdn = std::string(iothub_fqdn);
  config.device_id = std::string(device_id);
  config.device_key = std::string(device_key);
  mqtt_broker_uri = std::string("mqtts://") + config.iothub_fqdn;
  //
  return (initializeIoTHubClient() && initializeMqttClient());
}

//
bool Telemetry::terminate() {
  if (esp_mqtt_client_destroy(mqtt_client) != ESP_OK) {
    mqtt_client = nullptr;
    M5_LOGE("esp_mqtt_client_destroy failure.");
    return false;
  }
  mqtt_client = nullptr;
  return true;
}

//
bool Telemetry::task_handler() {
  if (mqtt_client == nullptr) {
    return false;
  }
  if (_mqtt_connected == false) {
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
  std::array<char, 128> telemetry_topic{};
  if (az_result_failed(az_iot_hub_client_telemetry_get_publish_topic(
          &iot_hub_client, nullptr, telemetry_topic.data(),
          telemetry_topic.size(), nullptr))) {
    M5_LOGE("Failed az_iot_hub_client_telemetry_get_publish_topic");
    return false;
  }
  // 送信するべき測定値があれば送信する
  constexpr auto MQTT_QOS{1};
  constexpr auto DO_NOT_RETAIN_MSG{0};
  if (_sending_fifo_buffer.empty()) {
    // nothing to do
  } else {
    // 送信用FIFO待ち行列から先頭のアイテムを得る
    auto item = _sending_fifo_buffer.front();
    // メッセージに変換する
    std::string datum =
        std::visit([this](const auto &x) { return to_json_message(x); }, item);
    // MQTT待ち行列に入れる
    if (auto message_id = esp_mqtt_client_enqueue(
            mqtt_client, telemetry_topic.data(), datum.data(), datum.length(),
            MQTT_QOS, DO_NOT_RETAIN_MSG, true);
        message_id < 0) {
      M5_LOGE("Failed publishing");
      return false;
    } else {
      M5_LOGD("MQTT enqueued; message id: %d", message_id);
      M5_LOGV("MQTT enqueued; %s", datum.data());
      // MQTT待ち行列に送った後も、実際にMQTT送信が終わるまでポインタが指すメッセージの実体を保持しておく
      _sent_messages.insert({message_id, std::move(datum)});
      // MQTT待ち行列に送ったので送信用FIFO待ち行列から先頭のアイテムを消す
      _sending_fifo_buffer.pop();
      return true;
    }
  }
  return true;
}
