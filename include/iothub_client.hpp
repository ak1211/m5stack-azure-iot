// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef IOTHUB_CLIENT_HPP
#define IOTHUB_CLIENT_HPP

#include <Arduinojson.h>
#include "Esp32MQTTClient.h"
#include "bme280_sensor.hpp"
#include "sgp30_sensor.hpp"

// iso8601 format.
// "2021-02-11T00:56:00.000+00:00"
//  12345678901234567890123456789
#define AT_FIELD_MAX_LENGTH 29

static const size_t MESSAGE_MAX_LEN = 1024;

inline JsonDocument &mapToJson(JsonDocument &output, const Bme280::TempHumiPres &input)
{
    char at[AT_FIELD_MAX_LENGTH + 1];
    struct tm utc;

    gmtime_r(&input.at, &utc);
    //
    strftime(at, AT_FIELD_MAX_LENGTH, "%Y-%m-%dT%H:%M:%SZ", &utc);
    //
    output["sensorId"] = input.sensor_id;
    output["measuredAt"] = at;
    output["temperature"] = input.temperature;
    output["humidity"] = input.relative_humidity;
    output["pressure"] = input.pressure;
    return output;
}

inline JsonDocument &mapToJson(JsonDocument &output, const Sgp30::TvocEco2 &input)
{
    char at[AT_FIELD_MAX_LENGTH + 1];
    struct tm utc;

    gmtime_r(&input.at, &utc);
    //
    strftime(at, AT_FIELD_MAX_LENGTH, "%Y-%m-%dT%H:%M:%SZ", &utc);
    //
    output["sensorId"] = input.sensor_id;
    output["measuredAt"] = at;
    output["tvoc"] = input.tvoc;
    output["eCo2"] = input.eCo2;
    output["tvoc_baseline"] = input.tvoc_baseline;
    output["eCo2_baseline"] = input.eCo2_baseline;
    return output;
}

//
//
//
class IotHubClient
{
public:
    static void init();
    static void terminate();
    static JsonDocument *push(JsonDocument &v);
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

private:
    static void sendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result);
    static void messageCallback(const char *payLoad, int size);
    static void deviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size);
    static int deviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size);
    static void connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason);
};

#undef AT_FIELD_MAX_LENGTH

#endif // IOTHUB_CLIENT_HPP
