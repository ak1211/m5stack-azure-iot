// Copyright (c) 2022 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "Application.hpp"
#include "Gui.hpp"
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <SD.h>
#include <WiFi.h>
#include <chrono>
#include <esp_sntp.h>
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

//
std::optional<std::string> Application::getSettings_wifi_SSID() {
  if (settings_json.containsKey("wifi")) {
    if (settings_json["wifi"].containsKey("SSID")) {
      return settings_json["wifi"]["SSID"];
    }
  }
  return std::nullopt;
}

//
std::optional<std::string> Application::getSettings_wifi_password() {
  if (settings_json.containsKey("wifi")) {
    if (settings_json["wifi"].containsKey("password")) {
      return settings_json["wifi"]["password"];
    }
  }
  return std::nullopt;
}

//
std::optional<std::string> Application::getSettings_AzureIoTHub_FQDN() {
  if (settings_json.containsKey("AzureIoTHub")) {
    if (settings_json["AzureIoTHub"].containsKey("FQDN")) {
      return settings_json["AzureIoTHub"]["FQDN"];
    }
  }
  return std::nullopt;
}

//
std::optional<std::string> Application::getSettings_AzureIoTHub_DeviceID() {
  if (settings_json.containsKey("AzureIoTHub")) {
    if (settings_json["AzureIoTHub"].containsKey("DeviceID")) {
      return settings_json["AzureIoTHub"]["DeviceID"];
    }
  }
  return std::nullopt;
}

//
std::optional<std::string> Application::getSettings_AzureIoTHub_DeviceKey() {
  if (settings_json.containsKey("AzureIoTHub")) {
    if (settings_json["AzureIoTHub"].containsKey("DeviceKey")) {
      return settings_json["AzureIoTHub"]["DeviceKey"];
    }
  }
  return std::nullopt;
}

//
bool Application::task_handler() {
  ArduinoOTA.handle();
  M5.update();
  if (M5.BtnA.wasPressed()) {
    _gui.movePrev();
  } else if (M5.BtnB.wasPressed()) {
    _gui.home();
  } else if (M5.BtnC.wasPressed()) {
    _gui.moveNext();
  }
  static system_clock::time_point before_db_tp{};
  auto now_tp = system_clock::now();
  // 随時測定する
  _measuring_task.task_handler(now_tp);
  //
  if (now_tp - before_db_tp >= 333s) {
    // データベースの整理
    system_clock::time_point tp = now_tp - minutes{Gui::CHART_X_POINT_COUNT};
    if (_data_acquisition_db.delete_old_measurements_from_database(
            std::chrono::floor<minutes>(tp)) == false) {
      M5_LOGE("delete old measurements failed.");
    }
    before_db_tp = now_tp;
  }
  //
  static steady_clock::time_point before_epoch{};
  auto now_epoch = steady_clock::now();
  if (now_epoch - before_epoch >= 3s) {
    idle_task_handler();
    before_epoch = now_epoch;
  }

  return true;
}

//
void Application::idle_task_handler() {
  if (WiFi.status() != WL_CONNECTED) {
    // WiFiが接続されていない場合は接続する。
    std::string ssid;
    std::string password;
    // guard
    if (getSettings_wifi_SSID()) {
      ssid = getSettings_wifi_SSID().value();
    } else {
      ESP_LOGE("%s", "wifi SSID not set");
      return;
    }
    if (getSettings_wifi_password()) {
      password = getSettings_wifi_password().value();
    } else {
      ESP_LOGE("%s", "wifi password not set");
      return;
    }
    WiFi.begin(ssid.c_str(), password.c_str());
    return;
  }
  if (_telemetry.isConnected()) {
    _telemetry.task_handler();
    return;
  } else {
    static steady_clock::time_point before{steady_clock::now()};
    // 再接続
    if (steady_clock::now() - before > 1min) {
      before = steady_clock::now();
      //
      if (!_telemetry.reconnect()) {
        M5_LOGE("Reconnect telemetry failed.");
      }
    }
    return;
  }
}

