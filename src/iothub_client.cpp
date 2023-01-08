// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "iothub_client.hpp"
#include "credentials.h"
#include <Arduinojson.h>
#include <chrono>
#include <cmath>
#include <ctime>
#include <esp_sntp.h>
#include <mqtt_client.h>
#include <sstream>
#include <string>

// Azure IoT SDK for C includes
extern "C" {
#include <az_core.h>
#include <az_iot.h>
#include <azure_ca.h>
}

// ログ出し用
constexpr static const char TELEMETRY[] = "TELEMETRY";

// When developing for your own Arduino-based platform,
// please follow the format '(ard;<platform>)'.
#define AZURE_SDK_CLIENT_USER_AGENT "c%2F" AZ_SDK_VERSION_STRING "(ard;esp32)"

//
double round_to_2_decimal_places(double x) {
  return std::nearbyint(x * 100.0) / 100.0;
}

//
uint32_t IotHubClient::message_id = 0;
std::array<uint8_t, 200> IotHubClient::mqtt_password;
std::array<char, 128> IotHubClient::incoming_data;
//
az_iot_hub_client IotHubClient::client;
az_iot_hub_client_options IotHubClient::client_options{
    az_iot_hub_client_options_default()};
//
esp_mqtt_client_handle_t IotHubClient::mqtt_client{};

//
//
//
esp_err_t IotHubClient::mqtt_event_handler(esp_mqtt_event_handle_t event) {
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
    ESP_LOGI(TELEMETRY, "Topic: %s", incoming_data.data());
    incoming_data.fill('\0');
    std::copy_n(event->data, event->data_len, incoming_data.begin());
    ESP_LOGI(TELEMETRY, "Data: %s", incoming_data.data());
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
bool IotHubClient::begin(std::string_view iothub_fqdn,
                         std::string_view device_id,
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
  if (initializeIoTHubClient() && initializeMqttClient()) {
    return true;
  } else {
    return true;
  }
}

//
//
//
bool IotHubClient::initializeIoTHubClient() {
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
bool IotHubClient::initializeMqttClient() {
  // Using SAS
  AzIoTSasToken azIoTSasToken = AzIoTSasToken(
      &client,
      az_span_create((uint8_t *)(config_device_key.c_str()),
                     config_device_key.length()),
      az_span_create(sas_signature_buffer.data(), sas_signature_buffer.size()),
      az_span_create(mqtt_password.data(), mqtt_password.size()));
  if (azIoTSasToken.Generate(SAS_TOKEN_DURATION_IN_MINUTES) != 0) {
    ESP_LOGE(TELEMETRY, "Failed generating SAS token");
    return false;
  }
  optSasToken = azIoTSasToken;

  esp_mqtt_client_config_t mqtt_config{0};

  mqtt_config.uri = mqtt_broker_uri.c_str();
  mqtt_config.port = mqtt_port;
  mqtt_config.client_id = mqtt_client_id.c_str();
  mqtt_config.username = mqtt_username.c_str();

  // Using SAS key
  mqtt_config.password =
      reinterpret_cast<char *>(az_span_ptr(azIoTSasToken.Get()));

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
void IotHubClient::check() {
  if (optSasToken.has_value() && optSasToken.value().IsExpired()) {
    ESP_LOGI(TELEMETRY, "SAS token expired; reconnecting with a new one.");
    (void)esp_mqtt_client_destroy(mqtt_client);
    initializeMqttClient();
  }
}

//
//
//
bool IotHubClient::pushMessage(JsonDocument &doc) {
  if (doc.isNull()) {
    return true;
  }
  std::ostringstream stream;
  //
  doc["messageId"] = message_id;
  serializeJson(doc, stream);
  std::string payload = stream.str();
  ESP_LOGD(TELEMETRY, "messagePayload:%s", payload.c_str());
  // Count up message_id
  message_id = message_id + 1;

  // The topic could be obtained just once during setup,
  // however if properties are used the topic need to be generated again to
  // reflect the current values of the properties.
  if (az_result_failed(az_iot_hub_client_telemetry_get_publish_topic(
          &client, nullptr, telemetry_topic.data(), telemetry_topic.size(),
          nullptr))) {
    ESP_LOGE(TELEMETRY, "Failed az_iot_hub_client_telemetry_get_publish_topic");
    return false;
  }
  if (esp_mqtt_client_publish(mqtt_client, telemetry_topic.data(),
                              payload.c_str(), 0, MQTT_QOS1,
                              DO_NOT_RETAIN_MSG) == 0) {
    ESP_LOGE(TELEMETRY, "Failed publishing");
    return false;
  } else {
    ESP_LOGI(TELEMETRY, "Message published successfully");
    return true;
  }
}

//
//
//
bool IotHubClient::pushTempHumiPres(const std::string &sensor_id,
                                    const TempHumiPres &input) {
  DynamicJsonDocument doc(MESSAGE_MAX_LEN);
  if (doc.capacity() == 0) {
    ESP_LOGE(TELEMETRY, "memory allocation error.");
    return false;
  }
  const DegC tCelcius = input.temperature;
  const RelHumidity rh = input.relative_humidity;
  const HectoPa hpa = input.pressure;
  std::string buf;
  //
  doc["sensorId"] = sensor_id;
  doc["measuredAt"] = TickTack::isoformatUTC(buf, input.at);
  doc["temperature"] = round_to_2_decimal_places(tCelcius.count());
  doc["humidity"] = round_to_2_decimal_places(rh.count());
  doc["pressure"] = round_to_2_decimal_places(hpa.count());
  //
  return pushMessage(doc);
}

//
//
//
bool IotHubClient::pushTvocEco2(const std::string &sensor_id,
                                const TvocEco2 &input) {
  DynamicJsonDocument doc(MESSAGE_MAX_LEN);
  if (doc.capacity() == 0) {
    ESP_LOGE(TELEMETRY, "memory allocation error.");
    return false;
  }
  std::string buf;
  //
  doc["sensorId"] = sensor_id;
  doc["measuredAt"] = TickTack::isoformatUTC(buf, input.at);
  doc["tvoc"] = input.tvoc.value;
  doc["eCo2"] = input.eCo2.value;
  if (input.tvoc_baseline.has_value()) {
    doc["tvoc_baseline"] = input.tvoc_baseline.value().value;
  }
  if (input.eCo2_baseline.has_value()) {
    doc["eCo2_baseline"] = input.eCo2_baseline.value().value;
  }
  //
  return pushMessage(doc);
}

//
//
//
bool IotHubClient::pushCo2TempHumi(const std::string &sensor_id,
                                   const Co2TempHumi &input) {
  DynamicJsonDocument doc(MESSAGE_MAX_LEN);
  if (doc.capacity() == 0) {
    ESP_LOGE(TELEMETRY, "memory allocation error.");
    return false;
  }
  const DegC tCelcius = input.temperature;
  const RelHumidity rh = input.relative_humidity;
  std::string buf;
  //
  doc["sensorId"] = sensor_id;
  doc["measuredAt"] = TickTack::isoformatUTC(buf, input.at);
  doc["co2"] = input.co2.value;
  doc["temperature"] = round_to_2_decimal_places(tCelcius.count());
  doc["humidity"] = round_to_2_decimal_places(rh.count());
  //
  return pushMessage(doc);
}
