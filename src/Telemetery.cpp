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

// Azure IoT SDK for C includes
extern "C" {
#include <az_core.h>
#include <az_iot.h>
#include <azure_ca.h>
}
using namespace std::chrono;
using namespace std::string_literals;

constexpr std::size_t Capacity{JSON_OBJECT_SIZE(128)};
//
static std::string to_absolute_sensor_id(SensorDescriptor descriptor) {
  return std::string(Credentials.device_id) + "-"s + descriptor.str();
}
// 送信用メッセージに変換する
template <>
std::string
Telemetry::to_json_message<MeasurementBme280>(MessageId messageId,
                                              const MeasurementBme280 &in) {
  StaticJsonDocument<Capacity> doc;
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
std::string
Telemetry::to_json_message<MeasurementM5Env3>(MessageId messageId,
                                              const MeasurementM5Env3 &in) {
  StaticJsonDocument<Capacity> doc;
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
std::string
Telemetry::to_json_message<MeasurementSgp30>(MessageId messageId,
                                             const MeasurementSgp30 &in) {
  StaticJsonDocument<Capacity> doc;
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
std::string
Telemetry::to_json_message<MeasurementScd30>(MessageId messageId,
                                             const MeasurementScd30 &in) {
  StaticJsonDocument<Capacity> doc;
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
std::string
Telemetry::to_json_message<MeasurementScd41>(MessageId messageId,
                                             const MeasurementScd41 &in) {
  StaticJsonDocument<Capacity> doc;
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
// globals
//
static uint32_t message_id = 0;
// 送信用バッファ
std::queue<Telemetry::Payload> Telemetry::sending_fifo_queue{};
//
static az_iot_hub_client client{};
static az_iot_hub_client_options client_options{
    az_iot_hub_client_options_default()};
static std::string config_iothub_fqdn{};
static std::string config_device_id{};
static std::string config_device_key{};
//
constexpr static int MQTT_QOS1{1};
constexpr static int DO_NOT_RETAIN_MSG{0};
constexpr static int mqtt_port{AZ_IOT_DEFAULT_MQTT_CONNECT_PORT};
static esp_mqtt_client_handle_t mqtt_client{};
static std::string mqtt_broker_uri{};
static std::string mqtt_client_id{};
static std::string mqtt_username{};
static std::array<char, 128> telemetry_topic{};
//
static std::array<uint8_t, 200> mqtt_password{};
static std::array<char, 128> incoming_data{};
//
constexpr static int SAS_TOKEN_DURATION_IN_MINUTES{60};
static std::array<uint8_t, 256> sas_signature_buffer{};
static std::optional<AzIoTSasToken> optSasToken{};

//
//
//
esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event) {
  switch (event->event_id) {
    int i, r;

  case MQTT_EVENT_ERROR:
    ESP_LOGI(TELEMETRY, "MQTT event MQTT_EVENT_ERROR");
    break;
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TELEMETRY, "MQTT event MQTT_EVENT_CONNECTED");
    r = esp_mqtt_client_subscribe(mqtt_client,
                                  AZ_IOT_HUB_CLIENT_C2D_SUBSCRIBE_TOPIC, 1);
    if (r == -1) {
      ESP_LOGE(TELEMETRY, "Could not subscribe for cloud-to-device messages.");
    } else {
      ESP_LOGI(TELEMETRY,
               "Subscribed for cloud-to-device messages; message id:%d", r);
    }
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TELEMETRY, "MQTT event MQTT_EVENT_DISCONNECTED");
    break;
  case MQTT_EVENT_SUBSCRIBED:
    ESP_LOGI(TELEMETRY, "MQTT event MQTT_EVENT_SUBSCRIBED");
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    ESP_LOGI(TELEMETRY, "MQTT event MQTT_EVENT_UNSUBSCRIBED");
    break;
  case MQTT_EVENT_PUBLISHED:
    ESP_LOGI(TELEMETRY, "MQTT event MQTT_EVENT_PUBLISHED");
    break;
  case MQTT_EVENT_DATA:
    ESP_LOGI(TELEMETRY, "MQTT event MQTT_EVENT_DATA");
    incoming_data.fill('\0');
    std::copy_n(event->topic, event->topic_len, incoming_data.begin());
    ESP_LOGI(RECEIVE, "Topic: %s", incoming_data.data());
    incoming_data.fill('\0');
    std::copy_n(event->data, event->data_len, incoming_data.begin());
    ESP_LOGI(RECEIVE, "Data: %s", incoming_data.data());
    break;
  case MQTT_EVENT_BEFORE_CONNECT:
    ESP_LOGI(TELEMETRY, "MQTT event MQTT_EVENT_BEFORE_CONNECT");
    break;
  default:
    ESP_LOGE(TELEMETRY, "MQTT event UNKNOWN");
    break;
  }

  return ESP_OK;
}

//
//
//
static bool initializeIoTHubClient() {
  if (az_result_failed(az_iot_hub_client_init(
          &client,
          az_span_create((uint8_t *)config_iothub_fqdn.c_str(),
                         config_iothub_fqdn.length()),
          az_span_create((uint8_t *)config_device_id.c_str(),
                         config_device_id.length()),
          &client_options))) {
    ESP_LOGE(TELEMETRY, "Failed initializing Azure IoT Hub client");
    return false;
  }

  {
    std::array<char, 128> buffer{};
    if (az_result_failed(az_iot_hub_client_get_client_id(
            &client, buffer.data(), buffer.size() - 1, nullptr))) {
      ESP_LOGE(TELEMETRY, "Failed getting client id");
      return false;
    }
    mqtt_client_id = std::string{buffer.data()};
  }

  {
    std::array<char, 128> buffer{};
    if (az_result_failed(az_iot_hub_client_get_user_name(
            &client, buffer.data(), buffer.size() - 1, NULL))) {
      ESP_LOGE(TELEMETRY, "Failed to get MQTT clientId, return code");
      return false;
    }
    mqtt_username = std::string{buffer.data()};
  }

  ESP_LOGI(TELEMETRY, "Client ID: %s", mqtt_client_id.c_str());
  ESP_LOGI(TELEMETRY, "Username: %s", mqtt_username.c_str());
  return true;
}

//
//
//
static bool initializeMqttClient() {
  // Using SAS
  optSasToken = []() -> std::optional<AzIoTSasToken> {
    AzIoTSasToken azIoTSasToken = AzIoTSasToken(
        &client,
        az_span_create((uint8_t *)(config_device_key.c_str()),
                       config_device_key.length()),
        az_span_create(sas_signature_buffer.data(),
                       sas_signature_buffer.size()),
        az_span_create(mqtt_password.data(), mqtt_password.size()));
    if (azIoTSasToken.Generate(SAS_TOKEN_DURATION_IN_MINUTES) != 0) {
      ESP_LOGE(TELEMETRY, "Failed generating SAS token");
      return std::nullopt;
    }
    return std::make_optional(azIoTSasToken);
  }();
  if (!optSasToken) {
    return false;
  }

  esp_mqtt_client_config_t mqtt_config{0};

  mqtt_config.uri = mqtt_broker_uri.c_str();
  mqtt_config.port = mqtt_port;
  mqtt_config.client_id = mqtt_client_id.c_str();
  mqtt_config.username = mqtt_username.c_str();

  // Using SAS key
  mqtt_config.password =
      reinterpret_cast<char *>(az_span_ptr(optSasToken->Get()));

  mqtt_config.keepalive = 30;
  mqtt_config.disable_clean_session = 0;
  mqtt_config.disable_auto_reconnect = false;
  mqtt_config.event_handle = mqtt_event_handler;
  mqtt_config.user_context = nullptr;
  mqtt_config.cert_pem = reinterpret_cast<const char *>(ca_pem);

  mqtt_client = esp_mqtt_client_init(&mqtt_config);

  if (mqtt_client == nullptr) {
    ESP_LOGE(TELEMETRY, "Failed creating mqtt client");
    return false;
  }

  esp_err_t start_result = esp_mqtt_client_start(mqtt_client);

  if (start_result != ESP_OK) {
    ESP_LOGE(TELEMETRY, "Could not start mqtt client; error code:%d",
             start_result);
    return false;
  } else {
    ESP_LOGI(TELEMETRY, "MQTT client started");
    return true;
  }
}

//
//
//
bool Telemetry::init(std::string_view iothub_fqdn, std::string_view device_id,
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
  if (optSasToken.has_value() && optSasToken->IsExpired()) {
    ESP_LOGI(TELEMETRY, "SAS token expired; reconnecting with a new one.");
    (void)esp_mqtt_client_destroy(mqtt_client);
    return initializeMqttClient();
  }
  // The topic could be obtained just once during setup,
  // however if properties are used the topic need to be generated again to
  // reflect the current values of the properties.
  if (az_result_failed(az_iot_hub_client_telemetry_get_publish_topic(
          &client, nullptr, telemetry_topic.data(), telemetry_topic.size(),
          nullptr))) {
    ESP_LOGE(TELEMETRY, "Failed az_iot_hub_client_telemetry_get_publish_topic");
    return false;
  }
  // 送信するべき測定値があれば送信する
  if (!sending_fifo_queue.empty()) {
    std::string payload = std::visit(
        [](const auto &x) { return Telemetry::to_json_message(message_id, x); },
        sending_fifo_queue.front());
    ESP_LOGD(SEND, "%s", payload.c_str());
    auto count = esp_mqtt_client_publish(mqtt_client, telemetry_topic.data(),
                                         payload.c_str(), 0, MQTT_QOS1,
                                         DO_NOT_RETAIN_MSG);
    if (count < 0) {
      ESP_LOGE(TELEMETRY, "Failed publishing");
      return false;
    } else {
      ESP_LOGI(TELEMETRY, "Message published successfully");
      //  Count up message_id
      message_id = message_id + 1;
      // IoT Coreへ送信した測定値をFIFOから消す
      sending_fifo_queue.pop();
      return true;
    }
  }
  return true;
}