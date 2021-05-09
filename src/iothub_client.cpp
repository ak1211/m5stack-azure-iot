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
static void sendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result) {
  if (result == IOTHUB_CLIENT_CONFIRMATION_OK) {
    ESP_LOGD(TAG, "Send Confirmation Callback finished.");
  }
}
//
static void messageCallback(const char *payLoad, int size) {
  ESP_LOGI(TAG, "Message callback:%s", payLoad);
}
//
static int deviceMethodCallback(const char *methodName,
                                const unsigned char *payload, int size,
                                unsigned char **response, int *response_size) {
  ESP_LOGI(TAG, "Try to invoke method %s", methodName);
  const char *responseMessage = "\"Successfully invoke device method\"";
  int result = 200;

  if (strcmp(methodName, "calibration") == 0) {
    /*
    ESP_LOGI(TAG, "Start calibrate the sensor");
    if (!system_properties.sgp30.begin()) {
      responseMessage = "\"calibration failed\"";
      result = 503;
    }
    */
  } else if (strcmp(methodName, "start") == 0) {
    ESP_LOGI(TAG, "Start sending temperature and humidity data");
    //    messageSending = true;
  } else if (strcmp(methodName, "stop") == 0) {
    ESP_LOGI(TAG, "Stop sending temperature and humidity data");
    //    messageSending = false;
  } else {
    ESP_LOGI(TAG, "No method %s found", methodName);
    responseMessage = "\"No method found\"";
    result = 404;
  }

  *response_size = strlen(responseMessage) + 1;
  *response = (unsigned char *)strdup(responseMessage);

  return result;
}
//
static void
connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result,
                         IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason) {
  switch (reason) {
  case IOTHUB_CLIENT_CONNECTION_EXPIRED_SAS_TOKEN:
    // SASトークンの有効期限切れ。
    ESP_LOGD(TAG, "SAS token expired.");
    if (result == IOTHUB_CLIENT_CONNECTION_UNAUTHENTICATED) {
      //
      // Info: >>>Connection status: timeout
      // Info: >>>Re-connect.
      // Info: Initializing SNTP
      // assertion "Operating mode must not be set while SNTP client is running"
      // failed: file
      // "/home/runner/work/esp32-arduino-lib-builder/esp32-arduino-lib-builder/esp-idf/components/lwip/lwip/src/apps/sntp/sntp.c",
      // line 600, function: sntp_setoperatingmode abort() was called at PC
      // 0x401215bf on core 1
      //
      // Esp32MQTTClient 側で再接続時に以上のログが出てabortするので,
      // この時点で SNTPを停止しておくことで abort を回避する。
      ESP_LOGD(TAG, "SAS token expired, stop the SNTP.");
      sntp_stop();
    }
    break;
  case IOTHUB_CLIENT_CONNECTION_DEVICE_DISABLED:
    ESP_LOGE(TAG, "IOTHUB_CLIENT_CONNECTION_DEVICE_DISABLED");
    break;
  case IOTHUB_CLIENT_CONNECTION_BAD_CREDENTIAL:
    ESP_LOGE(TAG, "IOTHUB_CLIENT_CONNECTION_BAD_CREDENTIAL");
    break;
  case IOTHUB_CLIENT_CONNECTION_RETRY_EXPIRED:
    ESP_LOGE(TAG, "IOTHUB_CLIENT_CONNECTION_RETRY_EXPIRED");
    break;
  case IOTHUB_CLIENT_CONNECTION_NO_NETWORK:
    ESP_LOGE(TAG, "IOTHUB_CLIENT_CONNECTION_NO_NETWORK");
    break;
  case IOTHUB_CLIENT_CONNECTION_COMMUNICATION_ERROR:
    ESP_LOGE(TAG, "IOTHUB_CLIENT_CONNECTION_COMMUNICATION_ERROR");
    break;
  case IOTHUB_CLIENT_CONNECTION_OK:
    ESP_LOGD(TAG, "IOTHUB_CLIENT_CONNECTION_OK");
    break;
  }
}
//
static void deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState,
                               const unsigned char *payLoad, int size) {
  switch (updateState) {
  case DEVICE_TWIN_UPDATE_COMPLETE:
    ESP_LOGD(TAG, "device_twin_update_complete");
    break;

  case DEVICE_TWIN_UPDATE_PARTIAL:
    ESP_LOGD(TAG, "device_twin_update_partial");
    break;
  }

  // Display Twin message.
  char *buff = (char *)calloc(size + 1, sizeof(char));
  if (!buff) {
    ESP_LOGE(TAG, "memory allocation error");
    return;
  }

  strncpy(buff, (const char *)payLoad, size);
  ESP_LOGI(TAG, "%s", buff);

  StaticJsonDocument<MESSAGE_MAX_LEN> json;
  DeserializationError error = deserializeJson(json, buff);
  if (error) {
    ESP_LOGE(TAG, "%s", error.f_str());
    return;
  }
  free(buff);
  //
  /*
  const char *updatedAt = json["reported"]["sgp30_baseline"]["updatedAt"];
  if (updatedAt) {
    ESP_LOGI(TAG, "%s", updatedAt);
    // set baseline
    uint16_t tvoc_baseline =
        json["reported"]["sgp30_baseline"]["tvoc"].as<uint16_t>();
    uint16_t eCo2_baseline =
        json["reported"]["sgp30_baseline"]["eCo2"].as<uint16_t>();
    ESP_LOGD(TAG, "eCo2:%d, TVOC:%d", eCo2_baseline, tvoc_baseline);
    int ret =
        system_properties.sgp30.setIAQBaseline(eCo2_baseline, tvoc_baseline);
    ESP_LOGD(TAG, "setIAQBaseline():%d", ret);
  }
  */
}

//
//
//
IotHubClient::IotHubClient() {}
//
//
//
IotHubClient::~IotHubClient() { Esp32MQTTClient_Close(); }

//
//
//
bool IotHubClient::begin(const std::string &conn_str) {
  // copy
  connection_string = conn_str;
  //
  Esp32MQTTClient_SetOption(OPTION_MINI_SOLUTION_NAME, "GetStarted");
  Esp32MQTTClient_Init((const uint8_t *)connection_string.c_str(), true);

  Esp32MQTTClient_SetSendConfirmationCallback(sendConfirmationCallback);
  Esp32MQTTClient_SetMessageCallback(messageCallback);
  Esp32MQTTClient_SetDeviceTwinCallback(deviceTwinCallback);
  Esp32MQTTClient_SetDeviceMethodCallback(deviceMethodCallback);
  Esp32MQTTClient_SetConnectionStatusCallback(connectionStatusCallback);
  return true;
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
