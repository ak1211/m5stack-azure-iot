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
template <typename DataType> class BasicChart : public TileBase {
protected:
  lv_obj_t *tile_obj{nullptr};
  lv_obj_t *container_obj{nullptr};
  lv_obj_t *chart_obj{nullptr};
  lv_obj_t *title_obj{nullptr};
  lv_obj_t *label_obj{nullptr};
  //
  constexpr static std::array<lv_palette_t, 6> LINE_COLOR_TABLE{
      LV_PALETTE_RED,    LV_PALETTE_ORANGE, LV_PALETTE_INDIGO,
      LV_PALETTE_PURPLE, LV_PALETTE_BROWN,  LV_PALETTE_BLUE,
  };
  //
  std::vector<lv_chart_series_t *> chart_series_vect{};
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
  /*
  template <typename U>
  void setHeading(std::string_view strUnit,
                  std::optional<system_clock::time_point> opt_timepoint,
                  std::optional<U> opt_value) noexcept;
                  */
  void
  setChartValue(const std::vector<DataType> &histories,
                std::function<lv_coord_t(const DataType &)> mapping) noexcept;
};

//
// 気温
//
class TemperatureChart final : public BasicChart<Database::TimePointAndDouble> {
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
  //
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
// 相対湿度
//
class RelativeHumidityChart final
    : public BasicChart<Database::TimePointAndDouble> {
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
  //
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
// 気圧
//
class PressureChart final : public BasicChart<Database::TimePointAndDouble> {
private:
  using Measurements = std::tuple<std::optional<Sensor::MeasurementBme280>,
                                  std::optional<Sensor::MeasurementScd30>,
                                  std::optional<Sensor::MeasurementScd41>,
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
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
// Co2 / ECo2
//
class CarbonDeoxidesChart final
    : public BasicChart<Database::TimePointAndIntAndOptInt> {
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
  //
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
// Total VOC
//
class TotalVocChart final
    : public BasicChart<Database::TimePointAndIntAndOptInt> {
private:
  std::optional<Sensor::MeasurementSgp30> latest{};

public:
  TotalVocChart(InitArg init) noexcept;
  TotalVocChart(TotalVocChart &&) = delete;
  TotalVocChart &operator=(const TotalVocChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static void drawEventHook(lv_event_t *event) noexcept;
};

#if 0
//
//
//
class M5Env3TemperatureChart final
    : public BasicChart<Database::TimePointAndDouble> {
private:
  std::optional<Sensor::MeasurementM5Env3> latest{};

public:
  M5Env3TemperatureChart(InitArg init) noexcept;
  M5Env3TemperatureChart(M5Env3TemperatureChart &&) = delete;
  M5Env3TemperatureChart &operator=(const M5Env3TemperatureChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
//
//
class Bme280TemperatureChart final
    : public BasicChart<Database::TimePointAndDouble> {
private:
  std::optional<Database::RowId> displayed_rowid{};

public:
  Bme280TemperatureChart(InitArg init) noexcept;
  Bme280TemperatureChart(Bme280TemperatureChart &&) = delete;
  Bme280TemperatureChart &operator=(const Bme280TemperatureChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
//
//
class Scd30TemperatureChart final
    : public BasicChart<Database::TimePointAndDouble> {
private:
  std::optional<Database::RowId> displayed_rowid{};

public:
  Scd30TemperatureChart(InitArg init) noexcept;
  Scd30TemperatureChart(Scd30TemperatureChart &&) = delete;
  Scd30TemperatureChart &operator=(const Scd30TemperatureChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
//
//
class Scd41TemperatureChart final
    : public BasicChart<Database::TimePointAndDouble> {
private:
  std::optional<Database::RowId> displayed_rowid{};

public:
  Scd41TemperatureChart(InitArg init) noexcept;
  Scd41TemperatureChart(Scd41TemperatureChart &&) = delete;
  Scd41TemperatureChart &operator=(const Scd30TemperatureChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
//
//
class M5Env3RelativeHumidityChart final
    : public BasicChart<Database::TimePointAndDouble> {
private:
  std::optional<Database::RowId> displayed_rowid{};

public:
  M5Env3RelativeHumidityChart(InitArg init) noexcept;
  M5Env3RelativeHumidityChart(M5Env3RelativeHumidityChart &&) = delete;
  M5Env3RelativeHumidityChart &
  operator=(const M5Env3RelativeHumidityChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
//
//
class Bme280RelativeHumidityChart final
    : public BasicChart<Database::TimePointAndDouble> {
private:
  std::optional<Database::RowId> displayed_rowid{};

public:
  Bme280RelativeHumidityChart(InitArg init) noexcept;
  Bme280RelativeHumidityChart(Bme280RelativeHumidityChart &&) = delete;
  Bme280RelativeHumidityChart &
  operator=(const Bme280RelativeHumidityChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
//
//
class Scd30RelativeHumidityChart final
    : public BasicChart<Database::TimePointAndDouble> {
private:
  std::optional<Database::RowId> displayed_rowid{};

public:
  Scd30RelativeHumidityChart(InitArg init) noexcept;
  Scd30RelativeHumidityChart(Scd30RelativeHumidityChart &&) = delete;
  Scd30RelativeHumidityChart &
  operator=(const Scd30RelativeHumidityChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
//
//
class Scd41RelativeHumidityChart final
    : public BasicChart<Database::TimePointAndDouble> {
private:
  std::optional<Database::RowId> displayed_rowid{};

public:
  Scd41RelativeHumidityChart(InitArg init) noexcept;
  Scd41RelativeHumidityChart(Scd41RelativeHumidityChart &&) = delete;
  Scd41RelativeHumidityChart &
  operator=(const Scd41RelativeHumidityChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
//
//
class M5Env3PressureChart final
    : public BasicChart<Database::TimePointAndDouble> {
private:
  std::optional<Database::RowId> displayed_rowid{};

public:
  M5Env3PressureChart(InitArg init) noexcept;
  M5Env3PressureChart(M5Env3PressureChart &&) = delete;
  M5Env3PressureChart &operator=(const M5Env3PressureChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  constexpr static DeciPa BIAS = round<DeciPa>(HectoPa{1000});
  //
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
//
//
class Bme280PressureChart final
    : public BasicChart<Database::TimePointAndDouble> {
private:
  std::optional<Database::RowId> displayed_rowid{};

public:
  Bme280PressureChart(InitArg init) noexcept;
  Bme280PressureChart(Bme280PressureChart &&) = delete;
  Bme280PressureChart &operator=(const Bme280PressureChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  constexpr static DeciPa BIAS = round<DeciPa>(HectoPa{1000});
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
//
//
class Sgp30TotalVocChart final
    : public BasicChart<Database::TimePointAndIntAndOptInt> {
private:
  std::optional<Database::RowId> displayed_rowid{};

public:
  Sgp30TotalVocChart(InitArg init) noexcept;
  Sgp30TotalVocChart(Sgp30TotalVocChart &&) = delete;
  Sgp30TotalVocChart &operator=(const Sgp30TotalVocChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
//
//
class Sgp30Eco2Chart final
    : public BasicChart<Database::TimePointAndIntAndOptInt> {
private:
  std::optional<Database::RowId> displayed_rowid{};

public:
  Sgp30Eco2Chart(InitArg init) noexcept;
  Sgp30Eco2Chart(Sgp30Eco2Chart &&) = delete;
  Sgp30Eco2Chart &operator=(const Sgp30Eco2Chart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
//
//
class Scd30Co2Chart final
    : public BasicChart<Database::TimePointAndIntAndOptInt> {
private:
  std::optional<Database::RowId> displayed_rowid{};

public:
  Scd30Co2Chart(InitArg init) noexcept;
  Scd30Co2Chart(Scd30Co2Chart &&) = delete;
  Scd30Co2Chart &operator=(const Scd30Co2Chart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
};

//
//
//
class Scd41Co2Chart final
    : public BasicChart<Database::TimePointAndIntAndOptInt> {
private:
  std::optional<Database::RowId> displayed_rowid{};

public:
  Scd41Co2Chart(InitArg init) noexcept;
  Scd41Co2Chart(Scd41Co2Chart &&) = delete;
  Scd41Co2Chart &operator=(const Scd41Co2Chart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
};
#endif

} // namespace Widget

//
//
//
class Gui {
  inline static Gui *_instance{nullptr};
  constexpr static uint16_t MILLISECONDS_OF_PERIODIC_TIMER = 100;

public:
  constexpr static uint16_t CHART_X_POINT_COUNT = 120;
  Gui(M5GFX &gfx) : gfx{gfx} {
    if (_instance) {
      delete _instance;
    }
    _instance = this;
  }
  //
  virtual ~Gui() {}
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
