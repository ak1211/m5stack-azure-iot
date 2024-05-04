// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "Application.hpp"
#include "Database.hpp"
#include "Sensor.hpp"
#include <algorithm>
#include <chrono>
#include <lvgl.h>
#include <memory>
#include <tuple>
#include <vector>

#include <M5Unified.h>

//
//
//
namespace Widget {
using namespace std::chrono;
// init argument for "lv_tileview_add_tile()"
using InitArg = std::tuple<lv_obj_t *, uint8_t, uint8_t, lv_dir_t>;

class TileBase {
  // LVGL timer
  lv_timer_t *periodic_timer{nullptr};

protected:
  lv_obj_t *tileview_obj{nullptr};
  lv_obj_t *tile_obj{nullptr};

public:
  constexpr static auto PERIODIC_TIMER_INTERVAL = std::chrono::milliseconds{60};
  //
  TileBase(InitArg init) {
    if (tileview_obj = std::get<0>(init); tileview_obj) {
      tile_obj = std::apply(lv_tileview_add_tile, init);
      // set value changed callback
      lv_obj_add_event_cb(tileview_obj, event_value_changed_callback,
                          LV_EVENT_VALUE_CHANGED, this);
    }
  }
  //
  virtual ~TileBase() {
    delTimer();
    lv_obj_remove_event_cb(tileview_obj, event_value_changed_callback);
    if (tile_obj) {
      lv_obj_del(tile_obj);
    }
  };
  //
  bool isActiveTile() const noexcept {
    return tile_obj == lv_tileview_get_tile_act(tileview_obj);
  }
  //
  void setActiveTile() noexcept {
    lv_obj_set_tile(tileview_obj, tile_obj, LV_ANIM_ON);
  }
  //
  bool createTimer() {
    if (timer_callback) {
      delTimer();
      periodic_timer = lv_timer_create(
          timer_callback,
          duration_cast<milliseconds>(PERIODIC_TIMER_INTERVAL).count(), this);
      return true;
    } else {
      return false;
    }
  }
  //
  void delTimer() {
    if (periodic_timer) {
      lv_timer_del(periodic_timer);
    }
    periodic_timer = nullptr;
  }
  //
  static void timer_callback(lv_timer_t *timer) noexcept {
    if (timer) {
      if (auto *it = static_cast<TileBase *>(timer->user_data); it) {
        it->update();
      }
    }
  }
  //
  static void event_value_changed_callback(lv_event_t *event) noexcept {
    if (event) {
      if (auto it = static_cast<TileBase *>(event->user_data); it) {
        if (lv_tileview_get_tile_act(it->tileview_obj) == it->tile_obj) {
          M5_LOGV("event_value_changed_callback: active");
          // active
          it->createTimer();
        } else {
          M5_LOGV("event_value_changed_callback: inactive");
          // inactive
          it->delTimer();
        }
      }
    }
  }
  //
  virtual void update() noexcept = 0;
};

//
//
//
class BootMessage final : public TileBase {
  lv_obj_t *message_label_obj{nullptr};
  size_t count{0};

public:
  BootMessage(BootMessage &&) = delete;
  BootMessage &operator=(const BootMessage &) = delete;
  BootMessage(InitArg init) noexcept;
  //
  virtual void update() noexcept override;
  //
  void render() noexcept;
};

//
//
//
class Summary final : public TileBase {
  lv_obj_t *table_obj{nullptr};
  //
  using Measurements = std::tuple<std::optional<Sensor::MeasurementBme280>,
                                  std::optional<Sensor::MeasurementSgp30>,
                                  std::optional<Sensor::MeasurementScd30>,
                                  std::optional<Sensor::MeasurementScd41>,
                                  std::optional<Sensor::MeasurementM5Env3>>;
  Measurements latest{};

public:
  Summary(Summary &&) = delete;
  Summary &operator=(const Summary &) = delete;
  Summary(InitArg init) noexcept;
  //
  virtual void update() noexcept override;
  //
  void render() noexcept;
};

//
//
//
class Clock final : public TileBase {
  lv_obj_t *meter_obj{nullptr};
  lv_meter_scale_t *sec_scale{nullptr};
  lv_meter_scale_t *min_scale{nullptr};
  lv_meter_scale_t *hour_scale{nullptr};
  lv_meter_indicator_t *sec_indic{nullptr};
  lv_meter_indicator_t *min_indic{nullptr};
  lv_meter_indicator_t *hour_indic{nullptr};

public:
  Clock(Clock &&) = delete;
  Clock &operator=(const Clock &) = delete;
  Clock(InitArg init) noexcept;
  //
  virtual void update() noexcept override;
  //
  void render(const std::tm &tm) noexcept;
};

//
//
//
class SystemHealthy final : public TileBase {
  lv_obj_t *cont_col_obj{nullptr};
  //
  lv_style_t label_style;
  //
  lv_obj_t *time_label_obj{nullptr};
  lv_obj_t *status_label_obj{nullptr};
  lv_obj_t *battery_label_obj{nullptr};
  lv_obj_t *battery_charging_label_obj{nullptr};
  lv_obj_t *available_heap_label_obj{nullptr};
  lv_obj_t *available_internal_heap_label_obj{nullptr};
  lv_obj_t *minimum_free_heap_label_obj{nullptr};

public:
  SystemHealthy(SystemHealthy &&) = delete;
  SystemHealthy &operator=(const SystemHealthy &) = delete;
  SystemHealthy(InitArg init) noexcept;
  //
  virtual void update() noexcept override { render(); }
  void render() noexcept;
};

//
//
//
template <typename T> struct ShowMeasured {
  lv_color32_t color;
  std::string name;
  std::string_view unit;
  T meas;
};
template <typename T>
std::ostream &operator<<(std::ostream &os, const ShowMeasured<T> &rhs);

//
//
//
template <typename T> class BasicChart : public TileBase {
public:
  using DataType = std::tuple<SensorId, system_clock::time_point, T>;

protected:
  lv_obj_t *title_obj{nullptr};
  lv_style_t label_style{};
  lv_obj_t *label_obj{nullptr};
  lv_obj_t *chart_obj{nullptr};
  //
  const std::vector<lv_color_t> LINE_COLOR_TABLE;
  //
  std::vector<std::pair<SensorId, lv_chart_series_t *>> chart_series_vect{};
  //
  system_clock::time_point begin_x_tick{};
  //
  std::string subheading{};

public:
  BasicChart(BasicChart &&) = delete;
  BasicChart &operator=(const BasicChart &) = delete;
  BasicChart(InitArg init, const std::string &inSubheading) noexcept;
  //
  virtual size_t read_measurements_from_database(
      Database::OrderBy order, system_clock::time_point at_begin,
      Database::ReadCallback<DataType> callback) = 0;
  // データを座標に変換する関数
  virtual lv_point_t coordinateXY(system_clock::time_point begin_x,
                                  const DataType &in) noexcept = 0;
  //
  void render() noexcept;
};

//
// 気温
//
class TemperatureChart final : public BasicChart<double> {
private:
  using Measurements = std::tuple<std::optional<Sensor::MeasurementBme280>,
                                  std::optional<Sensor::MeasurementScd30>,
                                  std::optional<Sensor::MeasurementScd41>,
                                  std::optional<Sensor::MeasurementM5Env3>>;
  Measurements latest{};

public:
  TemperatureChart(TemperatureChart &&) = delete;
  TemperatureChart &operator=(const TemperatureChart &) = delete;
  TemperatureChart(InitArg init) noexcept;
  //
  virtual void update() noexcept override;
  // データを座標に変換する関数
  virtual lv_point_t
  coordinateXY(system_clock::time_point begin_x,
               const Database::TimePointAndDouble &in) noexcept override;
  //
  virtual size_t read_measurements_from_database(
      Database::OrderBy order, system_clock::time_point at_begin,
      Database::ReadCallback<Database::TimePointAndDouble> callback) override {
    return Application::measurements_database.read_temperatures(order, at_begin,
                                                                callback);
  }
  //
  static void event_draw_part_begin_callback(lv_event_t *event) noexcept;
};

//
// 相対湿度
//
class RelativeHumidityChart final : public BasicChart<double> {
private:
  using Measurements = std::tuple<std::optional<Sensor::MeasurementBme280>,
                                  std::optional<Sensor::MeasurementScd30>,
                                  std::optional<Sensor::MeasurementScd41>,
                                  std::optional<Sensor::MeasurementM5Env3>>;
  Measurements latest{};

public:
  RelativeHumidityChart(RelativeHumidityChart &&) = delete;
  RelativeHumidityChart &operator=(const RelativeHumidityChart &) = delete;
  RelativeHumidityChart(InitArg init) noexcept;
  //
  virtual void update() noexcept override;
  // データを座標に変換する関数
  virtual lv_point_t
  coordinateXY(system_clock::time_point begin_x,
               const Database::TimePointAndDouble &in) noexcept override;
  //
  virtual size_t read_measurements_from_database(
      Database::OrderBy order, system_clock::time_point at_begin,
      Database::ReadCallback<Database::TimePointAndDouble> callback) override {
    return Application::measurements_database.read_relative_humidities(
        order, at_begin, callback);
  }
  //
  static void event_draw_part_begin_callback(lv_event_t *event) noexcept;
};

//
// 気圧
//
class PressureChart final : public BasicChart<double> {
private:
  using Measurements = std::tuple<std::optional<Sensor::MeasurementBme280>,
                                  std::optional<Sensor::MeasurementM5Env3>>;
  Measurements latest{};

public:
  PressureChart(PressureChart &&) = delete;
  PressureChart &operator=(const PressureChart &) = delete;
  PressureChart(InitArg init) noexcept;
  //
  virtual void update() noexcept override;
  //
  constexpr static DeciPa BIAS = round<DeciPa>(HectoPa{1000});
  // データを座標に変換する関数
  virtual lv_point_t
  coordinateXY(system_clock::time_point begin_x,
               const Database::TimePointAndDouble &in) noexcept override;
  //
  virtual size_t read_measurements_from_database(
      Database::OrderBy order, system_clock::time_point at_begin,
      Database::ReadCallback<Database::TimePointAndDouble> callback) override {
    return Application::measurements_database.read_pressures(order, at_begin,
                                                             callback);
  }
  static void event_draw_part_begin_callback(lv_event_t *event) noexcept;
};

//
// Co2 / ECo2
//
class CarbonDeoxidesChart final : public BasicChart<uint16_t> {
private:
  using Measurements = std::tuple<std::optional<Sensor::MeasurementSgp30>,
                                  std::optional<Sensor::MeasurementScd30>,
                                  std::optional<Sensor::MeasurementScd41>>;
  Measurements latest{};

public:
  CarbonDeoxidesChart(CarbonDeoxidesChart &&) = delete;
  CarbonDeoxidesChart &operator=(const CarbonDeoxidesChart &) = delete;
  CarbonDeoxidesChart(InitArg init) noexcept;
  //
  virtual void update() noexcept override;
  // データを座標に変換する関数
  virtual lv_point_t
  coordinateXY(system_clock::time_point begin_x,
               const Database::TimePointAndUInt16 &in) noexcept override;
  //
  virtual size_t read_measurements_from_database(
      Database::OrderBy order, system_clock::time_point at_begin,
      Database::ReadCallback<Database::TimePointAndUInt16> callback) override {
    return Application::measurements_database.read_carbon_deoxides(
        order, at_begin, callback);
  }
};

//
// Total VOC
//
class TotalVocChart final : public BasicChart<uint16_t> {
private:
  std::optional<Sensor::MeasurementSgp30> latest{};

public:
  TotalVocChart(TotalVocChart &&) = delete;
  TotalVocChart &operator=(const TotalVocChart &) = delete;
  TotalVocChart(InitArg init) noexcept;
  //
  virtual void update() noexcept override;
  // データを座標に変換する関数
  virtual lv_point_t
  coordinateXY(system_clock::time_point begin_x,
               const Database::TimePointAndUInt16 &in) noexcept override;
  //
  virtual size_t read_measurements_from_database(
      Database::OrderBy order, system_clock::time_point at_begin,
      Database::ReadCallback<Database::TimePointAndUInt16> callback) override {
    return Application::measurements_database.read_total_vocs(order, at_begin,
                                                              callback);
  }
  //
  static void event_draw_part_begin_callback(lv_event_t *event) noexcept;
};
} // namespace Widget

