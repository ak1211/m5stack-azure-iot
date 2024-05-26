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
using InitArg =
    std::tuple<std::shared_ptr<lv_obj_t>, uint8_t, uint8_t, lv_dir_t>;
//
constexpr static auto MARGIN{8};

//
class TileBase {
  struct LvTimerDeleter {
    void operator()(lv_timer_t *ptr) const;
  };
  // LVGL timer
  std::unique_ptr<lv_timer_t, LvTimerDeleter> _periodic_timer;

protected:
  std::shared_ptr<lv_obj_t> _tileview_obj;
  std::shared_ptr<lv_obj_t> _tile_obj;
  std::shared_ptr<lv_obj_t> _title_obj;
  lv_style_t _title_style{};

public:
  constexpr static auto PERIODIC_TIMER_INTERVAL = std::chrono::milliseconds{30};
  //
  TileBase(InitArg init, std::string title);
  //
  virtual ~TileBase();
  //
  bool isActiveTile() const;
  //
  void setActiveTile();
  //
  bool createTimer();
  //
  void delTimer();
  //
  static void timer_callback(lv_timer_t *timer);
  //
  static void event_value_changed_callback(lv_event_t *event);
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
  std::shared_ptr<lv_obj_t> _message_label_obj;
  std::string _latest{};

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
  std::shared_ptr<lv_obj_t> _table_obj;
  //
  using Measurements = std::tuple<std::optional<Sensor::MeasurementBme280>,
                                  std::optional<Sensor::MeasurementSgp30>,
                                  std::optional<Sensor::MeasurementScd30>,
                                  std::optional<Sensor::MeasurementScd41>,
                                  std::optional<Sensor::MeasurementM5Env3>>;
  Measurements _latest{};

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

private:
  //
  static void event_draw_part_begin_callback(lv_event_t *event);
};

//
//
//
class Clock final : public TileBase {
  std::shared_ptr<lv_obj_t> _meter_obj;
  std::shared_ptr<lv_meter_scale_t> _sec_scale;
  std::shared_ptr<lv_meter_scale_t> _min_scale;
  std::shared_ptr<lv_meter_scale_t> _hour_scale;
  std::shared_ptr<lv_meter_indicator_t> _sec_indic;
  std::shared_ptr<lv_meter_indicator_t> _min_indic;
  std::shared_ptr<lv_meter_indicator_t> _hour_indic;

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
  std::shared_ptr<lv_obj_t> _table_obj;

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

private:
  //
  static void event_draw_part_begin_callback(lv_event_t *event);
};

//
//
//
class MessageBox {
  using ButtonClickCallback = std::function<void(uint16_t)>;

public:
  MessageBox(lv_obj_t *parent, std::string_view title, std::string_view text,
             std::vector<const char *> buttons, ButtonClickCallback callback);

private:
  std::shared_ptr<lv_obj_t> _msgbox_obj;
  std::vector<const char *> _msgbox_buttons;
  ButtonClickCallback _button_click_callback;
  //
  static void event_all_callback(lv_event_t *event);
};

