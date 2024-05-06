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
#include <unordered_map>
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
  bool isActiveTile() const {
    return tile_obj == lv_tileview_get_tile_act(tileview_obj);
  }
  //
  void setActiveTile() { lv_obj_set_tile(tileview_obj, tile_obj, LV_ANIM_ON); }
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
  static void timer_callback(lv_timer_t *timer) {
    if (timer) {
      if (auto *it = static_cast<TileBase *>(timer->user_data); it) {
        it->update();
      }
    }
  }
  //
  static void event_value_changed_callback(lv_event_t *event) {
    if (event) {
      if (auto it = static_cast<TileBase *>(event->user_data); it) {
        if (lv_tileview_get_tile_act(it->tileview_obj) == it->tile_obj) {
          M5_LOGV("event_value_changed_callback: active");
          // active
          it->onActivate();
          it->createTimer();
        } else {
          M5_LOGV("event_value_changed_callback: inactive");
          // inactive
          it->delTimer();
          it->onDeactivate();
        }
      }
    }
  }
  //
  virtual void onActivate() = 0;
  //
  virtual void onDeactivate() = 0;
  //
  virtual void update() = 0;
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
  BootMessage(InitArg init);
  //
  virtual void onActivate() override { render(); }
  //
  virtual void onDeactivate() override {}
  //
  virtual void update() override;
  //
  void render();
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
  Summary(InitArg init);
  //
  virtual void onActivate() override { render(); }
  //
  virtual void onDeactivate() override {}
  //
  virtual void update() override;
  //
  void render();
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
  Clock(InitArg init);
  //
  virtual void onActivate() override {}
  //
  virtual void onDeactivate() override {}
  //
  virtual void update() override;
  //
  void render(const std::tm &tm);
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
  SystemHealthy(InitArg init);
  //
  virtual void onActivate() override { render(); }
  //
  virtual void onDeactivate() override {}
  //
  virtual void update() override { render(); }
  //
  void render();
};

//
//
//
extern const std::unordered_map<SensorId, lv_color_t> LINE_COLOR_MAP;

