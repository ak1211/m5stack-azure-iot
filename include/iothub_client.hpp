// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef IOTHUB_CLIENT_HPP
#define IOTHUB_CLIENT_HPP

#include "Esp32MQTTClient.h"
#include "sensor.hpp"

//
//
//
class IotHubClient
{
public:
    static void init();
    static void terminate();
    static void push(const TempHumiPres &v);
    static inline void update(bool hasDelay)
    {
        Esp32MQTTClient_Check(hasDelay);
    }
    //
    ~IotHubClient()
    {
        Esp32MQTTClient_Close();
    }
    //
    static bool messageSending;
    static uint32_t message_id;
    static const size_t MESSAGE_MAX_LEN = 512;

private:
    static void sendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result);
    static void messageCallback(const char *payLoad, int size);
    static void deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size);
    static int deviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size);
    static void connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason);
};

#endif // IOTHUB_CLIENT_HPP