//
//
//
class ExportImportData final : public TileBase {
  std::shared_ptr<lv_obj_t> _export_button_obj;
  std::shared_ptr<lv_obj_t> _import_button_obj;
  std::shared_ptr<MessageBox> _messagebox;

public:
  constexpr static auto GUTTER{36};
  ExportImportData(ExportImportData &&) = delete;
  ExportImportData &operator=(const ExportImportData &) = delete;
  ExportImportData(InitArg init);
  //
  virtual void onActivate() override {}
  //
  virtual void onDeactivate() override {}
  //
  virtual void update() override;

private:
  //
  void doExport();
  //
  void showExportMessageBox();
  //
  void showImportMessageBox();
  //
  static void click_cloce_button_callback(lv_event_t *event);
  //
  static void event_all_callback(lv_event_t *event);
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
struct ChartSeriesWrapper {
  lv_chart_series_t *chart_series{nullptr};
  std::vector<lv_coord_t> y_points;
  //
  ChartSeriesWrapper(lv_chart_series_t *series, size_t num_points)
      : chart_series{series}, y_points(num_points) {
    if (chart_series == nullptr) {
      M5_LOGD("chart series has null");
      return;
    }
    fill(LV_CHART_POINT_NONE);
  }
  //
  void fill(lv_coord_t v) { std::fill(y_points.begin(), y_points.end(), v); }
  //
  std::pair<lv_coord_t, lv_coord_t> getMinMaxOfYPoints() {
    auto y_min = std::numeric_limits<lv_coord_t>::max();
    auto y_max = std::numeric_limits<lv_coord_t>::min();
    if (chart_series) {
      for (auto &y : y_points) {
        if (y != LV_CHART_POINT_NONE) {
          y_min = std::min(y_min, y);
          y_max = std::max(y_max, y);
        }
      }
    } else {
      M5_LOGD("chart_series had null");
    }
    return {y_min, y_max};
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

//
template <typename T>
std::ostream &operator<<(std::ostream &os, const ShowMeasured<T> &rhs);

//
//
//
template <typename T> class BasicChart {
public:
  constexpr static auto X_AXIS_TICK_COUNT{3};
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
  BasicChart(std::shared_ptr<lv_obj_t> parent_obj,
             std::shared_ptr<lv_obj_t> title_obj);
  //
  virtual ~BasicChart() { /* nothing to do */ }
  //
  std::shared_ptr<lv_obj_t> getLabelObj() { return _label_obj; }
  //
  std::shared_ptr<lv_obj_t> getChartObj() { return _chart_obj; }
  //
  system_clock::time_point getBeginX() const { return _begin_x_tp; }
  //
  void render();

private:
  lv_style_t _label_style{};
  std::shared_ptr<lv_obj_t> _label_obj;
  std::shared_ptr<lv_obj_t> _chart_obj;
  //
  std::unordered_map<SensorId, ChartSeriesWrapper> _chart_series_map{};
  //
  system_clock::time_point _begin_x_tp{};
  //
  static void event_draw_part_begin_callback(lv_event_t *event);
};

//
// 気温
//
class TemperatureChart final : public TileBase {
  //
  class BC : public BasicChart<double> {
  public:
    //
    BC(std::shared_ptr<lv_obj_t> parent_obj,
       std::shared_ptr<lv_obj_t> title_obj)
        : BasicChart<double>(parent_obj, title_obj) {}
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
  //
  using Measurements = std::tuple<std::optional<Sensor::MeasurementBme280>,
                                  std::optional<Sensor::MeasurementScd30>,
                                  std::optional<Sensor::MeasurementScd41>,
                                  std::optional<Sensor::MeasurementM5Env3>>;
  Measurements latest{};
  std::unique_ptr<BC> basic_chart;

public:
  TemperatureChart(TemperatureChart &&) = delete;
  TemperatureChart &operator=(const TemperatureChart &) = delete;
  TemperatureChart(InitArg init);
  //
  virtual void onActivate() override {
    if (_tile_obj == nullptr) {
      M5_LOGE("tile had null");
      return;
    }
    if (!basic_chart) {
      // create
      basic_chart = std::make_unique<BC>(_tile_obj, _title_obj);
    }
    render();
  }
  //
  virtual void onDeactivate() override {
    if (basic_chart) {
      basic_chart.reset();
    }
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
  class BC : public BasicChart<double> {
  public:
    //
    BC(std::shared_ptr<lv_obj_t> parent_obj,
       std::shared_ptr<lv_obj_t> title_obj)
        : BasicChart<double>(parent_obj, title_obj) {}
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
  //
  using Measurements = std::tuple<std::optional<Sensor::MeasurementBme280>,
                                  std::optional<Sensor::MeasurementScd30>,
                                  std::optional<Sensor::MeasurementScd41>,
                                  std::optional<Sensor::MeasurementM5Env3>>;
  Measurements latest{};
  std::unique_ptr<BC> basic_chart;

public:
  RelativeHumidityChart(RelativeHumidityChart &&) = delete;
  RelativeHumidityChart &operator=(const RelativeHumidityChart &) = delete;
  RelativeHumidityChart(InitArg init);
  //
  virtual void onActivate() override {
    if (_tile_obj == nullptr) {
      M5_LOGE("tile had null");
      return;
    }
    if (!basic_chart) {
      // create
      basic_chart = std::make_unique<BC>(_tile_obj, _title_obj);
    }
    render();
  }
  //
  virtual void onDeactivate() override {
    if (basic_chart) {
      basic_chart.reset();
    }
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
  class BC : public BasicChart<double> {
  public:
    //
    BC(std::shared_ptr<lv_obj_t> parent_obj,
       std::shared_ptr<lv_obj_t> title_obj)
        : BasicChart<double>(parent_obj, title_obj) {}
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
  //
  using Measurements = std::tuple<std::optional<Sensor::MeasurementBme280>,
                                  std::optional<Sensor::MeasurementM5Env3>>;
  Measurements latest{};
  std::unique_ptr<BC> basic_chart;

public:
  PressureChart(PressureChart &&) = delete;
  PressureChart &operator=(const PressureChart &) = delete;
  PressureChart(InitArg init);
  //
  virtual void onActivate() override {
    if (_tile_obj == nullptr) {
      M5_LOGE("tile had null");
      return;
    }
    if (!basic_chart) {
      // create
      basic_chart = std::make_unique<BC>(_tile_obj, _title_obj);
    }
    render();
  }
  //
  virtual void onDeactivate() override {
    if (basic_chart) {
      basic_chart.reset();
    }
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
  class BC : public BasicChart<uint16_t> {
  public:
    //
    BC(std::shared_ptr<lv_obj_t> parent_obj,
       std::shared_ptr<lv_obj_t> title_obj)
        : BasicChart<uint16_t>(parent_obj, title_obj) {}
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
  //
  using Measurements = std::tuple<std::optional<Sensor::MeasurementSgp30>,
                                  std::optional<Sensor::MeasurementScd30>,
                                  std::optional<Sensor::MeasurementScd41>>;
  Measurements latest{};
  std::unique_ptr<BC> basic_chart;

public:
  CarbonDeoxidesChart(CarbonDeoxidesChart &&) = delete;
  CarbonDeoxidesChart &operator=(const CarbonDeoxidesChart &) = delete;
  CarbonDeoxidesChart(InitArg init);
  //
  virtual void onActivate() override {
    if (_tile_obj == nullptr) {
      M5_LOGE("tile had null");
      return;
    }
    if (!basic_chart) {
      // create
      basic_chart = std::make_unique<BC>(_tile_obj, _title_obj);
    }
    render();
  }
  //
  virtual void onDeactivate() override {
    if (basic_chart) {
      basic_chart.reset();
    }
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
  class BC : public BasicChart<uint16_t> {
  public:
    //
    BC(std::shared_ptr<lv_obj_t> parent_obj,
       std::shared_ptr<lv_obj_t> title_obj)
        : BasicChart<uint16_t>(parent_obj, title_obj) {}
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
  //
  std::optional<Sensor::MeasurementSgp30> latest{};
  std::unique_ptr<BC> basic_chart;

public:
  TotalVocChart(TotalVocChart &&) = delete;
  TotalVocChart &operator=(const TotalVocChart &) = delete;
  TotalVocChart(InitArg init);
  //
  virtual void onActivate() override {
    if (_tile_obj == nullptr) {
      M5_LOGE("tile had null");
      return;
    }
    if (!basic_chart) {
      // create
      basic_chart = std::make_unique<BC>(_tile_obj, _title_obj);
    }
    render();
  }
  //
  virtual void onDeactivate() override {
    if (basic_chart) {
      basic_chart.reset();
    }
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
  lv_style_t _title_style{};
  lv_style_t _label_style{};
  std::shared_ptr<lv_obj_t> _container_obj;
  std::shared_ptr<lv_obj_t> _title_obj;
  std::shared_ptr<lv_obj_t> _label_obj;
  std::shared_ptr<lv_obj_t> _bar_obj;

public:
  Startup(Startup &&) = delete;
  Startup &operator=(const Startup &) = delete;
  Startup(lv_coord_t display_width, lv_coord_t display_height);
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
  constexpr static uint16_t CHART_X_POINT_COUNT = 1440;
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
  std::shared_ptr<lv_obj_t> _tileview_obj;
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
