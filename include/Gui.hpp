// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
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
  std::string latest{};

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
  lv_obj_t *lvgl_task_stack_mark_label_obj{nullptr};
  lv_obj_t *app_task_stack_mark_label_obj{nullptr};

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
class ChartLineColor {
public:
  const static std::unordered_map<SensorId, lv_color_t> LINE_COLOR_MAP;
  //
  static std::optional<lv_color_t> getAssignedColor(SensorId sensor_id);
  //
  static std::optional<lv_color32_t> getAssignedColor32(SensorId sensor_id);
};

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
  virtual ~ChartSeriesWrapper() { chart_remove_series(); }
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
    auto color = ChartLineColor::getAssignedColor(_sensor_id);
    _chart_series = lv_chart_add_series(
        _chart_obj, color ? color.value() : lv_color_black(),
        LV_CHART_AXIS_PRIMARY_Y);
  }
  //
  void chart_remove_series() {
    if (_chart_obj == nullptr) {
      M5_LOGE("chart had null");
      return;
    }
    if (_chart_series) {
      //
      // lv_chart_remove_seriesを呼ぶとクラッシュするので、とりあえず回避
      //
      // lv_chart_remove_series(_chart_obj, _chart_series);
      M5_LOGD("Avoid the \"lv_chart_remove_series\" function call to crash.");
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
template <typename T> class BasicChart {
public:
  constexpr static auto X_AXIS_TICK_COUNT{4};
  constexpr static auto Y_AXIS_TICK_COUNT{5};
  //
  using DataType = std::tuple<SensorId, system_clock::time_point, T>;
  // データーベースからデーターを得る
  virtual size_t read_measurements_from_database(
      Database::OrderBy order, system_clock::time_point at_begin,
      Database::ReadCallback<DataType> callback) = 0;
  // データを座標に変換する関数
  virtual lv_point_t coordinateXY(system_clock::time_point tp_zero,
                                  const DataType &in) = 0;
  // 軸目盛関数
  virtual void chart_draw_part_tick_label(lv_obj_draw_part_dsc_t *dsc) = 0;
  //
  BasicChart(lv_obj_t *parent_obj, std::string_view inSubheading);
  //
  virtual ~BasicChart();
  //
  lv_obj_t *getChartObj() const { return chart_obj; }
  //
  lv_obj_t *getLabelObj() const { return label_obj; }
  //
  system_clock::time_point getBeginX() const { return begin_x_tp; }
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
  system_clock::time_point begin_x_tp{};
  //
  std::string subheading{};
};

//
// 気温
//
class TemperatureChart final : public TileBase {
  //
  class C : public BasicChart<double> {
  public:
    //
    C(lv_obj_t *parent_obj, std::string_view inSubheading)
        : BasicChart<double>(parent_obj, inSubheading) {}
    // データーベースからデーターを得る
    virtual size_t read_measurements_from_database(
        Database::OrderBy order, system_clock::time_point at_begin,
        Database::ReadCallback<Database::TimePointAndDouble> callback) override;
    // データを座標に変換する関数
    virtual lv_point_t
    coordinateXY(system_clock::time_point tp_zero,
                 const Database::TimePointAndDouble &in) override;
    //
    virtual void
    chart_draw_part_tick_label(lv_obj_draw_part_dsc_t *dsc) override;
  };

private:
  using Measurements = std::tuple<std::optional<Sensor::MeasurementBme280>,
                                  std::optional<Sensor::MeasurementScd30>,
                                  std::optional<Sensor::MeasurementScd41>,
                                  std::optional<Sensor::MeasurementM5Env3>>;
  Measurements latest{};
  std::unique_ptr<C> basic_chart;

public:
  TemperatureChart(TemperatureChart &&) = delete;
  TemperatureChart &operator=(const TemperatureChart &) = delete;
  TemperatureChart(InitArg init);
  //
  virtual void onActivate() override {
    if (tile_obj == nullptr) {
      M5_LOGE("tile had null");
      return;
    }
    if (!basic_chart) {
      // create
      basic_chart = std::make_unique<C>(tile_obj, "Temperature");
    }
    render();
  }
  //
  virtual void onDeactivate() override {
    //    if (basic_chart) {
    //      basic_chart.reset();
    //    }
  }
  //
  virtual void update() override;
  //
  void render();
};

//
// 相対湿度
//
class RelativeHumidityChart final : public TileBase {
  //
  class C : public BasicChart<double> {
  public:
    //
    C(lv_obj_t *parent_obj, std::string_view inSubheading)
        : BasicChart<double>(parent_obj, inSubheading) {}
    // データーベースからデーターを得る
    virtual size_t read_measurements_from_database(
        Database::OrderBy order, system_clock::time_point at_begin,
        Database::ReadCallback<Database::TimePointAndDouble> callback) override;
    // データを座標に変換する関数
    virtual lv_point_t
    coordinateXY(system_clock::time_point tp_zero,
                 const Database::TimePointAndDouble &in) override;
    //
    virtual void
    chart_draw_part_tick_label(lv_obj_draw_part_dsc_t *dsc) override;
  };

private:
  using Measurements = std::tuple<std::optional<Sensor::MeasurementBme280>,
                                  std::optional<Sensor::MeasurementScd30>,
                                  std::optional<Sensor::MeasurementScd41>,
                                  std::optional<Sensor::MeasurementM5Env3>>;
  Measurements latest{};
  std::unique_ptr<C> basic_chart;

public:
  RelativeHumidityChart(RelativeHumidityChart &&) = delete;
  RelativeHumidityChart &operator=(const RelativeHumidityChart &) = delete;
  RelativeHumidityChart(InitArg init);
  //
  virtual void onActivate() override {
    if (tile_obj == nullptr) {
      M5_LOGE("tile had null");
      return;
    }
    if (!basic_chart) {
      // create
      basic_chart = std::make_unique<C>(tile_obj, "Relative Humidity");
    }
    render();
  }
  //
  virtual void onDeactivate() override {
    //    if (basic_chart) {
    //      basic_chart.reset();
    //    }
  }
  //
  virtual void update() override;
  //
  void render();
};

//
// 気圧
//
class PressureChart final : public TileBase {
  //
  class C : public BasicChart<double> {
  public:
    //
    C(lv_obj_t *parent_obj, std::string_view inSubheading)
        : BasicChart<double>(parent_obj, inSubheading) {}
    // データーベースからデーターを得る
    virtual size_t read_measurements_from_database(
        Database::OrderBy order, system_clock::time_point at_begin,
        Database::ReadCallback<Database::TimePointAndDouble> callback) override;
    // データを座標に変換する関数
    virtual lv_point_t
    coordinateXY(system_clock::time_point tp_zero,
                 const Database::TimePointAndDouble &in) override;
    //
    constexpr static DeciPa BIAS = round<DeciPa>(HectoPa{1000});
    //
    virtual void
    chart_draw_part_tick_label(lv_obj_draw_part_dsc_t *dsc) override;
  };

private:
  using Measurements = std::tuple<std::optional<Sensor::MeasurementBme280>,
                                  std::optional<Sensor::MeasurementM5Env3>>;
  Measurements latest{};
  std::unique_ptr<C> basic_chart;

public:
  PressureChart(PressureChart &&) = delete;
  PressureChart &operator=(const PressureChart &) = delete;
  PressureChart(InitArg init);
  //
  virtual void onActivate() override {
    if (tile_obj == nullptr) {
      M5_LOGE("tile had null");
      return;
    }
    if (!basic_chart) {
      // create
      basic_chart = std::make_unique<C>(tile_obj, "Pressure");
    }
    render();
  }
  //
  virtual void onDeactivate() override {
    //    if (basic_chart) {
    //      basic_chart.reset();
    //    }
  }
  //
  virtual void update() override;
  //
  void render();
};

//
// Co2 / ECo2
//
class CarbonDeoxidesChart final : public TileBase {
  //
  class C : public BasicChart<uint16_t> {
  public:
    //
    C(lv_obj_t *parent_obj, std::string_view inSubheading)
        : BasicChart<uint16_t>(parent_obj, inSubheading) {}
    // データーベースからデーターを得る
    virtual size_t read_measurements_from_database(
        Database::OrderBy order, system_clock::time_point at_begin,
        Database::ReadCallback<Database::TimePointAndUInt16> callback) override;
    // データを座標に変換する関数
    virtual lv_point_t
    coordinateXY(system_clock::time_point tp_zero,
                 const Database::TimePointAndUInt16 &in) override;
    //
    virtual void
    chart_draw_part_tick_label(lv_obj_draw_part_dsc_t *dsc) override;
  };

private:
  using Measurements = std::tuple<std::optional<Sensor::MeasurementSgp30>,
                                  std::optional<Sensor::MeasurementScd30>,
                                  std::optional<Sensor::MeasurementScd41>>;
  Measurements latest{};
  std::unique_ptr<C> basic_chart;

public:
  CarbonDeoxidesChart(CarbonDeoxidesChart &&) = delete;
  CarbonDeoxidesChart &operator=(const CarbonDeoxidesChart &) = delete;
  CarbonDeoxidesChart(InitArg init);
  //
  virtual void onActivate() override {
    if (tile_obj == nullptr) {
      M5_LOGE("tile had null");
      return;
    }
    if (!basic_chart) {
      // create
      basic_chart = std::make_unique<C>(tile_obj, "CO2");
    }
    render();
  }
  //
  virtual void onDeactivate() override {
    //    if (basic_chart) {
    //      basic_chart.reset();
    //    }
  }
  //
  virtual void update() override;
  //
  void render();
};

//
// Total VOC
//
class TotalVocChart final : public TileBase {
  //
  class C : public BasicChart<uint16_t> {
  public:
    //
    C(lv_obj_t *parent_obj, std::string_view inSubheading)
        : BasicChart<uint16_t>(parent_obj, inSubheading) {}
    // データーベースからデーターを得る
    virtual size_t read_measurements_from_database(
        Database::OrderBy order, system_clock::time_point at_begin,
        Database::ReadCallback<Database::TimePointAndUInt16> callback) override;
    // データを座標に変換する関数
    virtual lv_point_t
    coordinateXY(system_clock::time_point tp_zero,
                 const Database::TimePointAndUInt16 &in) override;
    //
    constexpr static int16_t DIVIDER = 2;
    //
    virtual void
    chart_draw_part_tick_label(lv_obj_draw_part_dsc_t *dsc) override;
  };

private:
  std::optional<Sensor::MeasurementSgp30> latest{};
  std::unique_ptr<C> basic_chart;

public:
  TotalVocChart(TotalVocChart &&) = delete;
  TotalVocChart &operator=(const TotalVocChart &) = delete;
  TotalVocChart(InitArg init);
  //
  virtual void onActivate() override {
    if (tile_obj == nullptr) {
      M5_LOGE("tile had null");
      return;
    }
    if (!basic_chart) {
      // create
      basic_chart = std::make_unique<C>(tile_obj, "Total VOC");
    }
    render();
  }
  //
  virtual void onDeactivate() override {
    //    if (basic_chart) {
    //      basic_chart.reset();
    //    }
  }
  //
  virtual void update() override;
  //
  void render();
};

//
//
//
class Startup final {
  lv_style_t title_style{};
  lv_style_t label_style{};
  lv_obj_t *container_obj{nullptr};
  lv_obj_t *title_obj{nullptr};
  lv_obj_t *label_obj{nullptr};
  lv_obj_t *bar_obj{nullptr};

public:
  Startup(Startup &&) = delete;
  Startup &operator=(const Startup &) = delete;
  Startup(lv_coord_t display_width, lv_coord_t display_height);
  ~Startup();
  //
  void updateMessage(const std::string &s);
  //
  void updateProgress(int16_t percent);
};

} // namespace Widget

//
//
//
class Gui {
public:
  constexpr static auto PERIODIC_TIMER_INTERVAL = std::chrono::milliseconds{30};
  constexpr static uint16_t CHART_X_POINT_COUNT = 180;
  Gui(M5GFX &gfx) : gfx{gfx} {}
  //
  bool begin();
  //
  bool startUi();
  //
  void home();
  //
  void movePrev();
  //
  void moveNext();
  //
  void vibrate();
  //
  bool update_startup_progress(int16_t percent) {
    if (_startup_widget) {
      _startup_widget->updateProgress(percent);
      return true;
    }
    return false;
  }
  //
  bool update_startup_message(const std::string &s) {
    if (_startup_widget) {
      _startup_widget->updateMessage(s);
      return true;
    }
    return false;
  }

private:
  M5GFX &gfx;
  //
  std::unique_ptr<Widget::Startup> _startup_widget{};
  //
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
  constexpr static size_t LVGL_BUFFER_ONE_SIZE_OF_BYTES = 8192;
  // LVGL use area
  struct LvglUseArea {
    // LVGL draw buffer
    static lv_color_t
        draw_buf_1[LVGL_BUFFER_ONE_SIZE_OF_BYTES / sizeof(lv_color_t)];
    static lv_color_t
        draw_buf_2[LVGL_BUFFER_ONE_SIZE_OF_BYTES / sizeof(lv_color_t)];
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