//
//
//
class Gui {
  static Gui *_instance;

public:
  constexpr static auto PERIODIC_TIMER_INTERVAL = std::chrono::milliseconds{60};
  constexpr static uint16_t CHART_X_POINT_COUNT = 60;
  Gui(M5GFX &gfx) : gfx{gfx} {
    if (_instance) {
      delete _instance;
    }
    _instance = this;
  }
  //
  static Gui *getInstance() noexcept { return _instance; }
  //
  bool begin() noexcept;
  //
  void startUi() noexcept;
  //
  void home() noexcept;
  //
  void movePrev() noexcept;
  //
  void moveNext() noexcept;
  //
  void vibrate() noexcept;
  //
  lv_res_t send_event_to_tileview(lv_event_code_t event_code, void *param) {
    return lv_event_send(_instance->tileview_obj, event_code, param);
  }

private:
  M5GFX &gfx;
  // LVGL tileview object
  lv_obj_t *tileview_obj{nullptr};
  // tile widget
  std::vector<std::unique_ptr<Widget::TileBase>> tile_vector{};
  //
  static bool check_if_active_tile(
      const std::unique_ptr<Widget::TileBase> &tile_to_test) noexcept {
    if (auto tile = tile_to_test.get(); tile) {
      return tile->isActiveTile();
    } else {
      return false;
    }
  }
  //
  template <typename T>
  inline void add_tile(const Widget::InitArg &arg) noexcept {
    tile_vector.emplace_back(std::make_unique<T>(arg));
  }

private:
  // LVGL use area
  struct {
    // LVGL draw buffer
    std::unique_ptr<lv_color_t[]> draw_buf_1;
    std::unique_ptr<lv_color_t[]> draw_buf_2;
    lv_disp_draw_buf_t draw_buf_dsc;
    lv_disp_drv_t disp_drv;
    lv_indev_drv_t indev_drv;
  } lvgl_use;
  //
  static void lvgl_use_display_flush_callback(lv_disp_drv_t *disp_drv,
                                              const lv_area_t *area,
                                              lv_color_t *color_p) noexcept;
  //
  static void lvgl_use_touchpad_read_callback(lv_indev_drv_t *indev_drv,
                                              lv_indev_data_t *data) noexcept;
};
