// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "Sensor.hpp"
#include <queue>
#include <string>
#include <variant>

// MQTT通信
namespace Telemetry {
constexpr static auto MAXIMUM_QUEUE_SIZE = 500;
using MessageId = uint32_t;
// 送信用
using Payload =
    std::variant<MeasurementBme280, MeasurementSgp30, MeasurementScd30,
                 MeasurementScd41, MeasurementM5Env3>;
// 送信用メッセージに変換する
template <typename T>
std::string to_json_message(MessageId messageId, const T &in);
// 送信用バッファ
extern std::queue<Payload> sending_fifo_queue;
//
extern bool init(std::string_view iothub_fqdn, std::string_view device_id,
                 std::string_view device_key);
//
extern bool loopMqtt();
//
inline bool pushMessage(const Payload &&in) {
  if (sending_fifo_queue.size() < MAXIMUM_QUEUE_SIZE) {
    sending_fifo_queue.push(in);
    return true;
  }
  return false;
}

} // namespace Telemetry
