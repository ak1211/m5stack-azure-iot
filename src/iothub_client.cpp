// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include <Arduinojson.h>
#include <M5Core2.h>
#include <ctime>
#include <Esp32MQTTClient.h>
#include "lwip/apps/sntp.h"

#include "credentials.h"
#include "iothub_client.hpp"

bool IotHubClient::messageSending = true;
uint32_t IotHubClient::message_id = 0;

//
//
//
void IotHubClient::sendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
    if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
    {
        ESP_LOGD("main", "Send Confirmation Callback finished.");
    }
}

//
//
//
void IotHubClient::messageCallback(const char *payLoad, int size)
{
    ESP_LOGD("main", "Message callback:%s", payLoad);
}

//
//
//
void IotHubClient::deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size)
{
    char *temp = (char *)malloc(size + 1);
    if (temp == NULL)
    {
        return;
    }
    memcpy(temp, payLoad, size);
    temp[size] = '\0';
    // Display Twin message.
    ESP_LOGD("main", "%s", temp);
    free(temp);
}

//
//
//
int IotHubClient::deviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size)
{
    ESP_LOGD("main", "Try to invoke method %s", methodName);
    const char *responseMessage = "\"Successfully invoke device method\"";
    int result = 200;

    if (strcmp(methodName, "start") == 0)
    {
        ESP_LOGD("main", "Start sending temperature and humidity data");
        messageSending = true;
    }
    else if (strcmp(methodName, "stop") == 0)
    {
        ESP_LOGD("main", "Stop sending temperature and humidity data");
        messageSending = false;
    }
    else
    {
        ESP_LOGD("main", "No method %s found", methodName);
        responseMessage = "\"No method found\"";
        result = 404;
    }

    *response_size = strlen(responseMessage) + 1;
    *response = (unsigned char *)strdup(responseMessage);

    return result;
}

//
//
//
void IotHubClient::connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason)
{
    switch (reason)
    {
    case IOTHUB_CLIENT_CONNECTION_EXPIRED_SAS_TOKEN:
        // SASトークンの有効期限切れ。
        ESP_LOGD("main", "SAS token expired.");
        if (result == IOTHUB_CLIENT_CONNECTION_UNAUTHENTICATED)
        {
            //
            // Info: >>>Connection status: timeout
            // Info: >>>Re-connect.
            // Info: Initializing SNTP
            // assertion "Operating mode must not be set while SNTP client is running" failed: file "/home/runner/work/esp32-arduino-lib-builder/esp32-arduino-lib-builder/esp-idf/components/lwip/lwip/src/apps/sntp/sntp.c", line 600, function: sntp_setoperatingmode
            // abort() was called at PC 0x401215bf on core 1
            //
            // Esp32MQTTClient 側で再接続時に以上のログが出てabortするので,
            // この時点で SNTPを停止しておくことで abort を回避する。
            ESP_LOGD("main", "SAS token expired, stop the SNTP.");
            sntp_stop();
        }
        break;
    case IOTHUB_CLIENT_CONNECTION_DEVICE_DISABLED:
        break;
    case IOTHUB_CLIENT_CONNECTION_BAD_CREDENTIAL:
        break;
    case IOTHUB_CLIENT_CONNECTION_RETRY_EXPIRED:
        break;
    case IOTHUB_CLIENT_CONNECTION_NO_NETWORK:
        break;
    case IOTHUB_CLIENT_CONNECTION_COMMUNICATION_ERROR:
        break;
    case IOTHUB_CLIENT_CONNECTION_OK:
        break;
    }
}

//
//
//
void IotHubClient::init()
{
    Esp32MQTTClient_SetOption(OPTION_MINI_SOLUTION_NAME, "GetStarted");
    Esp32MQTTClient_Init((const uint8_t *)Credentials.connection_string, true);

    Esp32MQTTClient_SetSendConfirmationCallback(sendConfirmationCallback);
    Esp32MQTTClient_SetMessageCallback(messageCallback);
    Esp32MQTTClient_SetDeviceTwinCallback(deviceTwinCallback);
    Esp32MQTTClient_SetDeviceMethodCallback(deviceMethodCallback);
    Esp32MQTTClient_SetConnectionStatusCallback(connectionStatusCallback);
}

//
//
//
JsonDocument *IotHubClient::push(JsonDocument &doc)
{
    char *messagePayload = (char *)calloc(MESSAGE_MAX_LEN + 1, sizeof(char));
    if (messagePayload == NULL)
    {
        ESP_LOGE("main", "memory allocation error.");
        return NULL;
    }
    doc["messageId"] = message_id;
    serializeJson(doc, messagePayload, MESSAGE_MAX_LEN);
    ESP_LOGD("main", "messagePayload:%s", messagePayload);
    message_id = message_id + 1;
    // Send teperature data
    EVENT_INSTANCE *message = Esp32MQTTClient_Event_Generate(messagePayload, MESSAGE);
    Esp32MQTTClient_Event_AddProp(message, "temperatureAlert", "true");
    Esp32MQTTClient_SendEventInstance(message);
    free(messagePayload);
    return &doc;
}