//
//
//
class ChartSeriesWrapper {
  SensorId _sensor_id;
  lv_obj_t *_chart_obj;
  lv_chart_series_t *_chart_series{nullptr};

public:
  //
  ChartSeriesWrapper(SensorId sensor_id, lv_obj_t *chart_obj)
      : _sensor_id{sensor_id}, _chart_obj{chart_obj}, _chart_series{nullptr} {
    if (_chart_obj == nullptr) {
      M5_LOGD("chart had null");
      return;
    }
  }
  //
  ~ChartSeriesWrapper() {
    if (_chart_series) {
      lv_chart_remove_series(_chart_obj, _chart_series);
    }
  }
  //
  bool operator==(const SensorId &other) const {
    return this->_sensor_id == other;
  }
  //
  bool operator!=(const SensorId &other) const { return !(*this == other); }
  //
  std::pair<lv_coord_t, lv_coord_t> getMinMaxOfYPoints() {
    auto y_min = std::numeric_limits<lv_coord_t>::max();
    auto y_max = std::numeric_limits<lv_coord_t>::min();
    if (_chart_series) {
      for (auto i = 0; i < lv_chart_get_point_count(_chart_obj); ++i) {
        if (_chart_series->y_points[i] != LV_CHART_POINT_NONE) {
          y_min = std::min(y_min, _chart_series->y_points[i]);
          y_max = std::max(y_max, _chart_series->y_points[i]);
        }
      }
    } else {
      M5_LOGD("chart_series had null");
    }
    return {y_min, y_max};
  }
  //
  bool available() const { return _chart_series != nullptr; }
  //
  std::optional<uint16_t> getXStartPointForValidValues() const {
    if (_chart_obj == nullptr) {
      return std::nullopt;
    }
    if (_chart_series == nullptr) {
      return std::nullopt;
    }
    auto index = 0;
    while (index < lv_chart_get_point_count(_chart_obj) &&
           _chart_series->y_points[index] == LV_CHART_POINT_NONE) {
      ++index;
    }
    return index;
  }
  //
  void chart_add_series() {
    if (_chart_obj == nullptr) {
      M5_LOGE("chart had null");
      return;
    }
    auto itr = LINE_COLOR_MAP.find(_sensor_id);
    lv_color_t color =
        itr != LINE_COLOR_MAP.end() ? itr->second : lv_color_black();
    _chart_series =
        lv_chart_add_series(_chart_obj, color, LV_CHART_AXIS_SECONDARY_Y);
  }
  //
  void chart_remove_series() {
    if (_chart_obj == nullptr) {
      M5_LOGE("chart had null");
      return;
    }
    if (_chart_series) {
      lv_chart_remove_series(_chart_obj, _chart_series);
    }
    _chart_series = nullptr;
  }
  //
  void chart_set_point_count(uint16_t count) {
    lv_chart_set_point_count(_chart_obj, count);
  }
  //
  bool chart_set_all_value(lv_coord_t value) {
    if (_chart_series) {
      lv_chart_set_all_value(_chart_obj, _chart_series, value);
      return true;
    } else {
      M5_LOGE("chart_series had null");
      return false;
    }
  }
  //
  bool chart_set_x_start_point(uint16_t id) {
    if (_chart_series) {
      lv_chart_set_x_start_point(_chart_obj, _chart_series, id);
      return true;
    } else {
      M5_LOGE("chart_series had null");
      return false;
    }
  }
  //
  bool chart_set_value_by_id(uint16_t id, lv_coord_t value) {
    if (_chart_series) {
      lv_chart_set_value_by_id(_chart_obj, _chart_series, id, value);
      return true;
    } else {
      M5_LOGE("chart_series had null");
      return false;
    }
  }
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
template <typename T> class BasicChart {
public:
  using DataType = std::tuple<SensorId, system_clock::time_point, T>;
  // データを座標に変換する関数
  using CoordinatePointFn =
      std::function<lv_point_t(system_clock::time_point, const DataType &)>;
  // データーベースからデーターを得る関数
  using ReadDataFn = std::function<size_t(
      Database::OrderBy order, system_clock::time_point at_begin,
      Database::ReadCallback<DataType> callback)>;
  //
  BasicChart(CoordinatePointFn coordinateXY,
             ReadDataFn read_measurements_from_database)
      : _coordinateXY{coordinateXY},
        _read_measurements_from_database{read_measurements_from_database} {}
  //
  void createWidgets(lv_obj_t *parent_obj, std::string_view inSubheading);
  //
  lv_obj_t *getChartObj() const { return chart_obj; }
  //
  lv_obj_t *getLabelObj() const { return label_obj; }
  //
  void render();

private:
  lv_obj_t *title_obj{nullptr};
  lv_style_t label_style{};
  lv_obj_t *label_obj{nullptr};
  lv_obj_t *chart_obj{nullptr};
  //
  std::vector<ChartSeriesWrapper> chart_series_vect{};
  //
  system_clock::time_point begin_x_tick{};
  //
  std::string subheading{};
  //
  const ReadDataFn _read_measurements_from_database;
  const CoordinatePointFn _coordinateXY;
};

//
// 気温
//
class TemperatureChart final : public TileBase {
private:
  using Measurements = std::tuple<std::optional<Sensor::MeasurementBme280>,
                                  std::optional<Sensor::MeasurementScd30>,
                                  std::optional<Sensor::MeasurementScd41>,
                                  std::optional<Sensor::MeasurementM5Env3>>;
  Measurements latest{};
  BasicChart<double> basic_chart;

public:
  TemperatureChart(TemperatureChart &&) = delete;
  TemperatureChart &operator=(const TemperatureChart &) = delete;
  TemperatureChart(InitArg init);
  //
  virtual void onActivate() override { basic_chart.render(); }
  //
  virtual void onDeactivate() override {}
  //
  virtual void update() override;
  // データーベースからデーターを得る
  static size_t read_measurements_from_database(
      Database::OrderBy order, system_clock::time_point at_begin,
      Database::ReadCallback<Database::TimePointAndDouble> callback) {
    return Application::measurements_database.read_temperatures(order, at_begin,
                                                                callback);
  }
  // データを座標に変換する関数
  static lv_point_t coordinateXY(system_clock::time_point begin_x,
                                 const Database::TimePointAndDouble &in);
  //
  static void event_draw_part_begin_callback(lv_event_t *event);
};

//
// 相対湿度
//
class RelativeHumidityChart final : public TileBase {
private:
  using Measurements = std::tuple<std::optional<Sensor::MeasurementBme280>,
                                  std::optional<Sensor::MeasurementScd30>,
                                  std::optional<Sensor::MeasurementScd41>,
                                  std::optional<Sensor::MeasurementM5Env3>>;
  Measurements latest{};
  BasicChart<double> basic_chart;

public:
  RelativeHumidityChart(RelativeHumidityChart &&) = delete;
  RelativeHumidityChart &operator=(const RelativeHumidityChart &) = delete;
  RelativeHumidityChart(InitArg init);
  //
  virtual void onActivate() override { basic_chart.render(); }
  //
  virtual void onDeactivate() override {}
  //
  virtual void update() override;
  // データーベースからデーターを得る
  static size_t read_measurements_from_database(
      Database::OrderBy order, system_clock::time_point at_begin,
      Database::ReadCallback<Database::TimePointAndDouble> callback) {
    return Application::measurements_database.read_relative_humidities(
        order, at_begin, callback);
  }
  // データを座標に変換する関数
  static lv_point_t coordinateXY(system_clock::time_point begin_x,
                                 const Database::TimePointAndDouble &in);
  //
  static void event_draw_part_begin_callback(lv_event_t *event);
};

//
// 気圧
//
class PressureChart final : public TileBase {
private:
  using Measurements = std::tuple<std::optional<Sensor::MeasurementBme280>,
                                  std::optional<Sensor::MeasurementM5Env3>>;
  Measurements latest{};
  BasicChart<double> basic_chart;

public:
  PressureChart(PressureChart &&) = delete;
  PressureChart &operator=(const PressureChart &) = delete;
  PressureChart(InitArg init);
  //
  virtual void onActivate() override { basic_chart.render(); }
  //
  virtual void onDeactivate() override {}
  //
  virtual void update() override;
  // データーベースからデーターを得る
  static size_t read_measurements_from_database(
      Database::OrderBy order, system_clock::time_point at_begin,
      Database::ReadCallback<Database::TimePointAndDouble> callback) {
    return Application::measurements_database.read_pressures(order, at_begin,
                                                             callback);
  }
  // データを座標に変換する関数
  static lv_point_t coordinateXY(system_clock::time_point begin_x,
                                 const Database::TimePointAndDouble &in);
  //
  constexpr static DeciPa BIAS = round<DeciPa>(HectoPa{1000});
  //
  static void event_draw_part_begin_callback(lv_event_t *event);
};

//
// Co2 / ECo2
//
class CarbonDeoxidesChart final : public TileBase {
private:
  using Measurements = std::tuple<std::optional<Sensor::MeasurementSgp30>,
                                  std::optional<Sensor::MeasurementScd30>,
                                  std::optional<Sensor::MeasurementScd41>>;
  Measurements latest{};
  BasicChart<uint16_t> basic_chart;

public:
  CarbonDeoxidesChart(CarbonDeoxidesChart &&) = delete;
  CarbonDeoxidesChart &operator=(const CarbonDeoxidesChart &) = delete;
  CarbonDeoxidesChart(InitArg init);
  //
  virtual void onActivate() override { basic_chart.render(); }
  //
  virtual void onDeactivate() override {}
  //
  virtual void update() override;
  // データーベースからデーターを得る
  static size_t read_measurements_from_database(
      Database::OrderBy order, system_clock::time_point at_begin,
      Database::ReadCallback<Database::TimePointAndUInt16> callback) {
    return Application::measurements_database.read_carbon_deoxides(
        order, at_begin, callback);
  }
  // データを座標に変換する関数
  static lv_point_t coordinateXY(system_clock::time_point begin_x,
                                 const Database::TimePointAndUInt16 &in);
};

//
// Total VOC
//
class TotalVocChart final : public TileBase {
private:
  std::optional<Sensor::MeasurementSgp30> latest{};
  BasicChart<uint16_t> basic_chart;

public:
  TotalVocChart(TotalVocChart &&) = delete;
  TotalVocChart &operator=(const TotalVocChart &) = delete;
  TotalVocChart(InitArg init);
  //
  virtual void onActivate() override { basic_chart.render(); }
  //
  virtual void onDeactivate() override {}
  //
  virtual void update() override;
  // データーベースからデーターを得る
  static size_t read_measurements_from_database(
      Database::OrderBy order, system_clock::time_point at_begin,
      Database::ReadCallback<Database::TimePointAndUInt16> callback) {
    return Application::measurements_database.read_total_vocs(order, at_begin,
                                                              callback);
  }
  // データを座標に変換する関数
  static lv_point_t coordinateXY(system_clock::time_point begin_x,
                                 const Database::TimePointAndUInt16 &in);
  //
  static void event_draw_part_begin_callback(lv_event_t *event);
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
  static Gui *getInstance() { return _instance; }
  //
  bool begin();
  //
  void startUi();
  //
  void home();
  //
  void movePrev();
  //
  void moveNext();
  //
  void vibrate();
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
  static bool
  check_if_active_tile(const std::unique_ptr<Widget::TileBase> &tile_to_test) {
    if (auto tile = tile_to_test.get(); tile) {
      return tile->isActiveTile();
    } else {
      return false;
    }
  }
  //
  template <typename T> inline void add_tile(const Widget::InitArg &arg) {
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
                                              lv_color_t *color_p);
  //
  static void lvgl_use_touchpad_read_callback(lv_indev_drv_t *indev_drv,
                                              lv_indev_data_t *data);
};
