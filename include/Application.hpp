// Copyright (c) 2022 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "Database.hpp"
#include "Gui.hpp"
#include "MeasuringTask.hpp"
#include "RgbLed.hpp"
#include "Sensor.hpp"
#include "Telemetry.hpp"
#include <WiFi.h>
#include <chrono>
#include <esp_task.h>
#include <string>

//
//
//
class Application final {
public:
  //
  constexpr static auto LVGL_TASK_STACK_SIZE = size_t{8192};
  //
  constexpr static auto APPLICATION_TASK_STACK_SIZE = size_t{8192};
  //
  constexpr static auto TIMEOUT = std::chrono::seconds{3};
  // time zone = Asia_Tokyo(UTC+9)
  constexpr static auto TZ_TIME_ZONE = std::string_view{"JST-9"};
  //
  constexpr static std::string_view MEASUREMENTS_DATABASE_FILE_NAME{
      "file:/littlefs/measurements.db?pow=0&mode=memory"};
  //
  constexpr static auto BME280_I2C_ADDRESS = uint8_t{0x76};
  constexpr static auto SENSOR_DESCRIPTOR_BME280 =
      SensorDescriptor({'B', 'M', 'E', '2', '8', '0', '\0', '\0'});
  constexpr static auto SENSOR_DESCRIPTOR_SGP30 =
      SensorDescriptor({'S', 'G', 'P', '3', '0', '\0', '\0', '\0'});
  constexpr static auto SENSOR_DESCRIPTOR_SCD30 =
      SensorDescriptor({'S', 'C', 'D', '3', '0', '\0', '\0', '\0'});
  constexpr static auto SENSOR_DESCRIPTOR_SCD41 =
      SensorDescriptor({'S', 'C', 'D', '4', '1', '\0', '\0', '\0'});
  constexpr static auto SENSOR_DESCRIPTOR_M5ENV3 =
      SensorDescriptor({'M', '5', 'E', 'N', 'V', '3', '\0', '\0'});

public:
  Application(const Application &) = delete;
  Application &operator=(const Application &) = delete;
  Application(Application &&) = delete;
  Application &operator=(Application &&) = delete;
  Application(M5GFX &gfx) : _gui{gfx} {
    if (_instance) {
      esp_system_abort("multiple Application started.");
    }
    _instance = this;
  }
  //
  bool task_handler();
  // 起動
  bool startup();
  //
  std::string getStartupLog() const { return _startup_log; }
  //
  static RgbLed &getRgbLed() { return getInstance()->_rgb_led; }
  //
  static Database &getMeasurementsDatabase() {
    return getInstance()->_measurements_database;
  }
  //
  static Telemetry &getTelemetry() { return getInstance()->_telemetry; }
  //
  static Gui &getGui() { return getInstance()->_gui; }
  //
  static std::vector<std::unique_ptr<Sensor::Device>> &getSensors() {
    return getInstance()->_sensors;
  }
  //
  static TaskHandle_t getLvglTaskHandle() {
    return getInstance()->_rtos_lvgl_task_handle;
  }
  //
  static TaskHandle_t getApplicationTaskHandle() {
    return getInstance()->_rtos_application_task_handle;
  }
  //
  static Application *getInstance() {
    if (_instance == nullptr) {
      esp_system_abort("Application is not started.");
    }
    return _instance;
  }
  //
  static bool isTimeSynced() { return getInstance()->_time_is_synced; }
  //
  static std::chrono::seconds uptime() {
    auto elapsed = std::chrono::steady_clock::now() -
                   getInstance()->_application_start_time;
    return std::chrono::duration_cast<std::chrono::seconds>(elapsed);
  }
  // iso8601 format.
  static std::string isoformatUTC(std::time_t utctime);
  static std::string isoformatUTC(std::chrono::system_clock::time_point utctp) {
    return isoformatUTC(std::chrono::system_clock::to_time_t(utctp));
  }

private:
  static Application *_instance;
  //
  static const std::chrono::steady_clock::time_point _application_start_time;
  // 起動時のログ
  std::string _startup_log;
  // インターネット時間サーバーに同期しているか
  bool _time_is_synced{false};
  // LED
  RgbLed _rgb_led;
  // データーベース
  Database _measurements_database;
  // テレメトリ
  Telemetry _telemetry;
  // GUI
  Gui _gui;
  // センサー
  std::vector<std::unique_ptr<Sensor::Device>> _sensors;
  //
  MeasuringTask _measuring_task;
  //
  TaskHandle_t _rtos_lvgl_task_handle{};
  //
  TaskHandle_t _rtos_application_task_handle{};
  //
  void idle_task_handler();
  //
  bool start_wifi(std::ostream &os);
  // インターネット時間サーバと同期する
  bool synchronize_ntp(std::ostream &os);
  //
  bool start_telemetry(std::ostream &os);
  //
  bool start_database(std::ostream &os);
  //
  bool start_sensor_BME280(std::ostream &os);
  //
  bool start_sensor_SGP30(std::ostream &os);
  //
  bool start_sensor_SCD30(std::ostream &os);
  //
  bool start_sensor_SCD41(std::ostream &os);
  //
  bool start_sensor_M5ENV3(std::ostream &os);
  //
  static void time_sync_notification_callback(struct timeval *time_val);
  //
  static void wifi_event_callback(WiFiEvent_t event);
};
