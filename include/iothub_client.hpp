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
struct Iso8601FormatField
{
    char at[29 + 1];
};
static_assert(sizeof(Iso8601FormatField) == 30, "something went Wrong");

static const size_t MESSAGE_MAX_LEN = JSON_STRING_SIZE(1024);

struct JsonDocSets
{
    StaticJsonDocument<MESSAGE_MAX_LEN> message;
    StaticJsonDocument<MESSAGE_MAX_LEN> state;
};

inline StaticJsonDocument<MESSAGE_MAX_LEN> &takeMessageFromJsonDocSets(JsonDocSets &doc_sets)
{
    return doc_sets.message;
}

inline StaticJsonDocument<MESSAGE_MAX_LEN> &takeStateFromJsonDocSets(JsonDocSets &doc_sets)
{
    return doc_sets.state;
}

inline JsonDocSets &mapToJson(JsonDocSets &output, const Bme280::TempHumiPres &input)
{
    struct Iso8601FormatField at_field = {};
    struct tm utc;

    gmtime_r(&input.at, &utc);
    //
    strftime(at_field.at, sizeof(at_field), "%Y-%m-%dT%H:%M:%SZ", &utc);
    //
    output.message["sensorId"] = input.sensor_id;
    output.message["measuredAt"] = at_field.at;
    output.message["temperature"] = input.temperature;
    output.message["humidity"] = input.relative_humidity;
    output.message["pressure"] = input.pressure;
    //
    output.state.clear();
    //
    return output;
}

inline JsonDocSets &mapToJson(JsonDocSets &output, const Sgp30::TvocEco2 &input)
{
    struct Iso8601FormatField at_field = {};
    struct tm utc;

    gmtime_r(&input.at, &utc);
    //
    strftime(at_field.at, sizeof(at_field), "%Y-%m-%dT%H:%M:%SZ", &utc);
    //
    output.message["sensorId"] = input.sensor_id;
    output.message["measuredAt"] = at_field.at;
    output.message["tvoc"] = input.tvoc;
    output.message["eCo2"] = input.eCo2;
    output.message["tvoc_baseline"] = input.tvoc_baseline;
    output.message["eCo2_baseline"] = input.eCo2_baseline;
    //
    output.state["sgp30_baseline"]["updatedAt"] = at_field.at;
    output.state["sgp30_baseline"]["tvoc"] = input.tvoc_baseline;
    output.state["sgp30_baseline"]["eCo2"] = input.eCo2_baseline;
    //
    return output;
}

//
//
//
class IotHubClient
{
public:
    static void init(
        SEND_CONFIRMATION_CALLBACK send_confirmation_callback,
        MESSAGE_CALLBACK message_callback,
        DEVICE_METHOD_CALLBACK device_method_callback,
        DEVICE_TWIN_CALLBACK device_twin_callback,
        CONNECTION_STATUS_CALLBACK connection_status_callback);
    static void terminate();
    static JsonDocument *pushMessage(JsonDocument &v);
    static JsonDocument *pushState(JsonDocument &v);
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
};

#undef AT_FIELD_MAX_LENGTH

#endif // IOTHUB_CLIENT_HPP
