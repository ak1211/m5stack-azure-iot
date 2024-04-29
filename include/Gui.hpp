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

//
//
//
struct TileBase {
  virtual void setActiveTile(lv_obj_t *tileview_obj) noexcept = 0;
  virtual bool isActiveTile(lv_obj_t *tileview_obj) const noexcept = 0;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept = 0;
  virtual void timerHook() noexcept = 0;
};

//
//
//
class BootMessage final : public TileBase {
  lv_obj_t *tile_obj{nullptr};
  lv_obj_t *message_label_obj{nullptr};
  size_t count{0};

public:
  BootMessage(InitArg init) noexcept;
  BootMessage(BootMessage &&) = delete;
  BootMessage &operator=(const BootMessage &) = delete;
  virtual ~BootMessage() noexcept;
  virtual void setActiveTile(lv_obj_t *tileview_obj) noexcept override {
    lv_obj_set_tile(tileview_obj, tile_obj, LV_ANIM_ON);
  }
  virtual bool isActiveTile(lv_obj_t *tileview_obj) const noexcept override {
    return tile_obj == lv_tileview_get_tile_act(tileview_obj);
  }
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  void render() noexcept;
};

//
//
//
class Summary final : public TileBase {
  lv_obj_t *tile_obj{nullptr};
  lv_obj_t *table_obj{nullptr};
  //
  using Measurements = std::tuple<std::optional<Sensor::MeasurementBme280>,
                                  std::optional<Sensor::MeasurementSgp30>,
                                  std::optional<Sensor::MeasurementScd30>,
                                  std::optional<Sensor::MeasurementScd41>,
                                  std::optional<Sensor::MeasurementM5Env3>>;
  Measurements latest{};

public:
  Summary(InitArg init) noexcept;
  Summary(Summary &&) = delete;
  Summary &operator=(const Summary &) = delete;
  virtual ~Summary() noexcept;
  virtual void setActiveTile(lv_obj_t *tileview_obj) noexcept override {
    lv_obj_set_tile(tileview_obj, tile_obj, LV_ANIM_ON);
  }
  virtual bool isActiveTile(lv_obj_t *tileview_obj) const noexcept override {
    return tile_obj == lv_tileview_get_tile_act(tileview_obj);
  }
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  void render() noexcept;
};

//
//
//
class Clock final : public TileBase {
  lv_obj_t *tile_obj{nullptr};
  lv_obj_t *meter_obj{nullptr};
  lv_meter_scale_t *sec_scale{nullptr};
  lv_meter_scale_t *min_scale{nullptr};
  lv_meter_scale_t *hour_scale{nullptr};
  lv_meter_indicator_t *sec_indic{nullptr};
  lv_meter_indicator_t *min_indic{nullptr};
  lv_meter_indicator_t *hour_indic{nullptr};

public:
  Clock(InitArg init) noexcept;
  Clock(Clock &&) = delete;
  Clock &operator=(const Clock &) = delete;
  virtual ~Clock() noexcept;
  virtual void setActiveTile(lv_obj_t *tileview_obj) noexcept override {
    lv_obj_set_tile(tileview_obj, tile_obj, LV_ANIM_ON);
  }
  virtual bool isActiveTile(lv_obj_t *tileview_obj) const noexcept override {
    return tile_obj == lv_tileview_get_tile_act(tileview_obj);
  }
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  void render(const std::tm &tm) noexcept;
};

//
//
//
class SystemHealthy final : public TileBase {
  lv_obj_t *tile_obj{nullptr};
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
  SystemHealthy(InitArg init) noexcept;
  SystemHealthy(SystemHealthy &&) = delete;
  SystemHealthy &operator=(const SystemHealthy &) = delete;
  virtual ~SystemHealthy() noexcept;
  virtual void setActiveTile(lv_obj_t *tileview_obj) noexcept override {
    lv_obj_set_tile(tileview_obj, tile_obj, LV_ANIM_ON);
  }
  virtual bool isActiveTile(lv_obj_t *tileview_obj) const noexcept override {
    return tile_obj == lv_tileview_get_tile_act(tileview_obj);
  }
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  void render() noexcept;
};

