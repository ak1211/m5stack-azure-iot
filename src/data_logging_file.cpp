// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "data_logging_file.hpp"
#include <SD.h>

constexpr static const char *TAG = "LoggingModule";

//
//
//
bool DataLoggingFile::begin() {
  _ready = false;
  switch (SD.cardType()) {
  case CARD_MMC: /* fallthrough */
  case CARD_SD:  /* fallthrough */
  case CARD_SDHC: {
    write_header_to_log_file();
    data_logging_file = SD.open(data_fname.c_str(), FILE_APPEND);
    _ready = true;
    break;
  }
  case CARD_NONE: /* fallthrough */
  case CARD_UNKNOWN:
    ESP_LOGD(TAG, "No MEMORY CARD found.");
    break;
  }
  return _ready;
}

//
//
//
void DataLoggingFile::write_data_to_log_file(time_t at, const TempHumiPres &bme,
                                             const TvocEco2 &sgp,
                                             const Co2TempHumi &scd) {
  struct tm utc;
  gmtime_r(&at, &utc);

  const size_t LENGTH = 1024;
  char *p = (char *)calloc(LENGTH + 1, sizeof(char));
  if (p) {
    size_t i;
    i = 0;
    // first field is date and time
    i += strftime(&p[i], LENGTH - i, "%Y-%m-%dT%H:%M:%SZ", &utc);
    // 2nd field is temperature
    i += snprintf(&p[i], LENGTH - i, ", %6.2f", bme.temperature.value);
    // 3nd field is relative_humidity
    i += snprintf(&p[i], LENGTH - i, ", %6.2f", bme.relative_humidity.value);
    // 4th field is pressure
    i += snprintf(&p[i], LENGTH - i, ", %7.2f", bme.pressure.value);
    // 5th field is TVOC
    i += snprintf(&p[i], LENGTH - i, ", %5d", sgp.tvoc.value);
    // 6th field is eCo2
    i += snprintf(&p[i], LENGTH - i, ", %5d", sgp.eCo2.value);
    // 7th field is TVOC baseline
    if (sgp.tvoc_baseline.good()) {
      i += snprintf(&p[i], LENGTH - i, ", %5d", sgp.tvoc_baseline.get().value);
    } else {
      i += snprintf(&p[i], LENGTH - i, ",      ");
    }
    // 8th field is eCo2 baseline
    if (sgp.eCo2_baseline.good()) {
      i += snprintf(&p[i], LENGTH - i, ", %5d", sgp.eCo2_baseline.get().value);
    } else {
      i += snprintf(&p[i], LENGTH - i, ",      ");
    }
    // 9th field is co2
    i += snprintf(&p[i], LENGTH - i, ", %5d", scd.co2.value);
    // 10th field is temperature
    i += snprintf(&p[i], LENGTH - i, ", %6.2f", scd.temperature.value);
    // 11th field is relative_humidity
    i += snprintf(&p[i], LENGTH - i, ", %6.2f", scd.relative_humidity.value);

    ESP_LOGD(TAG, "%s", p);

    // write to file
    size_t size = data_logging_file.println(p);
    data_logging_file.flush();
    ESP_LOGD(TAG, "wrote size:%u", size);
  } else {
    ESP_LOGE(TAG, "memory allocation error");
  }

  free(p);
}

//
//
//
void DataLoggingFile::write_header_to_log_file() {
  File f = SD.open(header_fname.c_str(), FILE_WRITE);
  //
  const size_t LENGTH = 1024;
  char *p = (char *)calloc(LENGTH + 1, sizeof(char));
  size_t i;
  i = 0;
  // first field is date and time
  i += snprintf(&p[i], LENGTH - i, "%s", "datetime");
  // 2nd field is temperature
  i += snprintf(&p[i], LENGTH - i, ", %s", "temperature[C]");
  // 3nd field is relative_humidity
  i += snprintf(&p[i], LENGTH - i, ", %s", "humidity[%RH]");
  // 4th field is pressure
  i += snprintf(&p[i], LENGTH - i, ", %s", "pressure[hPa]");
  // 5th field is TVOC
  i += snprintf(&p[i], LENGTH - i, ", %s", "TVOC[ppb]");
  // 6th field is eCo2
  i += snprintf(&p[i], LENGTH - i, ", %s", "eCo2[ppm]");
  // 7th field is TVOC baseline
  i += snprintf(&p[i], LENGTH - i, ", %s", "TVOC baseline");
  // 8th field is eCo2 baseline
  i += snprintf(&p[i], LENGTH - i, ", %s", "eCo2 baseline");
  // 9th field is co2
  i += snprintf(&p[i], LENGTH - i, ", %s", "Co2[ppm]");
  // 10th field is temperature
  i += snprintf(&p[i], LENGTH - i, ", %s", "temperature[C]");
  // 11th field is relative_humidity
  i += snprintf(&p[i], LENGTH - i, ", %s", "humidity[%RH]");

  ESP_LOGD(TAG, "%s", p);

  // write to file
  size_t size = data_logging_file.println(p);
  data_logging_file.flush();
  ESP_LOGD(TAG, "wrote size:%u", size);

  free(p);
  //
  f.close();
}
