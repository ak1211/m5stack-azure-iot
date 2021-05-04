// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "iothub_client.hpp"
#include "credentials.h"
#include <Arduinojson.h>
#include <Esp32MQTTClient.h>
#include <M5Core2.h>
#include <ctime>
#include <lwip/apps/sntp.h>

constexpr static const char *TAG = "IoTHubModule";

uint32_t IotHubClient::message_id = 0;

//
//
//
void IotHubClient::init(SEND_CONFIRMATION_CALLBACK send_confirmation_callback,
                        MESSAGE_CALLBACK message_callback,
                        DEVICE_METHOD_CALLBACK device_method_callback,
                        DEVICE_TWIN_CALLBACK device_twin_callback,
                        CONNECTION_STATUS_CALLBACK connection_status_callback) {
  Esp32MQTTClient_SetOption(OPTION_MINI_SOLUTION_NAME, "GetStarted");
  Esp32MQTTClient_Init((const uint8_t *)Credentials.connection_string, true);

  Esp32MQTTClient_SetSendConfirmationCallback(send_confirmation_callback);
  Esp32MQTTClient_SetMessageCallback(message_callback);
  Esp32MQTTClient_SetDeviceTwinCallback(device_twin_callback);
  Esp32MQTTClient_SetDeviceMethodCallback(device_method_callback);
  Esp32MQTTClient_SetConnectionStatusCallback(connection_status_callback);
}

//
//
//
JsonDocument *IotHubClient::pushMessage(JsonDocument &doc) {
  if (doc.isNull()) {
    return &doc;
  }
  //
  char *messagePayload = (char *)calloc(MESSAGE_MAX_LEN + 1, sizeof(char));
  if (!messagePayload) {
    ESP_LOGE(TAG, "memory allocation error.");
    return nullptr;
  }
  doc["messageId"] = message_id;
  serializeJson(doc, messagePayload, MESSAGE_MAX_LEN);
  ESP_LOGD(TAG, "messagePayload:%s", messagePayload);
  message_id = message_id + 1;
  //
  EVENT_INSTANCE *message =
      Esp32MQTTClient_Event_Generate(messagePayload, MESSAGE);
  //    Esp32MQTTClient_Event_AddProp(message, "temperatureAlert", "true");
  Esp32MQTTClient_SendEventInstance(message);
  //
  free(messagePayload);
  return &doc;
}

//
//
//
JsonDocument *IotHubClient::pushState(JsonDocument &doc) {
  if (doc.isNull()) {
    return &doc;
  }
  //
  char *statePayload = (char *)calloc(MESSAGE_MAX_LEN + 1, sizeof(char));
  if (!statePayload) {
    ESP_LOGE(TAG, "memory allocation error.");
    return nullptr;
  }
  serializeJson(doc, statePayload, MESSAGE_MAX_LEN);
  ESP_LOGD(TAG, "statePayload:%s", statePayload);
  EVENT_INSTANCE *state = Esp32MQTTClient_Event_Generate(statePayload, STATE);
  Esp32MQTTClient_SendEventInstance(state);
  //
  free(statePayload);
  return &doc;
}
