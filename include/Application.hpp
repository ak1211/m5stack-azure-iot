// Copyright (c) 2022 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "Database.hpp"
#include <algorithm>

//
namespace Application {
//
// 起動時のログ
//
class BootLog {
  std::vector<char> message_cstring{'\0'};

public:
  //
  void logging(std::string_view sv) noexcept {
    message_cstring.pop_back();
    std::copy(sv.begin(), sv.end(), std::back_inserter(message_cstring));
    message_cstring.push_back('\n');
    message_cstring.push_back('\0');
  }
  //
  const char *c_str() const noexcept { return message_cstring.data(); }
  //
  size_t size() const noexcept { return message_cstring.size(); }
};

extern BootLog boot_log;

//
// constexpr static auto sqlite3_file_name =
// std::string_view{"/sd/measurements.sqlite3"};
extern Database measurements_database;

// 各センサーの測定値の記録数
constexpr static auto HISTORY_BUFFER_SIZE = 180;

} // namespace Application
