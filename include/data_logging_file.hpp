// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef DATA_LOGGING_FILE_HPP
#define DATA_LOGGIING_FILE_HPP

#include "sensor.hpp"
#include <M5Core2.h>
#include <cstring>

//
//
//
class DataLoggingFile {
public:
  constexpr static size_t FILENAME_MAX_LEN = 50;
  DataLoggingFile(const char *datafilename, const char *headerfilename)
      : data_fname{'\0'}, header_fname{'\0'}, _ready{false} {
    strncpy(const_cast<char *>(data_fname), datafilename, FILENAME_MAX_LEN);
    strncpy(const_cast<char *>(header_fname), headerfilename, FILENAME_MAX_LEN);
  }
  bool begin();
  bool ready() { return _ready; }
  void write_data_to_log_file(const Bme280 &bme, const Sgp30 &sgp,
                              const Scd30 &scd);
  void write_header_to_log_file();

private:
  const char data_fname[FILENAME_MAX_LEN + 1];
  const char header_fname[FILENAME_MAX_LEN + 1];

  File data_logging_file;
  bool _ready;
};

#endif // DATA_LOGGING_FILE_HPP