//
//
//
template <typename T> class BasicChart : public TileBase {
public:
  using DataType = std::tuple<SensorId, system_clock::time_point, T>;

protected:
  lv_obj_t *tile_obj{nullptr};
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
  BasicChart(InitArg init, const std::string &inSubheading) noexcept;
  BasicChart(BasicChart &&) = delete;
  BasicChart &operator=(const BasicChart &) = delete;
  virtual ~BasicChart() noexcept;
  virtual void setActiveTile(lv_obj_t *tileview_obj) noexcept override {
    lv_obj_set_tile(tileview_obj, tile_obj, LV_ANIM_ON);
  }
  virtual bool isActiveTile(lv_obj_t *tileview_obj) const noexcept override {
    return tile_obj == lv_tileview_get_tile_act(tileview_obj);
  }
  //
  virtual size_t read_measurements_from_database(
      Database::OrderBy order, system_clock::time_point at_begin,
      Database::ReadCallback<DataType> callback) = 0;
  // データを座標に変換する関数
  virtual lv_point_t coordinateXY(system_clock::time_point begin_x,
                                  const DataType &in) noexcept = 0;
  //
  virtual void render() noexcept;
};

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
  TemperatureChart(InitArg init) noexcept;
  TemperatureChart(TemperatureChart &&) = delete;
  TemperatureChart &operator=(const TemperatureChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
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
  static void drawEventHook(lv_event_t *event) noexcept;
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
  RelativeHumidityChart(InitArg init) noexcept;
  RelativeHumidityChart(RelativeHumidityChart &&) = delete;
  RelativeHumidityChart &operator=(const RelativeHumidityChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
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
  static void drawEventHook(lv_event_t *event) noexcept;
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
  PressureChart(InitArg init) noexcept;
  PressureChart(PressureChart &&) = delete;
  PressureChart &operator=(const PressureChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
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
  static void drawEventHook(lv_event_t *event) noexcept;
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
  CarbonDeoxidesChart(InitArg init) noexcept;
  CarbonDeoxidesChart(CarbonDeoxidesChart &&) = delete;
  CarbonDeoxidesChart &operator=(const CarbonDeoxidesChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
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
  TotalVocChart(InitArg init) noexcept;
  TotalVocChart(TotalVocChart &&) = delete;
  TotalVocChart &operator=(const TotalVocChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
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
  static void drawEventHook(lv_event_t *event) noexcept;
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

public:
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
  // LVGL use area
  struct {
    // LVGL draw buffer
    std::unique_ptr<lv_color_t[]> draw_buf_1;
    std::unique_ptr<lv_color_t[]> draw_buf_2;
    lv_disp_draw_buf_t draw_buf_dsc;
    lv_disp_drv_t disp_drv;
    lv_indev_drv_t indev_drv;
  } lvgl_use;
  // LVGL timer
  lv_timer_t *periodic_timer{nullptr};
  // LVGL tileview object
  lv_obj_t *tileview_obj{nullptr};
  // tile widget
  using TileVector = std::vector<std::unique_ptr<Widget::TileBase>>;
  TileVector tiles{};
  //
  static void display_flush_callback(lv_disp_drv_t *disp_drv,
                                     const lv_area_t *area,
                                     lv_color_t *color_p) noexcept;
  //
  static void touchpad_read_callback(lv_indev_drv_t *indev_drv,
                                     lv_indev_data_t *data) noexcept;
  //
  static bool check_if_active_tile(
      const std::unique_ptr<Widget::TileBase> &tile_to_test) noexcept {
    if (auto p = tile_to_test.get(); p) {
      return p->isActiveTile(_instance->tileview_obj);
    } else {
      return false;
    }
  }
  //
  template <typename T>
  inline void add_tile(const Widget::InitArg &arg) noexcept {
    tiles.emplace_back(std::make_unique<T>(arg));
  }
};
