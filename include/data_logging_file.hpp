// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once

#include "sensor.hpp"
#include <FS.h>
#include <string>

//
//
//
class DataLoggingFile final {
  std::string_view data_fname;
  std::string_view header_fname;
  File data_logging_file;
  bool _ready;

public:
  constexpr static auto FILENAME_MAX_LEN = std::size_t{50};
  DataLoggingFile(std::string_view datafilename,
                  std::string_view headerfilename)
      : data_fname{datafilename}, header_fname{headerfilename}, _ready{false} {}
  bool begin();
  bool ready() { return _ready; }
  void write_data_to_log_file(time_t at, const TempHumiPres &, const TvocEco2 &,
                              const Co2TempHumi &);
  void write_header_to_log_file();
};