// 起動
bool Application::startup() {
  // 起動シーケンス
  std::vector<std::function<bool(std::ostream &)>> startup_sequence{
      std::bind(&Application::read_settings_json, this, std::placeholders::_1),
      std::bind(&Application::start_wifi, this, std::placeholders::_1),
      std::bind(&Application::synchronize_ntp, this, std::placeholders::_1),
      std::bind(&Application::start_database, this, std::placeholders::_1),
      std::bind(&Application::start_telemetry, this, std::placeholders::_1),
      std::bind(&Application::start_sensor_BME280, this, std::placeholders::_1),
      std::bind(&Application::start_sensor_SGP30, this, std::placeholders::_1),
      std::bind(&Application::start_sensor_SCD30, this, std::placeholders::_1),
      std::bind(&Application::start_sensor_SCD41, this, std::placeholders::_1),
      std::bind(&Application::start_sensor_M5ENV3, this, std::placeholders::_1),
  };
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

  // initializing M5Stack Core2 with M5Unified
  {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Power.setVibration(0); // stop the vibration
  }

  // init SD
  while (false == SD.begin(GPIO_NUM_4, SPI, 25000000)) {
    delay(500);
  }

  //
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

  // file system init
  if (LittleFS.begin(true)) {
    M5_LOGI("filesystem status : %lu / %lu.", LittleFS.usedBytes(),
            LittleFS.totalBytes());
  } else {
    M5_LOGE("filesystem inititalize failed.");
    M5.Display.print("filesystem inititalize failed.");
    std::this_thread::sleep_for(1min);
    esp_system_abort("filesystem inititalize failed.");
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

  // create RTOS task for LVGL
  xTaskCreatePinnedToCore(
      [](void *arg) -> void {
        while (true) {
          lv_task_handler();
          std::this_thread::sleep_for(10ms);
        }
      },
      "Task:LVGL", LVGL_TASK_STACK_SIZE, nullptr, 5, &_rtos_lvgl_task_handle,
      ARDUINO_RUNNING_CORE);

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

  // プログレスバーを表示しながら起動
  for (auto it = startup_sequence.begin(); it != startup_sequence.end(); ++it) {
    std::this_thread::sleep_for(100ms);
    auto func = *it;
    auto percent = 100 * std::distance(startup_sequence.begin(), it) /
                   startup_sequence.size();
    _gui.update_startup_progress(percent);
    //
    func(oss);
  }
  _gui.update_startup_progress(100); // 100%
  std::this_thread::sleep_for(100ms);
  lv_timer_del(timer);
  _rgb_led.clear();

  //
  if constexpr (false) {
    while (!isTimeSynced()) {
      std::this_thread::sleep_for(100ms);
    }
    std::this_thread::sleep_for(1s);
    std::unique_ptr<Sensor::Device> pdev =
        std::make_unique<Sensor::Bme280Device>(SENSOR_DESCRIPTOR_BME280,
                                               BME280_I2C_ADDRESS, Wire);
    _sensors.push_back(std::move(pdev));
    std::unique_ptr<Sensor::Device> pdev2 =
        std::make_unique<Sensor::M5Env3Device>(SENSOR_DESCRIPTOR_M5ENV3, Wire,
                                               32, 33);
    _sensors.push_back(std::move(pdev2));
    auto tp = floor<minutes>(system_clock::now());
    Sensor::Bme280 a{SENSOR_DESCRIPTOR_BME280, CentiDegC{1100}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 b{SENSOR_DESCRIPTOR_BME280, CentiDegC{1200}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 c{SENSOR_DESCRIPTOR_BME280, CentiDegC{1300}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 d{SENSOR_DESCRIPTOR_BME280, CentiDegC{1400}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 e{SENSOR_DESCRIPTOR_BME280, CentiDegC{1600}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 f{SENSOR_DESCRIPTOR_BME280, CentiDegC{1700}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 g{SENSOR_DESCRIPTOR_BME280, CentiDegC{1800}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 h{SENSOR_DESCRIPTOR_BME280, CentiDegC{1900}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 i{SENSOR_DESCRIPTOR_BME280, CentiDegC{2000}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 j{SENSOR_DESCRIPTOR_BME280, CentiDegC{2100}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 k{SENSOR_DESCRIPTOR_BME280, CentiDegC{2200}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 l{SENSOR_DESCRIPTOR_BME280, CentiDegC{2300}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 m{SENSOR_DESCRIPTOR_BME280, CentiDegC{2400}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 n{SENSOR_DESCRIPTOR_BME280, CentiDegC{2500}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 o{SENSOR_DESCRIPTOR_BME280, CentiDegC{2600}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 p{SENSOR_DESCRIPTOR_BME280, CentiDegC{2700}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 q{SENSOR_DESCRIPTOR_BME280, CentiDegC{2800}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 r{SENSOR_DESCRIPTOR_BME280, CentiDegC{2900}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 s{SENSOR_DESCRIPTOR_BME280, CentiDegC{3000}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 t{SENSOR_DESCRIPTOR_BME280, CentiDegC{3100}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 u{SENSOR_DESCRIPTOR_BME280, CentiDegC{3200}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 v{SENSOR_DESCRIPTOR_BME280, CentiDegC{3300}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 w{SENSOR_DESCRIPTOR_BME280, CentiDegC{3400}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 x{SENSOR_DESCRIPTOR_BME280, CentiDegC{3500}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 y{SENSOR_DESCRIPTOR_BME280, CentiDegC{3600}, CentiRH{5000},
                     DeciPa{1000}};
    Sensor::Bme280 z{SENSOR_DESCRIPTOR_BME280, CentiDegC{3700}, CentiRH{5000},
                     DeciPa{1000}};
    _data_acquisition_db.insert({tp - 25min, a});
    _data_acquisition_db.insert({tp - 24min, b});
    _data_acquisition_db.insert({tp - 23min, c});
    _data_acquisition_db.insert({tp - 22min, d});
    _data_acquisition_db.insert({tp - 21min, e});
    _data_acquisition_db.insert({tp - 20min, f});
    _data_acquisition_db.insert({tp - 19min, g});
    _data_acquisition_db.insert({tp - 18min, h});
    _data_acquisition_db.insert({tp - 17min, i});
    _data_acquisition_db.insert({tp - 16min, j});
    _data_acquisition_db.insert({tp - 15min, k});
    _data_acquisition_db.insert({tp - 14min, l});
    _data_acquisition_db.insert({tp - 13min, m});
    _data_acquisition_db.insert({tp - 12min, n});
    _data_acquisition_db.insert({tp - 11min, o});
    _data_acquisition_db.insert({tp - 10min, p});
    _data_acquisition_db.insert({tp - 9min, q});
    _data_acquisition_db.insert({tp - 8min, r});
    _data_acquisition_db.insert({tp - 7min, s});
    _data_acquisition_db.insert({tp - 6min, t});
    _data_acquisition_db.insert({tp - 5min, u});
    _data_acquisition_db.insert({tp - 4min, v});
    _data_acquisition_db.insert({tp - 3min, w});
    _data_acquisition_db.insert({tp - 2min, x});
    _data_acquisition_db.insert({tp - 1min, y});
    _data_acquisition_db.insert({tp - 0min, z});
  }

  //
  _measuring_task.begin(std::chrono::system_clock::now());

  //
  _gui.startUi();

  // create RTOS task for this Application
  xTaskCreatePinnedToCore(
      [](void *user_context) -> void {
        while (true) {
          Application *app = static_cast<Application *>(user_context);
          app->task_handler();
        }
      },
      "Task:Application", APPLICATION_TASK_STACK_SIZE, this, 1,
      &_rtos_application_task_handle, ARDUINO_RUNNING_CORE);

  return true;
}

//
bool Application::read_settings_json(std::ostream &os) {
  //
  if (auto file = LittleFS.open(SETTINGS_FILE_PATH.data())) {
    DeserializationError error = deserializeJson(settings_json, file);
    if (error == DeserializationError::Ok) {
      std::ostringstream ss;
      ss << "Setting file is \"" << SETTINGS_FILE_PATH << "\"";
      os << ss.str() << std::endl;
      M5_LOGI("%s", ss.str().c_str());
    } else {
      std::ostringstream ss;
      ss << "Error; Read \"" << SETTINGS_FILE_PATH << "\" file.";
      os << ss.str() << std::endl;
      M5_LOGE("%s", ss.str().c_str());
      goto error_halt;
    }
    file.close();
    return true;
  } else {
    std::ostringstream ss;
    ss << "Error; Open \"" << SETTINGS_FILE_PATH << "\" file.";
    os << ss.str() << std::endl;
    M5_LOGE("%s", ss.str().c_str());
    goto error_halt;
  }

error_halt:
  // halted
  while (true) {
    ArduinoOTA.handle();
    M5.update();
    delay(10);
  }
  return false;
}

//
bool Application::start_wifi(std::ostream &os) {
  os << "connect to WiFi" << std::endl;
  M5_LOGI("connect to WiFi");
  //
  std::string ssid;
  std::string password;
  // guard
  if (getSettings_wifi_SSID()) {
    ssid = getSettings_wifi_SSID().value();
  } else {
    std::ostringstream ss;
    ss << "wifi SSID not set";
    os << ss.str() << std::endl;
    M5_LOGE("%s", ss.str());
    return false;
  }
  if (getSettings_wifi_password()) {
    password = getSettings_wifi_password().value();
  } else {
    std::ostringstream ss;
    ss << "wifi password not set";
    os << ss.str() << std::endl;
    M5_LOGE("%s", ss.str());
    return false;
  }
  //
  {
    std::ostringstream ss;
    ss << "WiFi AP SSID\""s << ssid << "\""s;
    os << ss.str() << std::endl;
    M5_LOGI("%s", ss.str());
  }
  //  WiFi with Station mode
  WiFi.onEvent(wifi_event_callback);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
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
  Application::getInstance()->_time_is_synced = true;
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
bool Application::start_database(std::ostream &os) {
  os << "start database." << std::endl;
  M5_LOGI("start database.");
  //
  for (auto count = 1; count <= 2; ++count) {
    if (Application::_data_acquisition_db.begin(
            std::string(DATA_ACQUISITION_DATABASE_FILE_URI))) {
      return true;
    }
    // 失敗したらデータベースファイルを消去して再度初期化
    if (LittleFS.remove(DATA_ACQUISITION_DATABASE_FILE_URI.data())) {
      os << "file \"" << DATA_ACQUISITION_DATABASE_FILE_URI << "\" removed."
         << std::endl;
      M5_LOGI("file \"%s\" removed.",
              DATA_ACQUISITION_DATABASE_FILE_URI.data());
    } else {
      os << "file \"" << DATA_ACQUISITION_DATABASE_FILE_URI
         << "\" remove error." << std::endl;
      M5_LOGE("file \"%s\" remove error.",
              DATA_ACQUISITION_DATABASE_FILE_URI.data());
      return false;
    }
  }
  //
  if (Application::_data_acquisition_db.available()) {
    os << "Database is available." << std::endl;
    return true;
  } else {
    os << "Database is not available." << std::endl;
    M5_LOGE("Database is not available.");
    return false;
  }
}

//
bool Application::start_telemetry(std::ostream &os) {
  {
    std::ostringstream ss;
    ss << "start Telemetry.";
    os << ss.str() << std::endl;
    M5_LOGI("%s", ss.str());
  }
  //
  std::string iothub_fqdn;
  std::string iothub_device_id;
  std::string iothub_device_key;
  // guard
  if (getSettings_AzureIoTHub_FQDN()) {
    iothub_fqdn = getSettings_AzureIoTHub_FQDN().value();
  } else {
    std::ostringstream ss;
    ss << "Azure IoT Hub FQDN not set";
    os << ss.str() << std::endl;
    M5_LOGE("%s", ss.str());
    return false;
  }
  if (getSettings_AzureIoTHub_DeviceID()) {
    iothub_device_id = getSettings_AzureIoTHub_DeviceID().value();
  } else {
    std::ostringstream ss;
    ss << "Azure IoT Hub DeviceID not set";
    os << ss.str() << std::endl;
    M5_LOGE("%s", ss.str());
    return false;
  }
  if (getSettings_AzureIoTHub_DeviceKey()) {
    iothub_device_key = getSettings_AzureIoTHub_DeviceKey().value();
  } else {
    std::ostringstream ss;
    ss << "Azure IoT Hub DeviceKey not set";
    os << ss.str() << std::endl;
    M5_LOGE("%s", ss.str());
    return false;
  }

  //
  if (_telemetry.begin(iothub_fqdn, iothub_device_id, iothub_device_key) ==
      false) {
    std::ostringstream ss;
    ss << "MQTT subscribe failed.";
    os << ss.str() << std::endl;
    M5_LOGE("%s", ss.str());
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
bool Application::start_sensor_BME280(std::ostream &os) {
  //
  os << "start BME280 sensor." << std::endl;
  M5_LOGI("start BME280 sensor.");
  //
  std::unique_ptr<Sensor::Device> pdev = std::make_unique<Sensor::Bme280Device>(
      SENSOR_DESCRIPTOR_BME280, BME280_I2C_ADDRESS, Wire);
  if (pdev) {
    auto timeover{steady_clock::now() + TIMEOUT};
    while (pdev->begin() == false && steady_clock::now() < timeover) {
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
    while (pdev->begin() == false && steady_clock::now() < timeover) {
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
    while (pdev->begin() == false && steady_clock::now() < timeover) {
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
    while (pdev->begin() == false && steady_clock::now() < timeover) {
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
    while (pdev->begin() == false && steady_clock::now() < timeover) {
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
