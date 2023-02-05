// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "Sensor.hpp"
#include <FS.h>
#include <chrono>
#include <string>

//
//
//
class DataLoggingFile final {
  std::string_view data_fname;
  std::string_view header_fname;
  File data_logging_file;

public:
  DataLoggingFile(std::string_view datafilename,
                  std::string_view headerfilename)
      : data_fname{datafilename}, header_fname{headerfilename} {}
  ~DataLoggingFile();
  bool init();
  bool available() noexcept { return data_logging_file.available(); }
  void write_data_to_log_file(std::chrono::system_clock::time_point at,
                              const Sensor::Bme280 &, const Sensor::Sgp30 &,
                              const Sensor::Scd30 &) noexcept;
  void write_header_to_log_file() noexcept;
};
