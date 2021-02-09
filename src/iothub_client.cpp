// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include <M5Core2.h>
#include <ctime>
#include <Esp32MQTTClient.h>

#include "credentials.h"
#include "iothub_client.hpp"

bool IotHubClient::messageSending;
uint64_t IotHubClient::message_id;

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
void IotHubClient::init()
{
    Esp32MQTTClient_SetOption(OPTION_MINI_SOLUTION_NAME, "GetStarted");
    Esp32MQTTClient_Init((const uint8_t *)Credentials.connection_string, true);

    Esp32MQTTClient_SetSendConfirmationCallback(sendConfirmationCallback);
    Esp32MQTTClient_SetMessageCallback(messageCallback);
    Esp32MQTTClient_SetDeviceTwinCallback(deviceTwinCallback);
    Esp32MQTTClient_SetDeviceMethodCallback(deviceMethodCallback);
}

//
//
//
void IotHubClient::push(const TempHumiPres &v)
{
    const size_t AT_MAX_LEN = 30;
    char *buff = (char *)calloc(AT_MAX_LEN + MESSAGE_MAX_LEN, sizeof(char));
    if (buff == NULL)
    {
        ESP_LOGE("main", "memory allocation error.");
    }
    else
    {
        char *at = &buff[0];
        char *messagePayload = &buff[AT_MAX_LEN];
        struct tm utc;
        {
            time_t at = v.at;
            gmtime_r(&at, &utc);
        }
        //
        int length;
        length = strftime(at, AT_MAX_LEN, "%Y-%m-%dT%H:%M:%SZ", &utc);
        assert(length < AT_MAX_LEN);
        //
        length = snprintf(messagePayload, MESSAGE_MAX_LEN,
                          "{"
                          "\"sensorId\":\"%s\", "
                          "\"messageId\":%llu, "
                          "\"measuredAt\":\"%s\", "
                          "\"temperature\":%.2f, "
                          "\"humidity\":%.2f, "
                          "\"pressure\":%.2f"
                          "}",
                          v.sensor_id,
                          message_id++,
                          at,
                          v.temperature,
                          v.relative_humidity,
                          v.pressure);
        assert(length < MESSAGE_MAX_LEN);
        ESP_LOGD("main", "messagePayload:%s", messagePayload);
        // Send teperature data
        EVENT_INSTANCE *message = Esp32MQTTClient_Event_Generate(messagePayload, MESSAGE);
        Esp32MQTTClient_Event_AddProp(message, "temperatureAlert", "true");
        Esp32MQTTClient_SendEventInstance(message);
    }
    //
    free(buff);
}
