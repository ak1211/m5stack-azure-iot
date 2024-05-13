// Copyright (c) 2022 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "Application.hpp"
#include "Gui.hpp"
#include "credentials.h"
#include "esp_sntp.h"
#include <ArduinoOTA.h>
#include <LITTLEFS.h>
#include <WiFi.h>
#include <chrono>
#include <functional>
#include <future>
#include <lvgl.h>

#include <M5Unified.h>

using namespace std::literals::string_literals;
using namespace std::chrono;

//
Application *Application::_instance{nullptr};

//
const steady_clock::time_point Application::_application_start_time{
    steady_clock::now()};

// 起動
bool Application::startup() {
  //
  struct BufWithLogging : public std::stringbuf {
    Application *pApp;
    BufWithLogging(Application *p) : pApp{p} {}
    virtual int sync() {
      std::istringstream iss{this->str()};
      std::string work;
      std::string tail;
      while (std::getline(iss, work)) {
        tail = work;
      }
      pApp->_startup_log += tail;
      pApp->_startup_log.push_back('\n');
      pApp->getGui().update_startup_message(tail);
      return std::stringbuf::sync();
    }
  };
  BufWithLogging buf{this};
  std::ostream oss(&buf);
  // 起動シーケンス
  std::vector<std::function<bool(std::ostream &)>> startup_sequence{
      std::bind(&Application::start_wifi, this, std::placeholders::_1),
      std::bind(&Application::synchronize_ntp, this, std::placeholders::_1),
      std::bind(&Application::start_telemetry, this, std::placeholders::_1),
      std::bind(&Application::start_database, this, std::placeholders::_1),
      std::bind(&Application::start_sensor_BME280, this, std::placeholders::_1),
      std::bind(&Application::start_sensor_SGP30, this, std::placeholders::_1),
      std::bind(&Application::start_sensor_SCD30, this, std::placeholders::_1),
      std::bind(&Application::start_sensor_SCD41, this, std::placeholders::_1),
      std::bind(&Application::start_sensor_M5ENV3, this, std::placeholders::_1),
  };
  if (M5.Rtc.isEnabled()) {
    // RTCより時刻復帰する
    m5::rtc_datetime_t rtc = M5.Rtc.getDateTime();
    struct tm local_tm {};
    local_tm.tm_hour = rtc.time.hours;
    local_tm.tm_min = rtc.time.minutes;
    local_tm.tm_sec = rtc.time.seconds;
    local_tm.tm_year = rtc.date.year - 1900;
    local_tm.tm_mon = rtc.date.month - 1;
    local_tm.tm_mday = rtc.date.date;
    local_tm.tm_wday = rtc.date.weekDay;
    struct timespec timespec {
      .tv_sec = mktime(&local_tm), .tv_nsec = 0
    };
    clock_settime(CLOCK_REALTIME, &timespec);
  }

  // LED
  _rgb_led.begin();
  _rgb_led.setBrightness(50);

  // Display
  M5.Display.setColorDepth(LV_COLOR_DEPTH);
  M5.Display.setBrightness(160);

  // init GUI
  if (!_gui.begin()) {
    M5.Display.print("GUI inititalize failed.");
    return false;
  }

  // create RTOS task
  static TaskHandle_t rtos_task_handle{};
  xTaskCreatePinnedToCore(
      [](void *arg) -> void {
        while (true) {
          lv_timer_handler();
          std::this_thread::sleep_for(10ms);
        }
      },
      "Task:LVGL", 4096, nullptr, 5, &rtos_task_handle, ARDUINO_RUNNING_CORE);

  //
  int16_t timer_arg = 0;
  auto timer = lv_timer_create(
      [](lv_timer_t *t) -> void {
        assert(t);
        auto pValue = static_cast<int16_t *>(t->user_data);
        Application::getRgbLed().fill(RgbLed::hslToRgb(*pValue, 1, 0.5));
        *pValue = (*pValue + 1) % 360;
      },
      10, &timer_arg);
  // 起動
  for (auto it = startup_sequence.begin(); it != startup_sequence.end(); ++it) {
    std::this_thread::sleep_for(100ms);
    auto func = *it;
    auto percent = 100 * std::distance(startup_sequence.begin(), it) /
                   startup_sequence.size();
    _gui.update_startup_progress(percent);
    //
    func(oss);
  }
  _gui.update_startup_progress(100);
  std::this_thread::sleep_for(100ms);
  lv_timer_del(timer);
  _rgb_led.clear();
  //
  _gui.startUi();

  return true;
}

