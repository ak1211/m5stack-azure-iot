// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "DataLoggingFile.hpp"
#include "Application.hpp"
#include <SD.h>
#include <chrono>
#include <memory>

#include <M5Unified.h>

using namespace std::chrono;

DataLoggingFile::~DataLoggingFile() { data_logging_file.close(); }

//
bool DataLoggingFile::init() {
  switch (SD.cardType()) {
  case CARD_MMC:
    [[fallthrough]];
  case CARD_SD:
    [[fallthrough]];
  case CARD_SDHC: {
    write_header_to_log_file();
    data_logging_file = SD.open(data_fname.data(), FILE_APPEND);
    break;
  }
  case CARD_NONE:
    [[fallthrough]];
  case CARD_UNKNOWN:
    M5_LOGD("No MEMORY CARD found.");
    break;
  }
  return available();
}

//
void DataLoggingFile::write_data_to_log_file(
    system_clock::time_point at, const Sensor::Bme280 &bme,
    const Sensor::Sgp30 &sgp, const Sensor::Scd30 &scd) noexcept {
  if (!available()) {
    return;
  }
  struct tm utc;
  {
    std::time_t time = system_clock::to_time_t(at);
    gmtime_r(&time, &utc);
  }
  auto buffer = std::array<char, 1024>{'\0'};
  auto itr = buffer.begin();
  std::size_t d = 0, n = 0;

  // first field is date and time
  d = std::distance(itr, buffer.end());
  n = std::strftime(itr, d, "%Y-%m-%dT%H:%M:%SZ", &utc);
  std::advance(itr, n);
  // 2nd field is temperature
  {
    const DegC tCelcius = bme.temperature;
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %6.2f", static_cast<float>(tCelcius.count()));
    std::advance(itr, n);
  }
  // 3nd field is relative_humidity
  {
    const PctRH rh = bme.relative_humidity;
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %6.2f", static_cast<float>(rh.count()));
    std::advance(itr, n);
  }
  // 4th field is pressure
  {
    const HectoPa hpa = bme.pressure;
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %7.2f", static_cast<float>(hpa.count()));
    std::advance(itr, n);
  }
  // 5th field is TVOC
  d = std::distance(itr, buffer.end());
  n = std::snprintf(itr, d, ", %5d", sgp.tvoc.value);
  std::advance(itr, n);
  // 6th field is eCo2
  d = std::distance(itr, buffer.end());
  n = std::snprintf(itr, d, ", %5d", sgp.eCo2.value);
  std::advance(itr, n);
  // 7th field is TVOC baseline
  if (sgp.tvoc_baseline.has_value()) {
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %5d", sgp.tvoc_baseline.value().value);
    std::advance(itr, n);
  } else {
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ",      ");
    std::advance(itr, n);
  }
  // 8th field is eCo2 baseline
  if (sgp.eCo2_baseline.has_value()) {
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %5d", sgp.eCo2_baseline.value().value);
    std::advance(itr, n);
  } else {
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ",      ");
    std::advance(itr, n);
  }
  // 9th field is co2
  d = std::distance(itr, buffer.end());
  n = std::snprintf(itr, d, ", %5d", scd.co2.value);
  std::advance(itr, n);
  // 10th field is temperature
  {
    const DegC tCelcius = scd.temperature;
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %6.2f", static_cast<float>(tCelcius.count()));
    std::advance(itr, n);
  }
  // 11th field is relative_humidity
  {
    const PctRH rh = scd.relative_humidity;
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %6.2f", static_cast<float>(rh.count()));
    std::advance(itr, n);
  }
  *itr++ = '\0';
  M5_LOGD("%s", buffer.data());

  // write to file
  auto size = data_logging_file.println(buffer.data());
  data_logging_file.flush();
  M5_LOGD("wrote size:%u", size);
}

//
void DataLoggingFile::write_header_to_log_file() noexcept {
  File f = SD.open(header_fname.data(), FILE_WRITE);
  if (f.availableForWrite()) {
    //
    auto buffer = std::array<char, 1024>{'\0'};
    auto itr = buffer.begin();
    std::size_t d = 0, n = 0;
    // first field is date and time
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, "%s", "datetime");
    std::advance(itr, n);
    // 2nd field is temperature
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %s", "temperature[C]");
    std::advance(itr, n);
    // 3nd field is relative_humidity
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %s", "humidity[%RH]");
    std::advance(itr, n);
    // 4th field is pressure
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %s", "pressure[hPa]");
    std::advance(itr, n);
    // 5th field is TVOC
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %s", "TVOC[ppb]");
    std::advance(itr, n);
    // 6th field is eCo2
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %s", "eCo2[ppm]");
    std::advance(itr, n);
    // 7th field is TVOC baseline
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %s", "TVOC baseline");
    std::advance(itr, n);
    // 8th field is eCo2 baseline
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %s", "eCo2 baseline");
    std::advance(itr, n);
    // 9th field is co2
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %s", "Co2[ppm]");
    std::advance(itr, n);
    // 10th field is temperature
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %s", "temperature[C]");
    std::advance(itr, n);
    // 11th field is relative_humidity
    d = std::distance(itr, buffer.end());
    n = std::snprintf(itr, d, ", %s", "humidity[%RH]");
    std::advance(itr, n);

    *itr++ = '\0';
    M5_LOGD("%s", buffer.data());

    // write to file
    auto size = data_logging_file.println(buffer.data());
    data_logging_file.flush();
    M5_LOGD("wrote size:%u", size);
  }
  f.close();
}
