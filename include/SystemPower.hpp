// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "value_types.hpp"
#include <M5Unified.h>
#include <tuple>

namespace SystemPower {
// 初期化
extern void init() noexcept;
// 外部電源 / バッテリー電源
enum class PowerSource { External, Internal };
inline PowerSource getPowerSource() noexcept {

  return M5.Power.Axp192.isACIN() ? PowerSource::External
                                  : PowerSource::Internal;
}
//
using BatteryLevel = int8_t;
// バッテリー電圧
extern MilliVoltage getBatteryVoltage() noexcept;
// バッテリー残量
extern BatteryLevel getBatteryLevel() noexcept;
// バッテリー充電中 / 放電中
enum class BatteryCurrentDirection { Charging, Discharging };
//
extern std::pair<MilliAmpere, BatteryCurrentDirection>
getBatteryCurrent() noexcept;
} // namespace SystemPower