//
bool Application::start_wifi(std::ostream &os) {
  os << "connect to WiFi \""s << Credentials.wifi_ssid << "\""s << std::endl;
  M5_LOGI("connect to WiFi AP.");
  //  WiFi with Station mode
  WiFi.onEvent(wifi_event_callback);
  WiFi.mode(WIFI_STA);
  WiFi.begin(Credentials.wifi_ssid, Credentials.wifi_password);
  // WiFi APとの接続待ち
  auto timeover{steady_clock::now() + TIMEOUT};
  while (WiFi.status() != WL_CONNECTED && steady_clock::now() < timeover) {
    std::this_thread::sleep_for(1s);
  }

  // Over The Air update
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA
        .onStart([]() {
          String type;
          if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
          else // U_SPIFFS
            type = "filesystem";

          // NOTE: if updating SPIFFS this would be the place to unmount
          // SPIFFS using SPIFFS.end()
          Serial.println("Start updating " + type);
        })
        .onEnd([]() { Serial.println("\nEnd"); })
        .onProgress([](unsigned int progress, unsigned int total) {
          Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        })
        .onError([](ota_error_t error) {
          Serial.printf("Error[%u]: ", error);
          if (error == OTA_AUTH_ERROR)
            Serial.println("Auth Failed");
          else if (error == OTA_BEGIN_ERROR)
            Serial.println("Begin Failed");
          else if (error == OTA_CONNECT_ERROR)
            Serial.println("Connect Failed");
          else if (error == OTA_RECEIVE_ERROR)
            Serial.println("Receive Failed");
          else if (error == OTA_END_ERROR)
            Serial.println("End Failed");
        });

    ArduinoOTA.begin();
  }

  return WiFi.status() == WL_CONNECTED;
}

//
void Application::time_sync_notification_callback(struct timeval *time_val) {
  // 時刻をRTCに設定する
  struct tm local_tm {};
  if (getLocalTime(&local_tm)) {
    M5.Rtc.setTime(m5::rtc_time_t{local_tm});
    M5.Rtc.setDate(m5::rtc_date_t{local_tm});
  }
  //
  Application::getInstance()._time_is_synced = true;
}

// インターネット時間サーバと同期する
bool Application::synchronize_ntp(std::ostream &os) {
  //
  if (_time_is_synced) {
    return true;
  }
  //
  os << "synchronize time server." << std::endl;
  M5_LOGI("synchronize time server.");
  // インターネット時間に同期する
  sntp_set_time_sync_notification_cb(time_sync_notification_callback);
  configTzTime(TZ_TIME_ZONE.data(), "time.cloudflare.com",
               "ntp.jst.mfeed.ad.jp", "ntp.nict.jp");
  //
  return true;
}

//
bool Application::start_telemetry(std::ostream &os) {
  os << "start Telemetry." << std::endl;
  M5_LOGI("start Telemetry.");
  //
  if (_telemetry.begin(Credentials.iothub_fqdn, Credentials.device_id,
                       Credentials.device_key) == false) {
    os << "MQTT subscribe failed." << std::endl;
    return false;
  }
  //
  os << "waiting for Telemetry connection." << std::endl;
  //
  auto timeover{steady_clock::now() + TIMEOUT};
  while (_telemetry.isConnected() == false && steady_clock::now() < timeover) {
    std::this_thread::sleep_for(1s);
  }
  return _telemetry.isConnected();
}

//
bool Application::start_database(std::ostream &os) {
  os << "start database." << std::endl;
  M5_LOGI("start database.");
  //
  for (auto count = 1; count <= 2; ++count) {
    if (Application::_measurements_database.begin(
            std::string(MEASUREMENTS_DATABASE_FILE_NAME))) {
      return true;
    }
    // 失敗したらデータベースファイルを消去して再度初期化
    if (!LittleFS.remove(MEASUREMENTS_DATABASE_FILE_NAME.data())) {
      // ファイルの消去に失敗したらフォーマットする
      M5_LOGI("format filesystem.");
      if (LittleFS.format() == false) {
        M5_LOGE("format failed.");
      }
    } else {
      M5_LOGI("file \"%s\" removed.", MEASUREMENTS_DATABASE_FILE_NAME.data());
    }
  }
  //
  if (Application::_measurements_database.available()) {
    os << "Database is available." << std::endl;
    return true;
  } else {
    os << "Database is not available." << std::endl;
    M5_LOGE("Database is not available.");
    return false;
  }
}

//
bool Application::start_sensor_BME280(std::ostream &os) {
  //
  os << "start BME280 sensor." << std::endl;
  M5_LOGI("start BME280 sensor.");
  //
  std::unique_ptr<Sensor::Device> pdev = std::make_unique<Sensor::Bme280Device>(
      SENSOR_DESCRIPTOR_BME280, BME280_I2C_ADDRESS, Wire);
  if (pdev) {
    auto timeover{steady_clock::now() + TIMEOUT};
    while (pdev->init() == false && steady_clock::now() < timeover) {
      std::this_thread::sleep_for(1s);
    }
    if (pdev->available()) {
      _sensors.push_back(std::move(pdev));
      return true;
    }
  }
  //
  os << "BME280 sensor not found." << std::endl;
  M5_LOGE("BME280 sensor not found.");
  return false;
}

//
bool Application::start_sensor_SGP30(std::ostream &os) {
  //
  os << "start SGP30 sensor." << std::endl;
  M5_LOGI("start SGP30 sensor.");
  //
  std::unique_ptr<Sensor::Device> pdev =
      std::make_unique<Sensor::Sgp30Device>(SENSOR_DESCRIPTOR_SGP30, Wire);
  if (pdev) {
    auto timeover{steady_clock::now() + TIMEOUT};
    while (pdev->init() == false && steady_clock::now() < timeover) {
      std::this_thread::sleep_for(1s);
    }
    if (pdev->available()) {
      _sensors.push_back(std::move(pdev));
      return true;
    }
  }
  //
  os << "SGP30 sensor not found." << std::endl;
  M5_LOGE("SGP30 sensor not found.");
  return false;
}

//
bool Application::start_sensor_SCD30(std::ostream &os) {
  //
  os << "start SCD30 sensor." << std::endl;
  M5_LOGI("start SCD30 sensor.");
  //
  std::unique_ptr<Sensor::Device> pdev =
      std::make_unique<Sensor::Scd30Device>(SENSOR_DESCRIPTOR_SCD30, Wire);
  if (pdev) {
    auto timeover{steady_clock::now() + TIMEOUT};
    while (pdev->init() == false && steady_clock::now() < timeover) {
      std::this_thread::sleep_for(1s);
    }
    if (pdev->available()) {
      _sensors.push_back(std::move(pdev));
      return true;
    }
  }
  //
  os << "SCD30 sensor not found." << std::endl;
  M5_LOGE("SCD30 sensor not found.");
  return false;
}

//
bool Application::start_sensor_SCD41(std::ostream &os) {
  //
  os << "start SCD41 sensor." << std::endl;
  M5_LOGI("start SCD41 sensor.");
  //
  std::unique_ptr<Sensor::Device> pdev =
      std::make_unique<Sensor::Scd41Device>(SENSOR_DESCRIPTOR_SCD41, Wire);
  if (pdev) {
    auto timeover{steady_clock::now() + TIMEOUT};
    while (pdev->init() == false && steady_clock::now() < timeover) {
      std::this_thread::sleep_for(1s);
    }
    if (pdev->available()) {
      _sensors.push_back(std::move(pdev));
      return true;
    }
  }
  //
  os << "SCD41 sensor not found." << std::endl;
  M5_LOGE("SCD41 sensor not found.");
  return false;
}

//
bool Application::start_sensor_M5ENV3(std::ostream &os) {
  //
  os << "start ENV.III sensor." << std::endl;
  M5_LOGI("start ENV.III sensor.");
  //
  std::unique_ptr<Sensor::Device> pdev = std::make_unique<Sensor::M5Env3Device>(
      SENSOR_DESCRIPTOR_M5ENV3, Wire, M5.Ex_I2C.getSDA(), M5.Ex_I2C.getSCL());
  if (pdev) {
    auto timeover{steady_clock::now() + TIMEOUT};
    while (pdev->init() == false && steady_clock::now() < timeover) {
      std::this_thread::sleep_for(1s);
    }
    if (pdev->available()) {
      _sensors.push_back(std::move(pdev));
      return true;
    }
  }
  //
  os << "ENV.III sensor not found." << std::endl;
  M5_LOGE("ENV.III sensor not found.");
  return false;
}

// iso8601 format.
std::string Application::isoformatUTC(std::time_t utctime) {
  struct tm tm;
  gmtime_r(&utctime, &tm);
  std::ostringstream oss;
  oss << std::put_time(&tm, "%FT%TZ");
  return oss.str().c_str();
}

//
void Application::wifi_event_callback(WiFiEvent_t event) {
  switch (event) {
  case SYSTEM_EVENT_AP_START:
    M5_LOGI("AP Started");
    break;
  case SYSTEM_EVENT_AP_STOP:
    M5_LOGI("AP Stopped");
    break;
  case SYSTEM_EVENT_STA_START:
    M5_LOGI("STA Started");
    break;
  case SYSTEM_EVENT_STA_CONNECTED:
    M5_LOGI("STA Connected");
    break;
  case SYSTEM_EVENT_AP_STA_GOT_IP6: {
    auto localipv6 = WiFi.localIPv6();
    M5_LOGI("STA IPv6: %s", localipv6.toString());
  } break;
  case SYSTEM_EVENT_STA_GOT_IP: {
    auto localip = WiFi.localIP();
    M5_LOGI("STA IPv4: %s", localip.toString());
  } break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    M5_LOGI("STA Disconnected");
    break;
  case SYSTEM_EVENT_STA_STOP:
    M5_LOGI("STA Stopped");
    break;
  default:
    break;
  }
}
