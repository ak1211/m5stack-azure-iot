// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include "Application.hpp"
#include "Sensor.hpp"
#include <M5Core2.h>
#undef min
#undef max
#define LGFX_USE_V1
#define LGFX_AUTODETECT
#include <LovyanGFX.hpp>
#include <algorithm>
#include <chrono>
#include <lvgl.h>
#include <memory>
#include <tuple>

namespace GUI::Tile {
struct Interface {
  virtual void setActiveTile(lv_obj_t *tileview) noexcept = 0;
  virtual bool isActiveTile(lv_obj_t *tileview) const noexcept = 0;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept = 0;
  virtual void timerHook() noexcept = 0;
};
} // namespace GUI::Tile

namespace GUI {
// LovyanGFX
extern LGFX lcd;
// display resolution
constexpr static auto lcdHorizResolution{
    Application::M5StackCore2_HorizResolution};
constexpr static auto lcdVertResolution{
    Application::M5StackCore2_VertResolution};
// draw buffer
constexpr auto DRAW_BUF_1_SIZE =
    lcdHorizResolution * 50; // A buffer for 50 rows
extern std::unique_ptr<lv_color_t[]> draw_buf_1;
extern lv_disp_draw_buf_t draw_buf_dsc_1;
// display driver
extern lv_disp_drv_t disp_drv;
// touchpad
extern lv_indev_t *indev_touchpad;
extern lv_indev_drv_t indev_drv;
// bootstrapping message
extern std::vector<char> bootstrapping_message_cstr;
// tile widget
using TileVector = std::vector<std::unique_ptr<Tile::Interface>>;
extern lv_obj_t *tileview;
extern TileVector tiles;
//
extern void init() noexcept;
//
extern void showBootstrappingMessage(std::string_view msg) noexcept;
//
extern void startUI() noexcept;
//
extern void home() noexcept;
//
extern void prev() noexcept;
//
extern void next() noexcept;
//
extern void vibrate() noexcept;
} // namespace GUI

namespace GUI::Tile {
using namespace std::chrono;
// init argument for "lv_tileview_add_tile()"
using Init = std::tuple<lv_obj_t *, uint8_t, uint8_t, lv_dir_t>;

//
class BootMessage final : public Interface {
  lv_obj_t *tile{nullptr};
  lv_obj_t *message_label{nullptr};

public:
  BootMessage(Init init) noexcept;
  BootMessage(BootMessage &&) = delete;
  BootMessage &operator=(const BootMessage &) = delete;
  virtual ~BootMessage() noexcept;
  virtual void setActiveTile(lv_obj_t *tileview) noexcept override {
    lv_obj_set_tile(tileview, tile, LV_ANIM_OFF);
  }
  virtual bool isActiveTile(lv_obj_t *tileview) const noexcept override {
    return tile == lv_tileview_get_tile_act(tileview);
  }
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  void render() noexcept;
};

//
class Summary final : public Interface {
  lv_obj_t *tile{nullptr};
  lv_obj_t *table{nullptr};
  //
  using Measurements = std::tuple<
      std::optional<MeasurementBme280>, std::optional<MeasurementSgp30>,
      std::optional<MeasurementScd30>, std::optional<MeasurementM5Env3>>;
  Measurements latest;

public:
  Summary(Init init) noexcept;
  Summary(Summary &&) = delete;
  Summary &operator=(const Summary &) = delete;
  virtual ~Summary() noexcept;
  virtual void setActiveTile(lv_obj_t *tileview) noexcept override {
    lv_obj_set_tile(tileview, tile, LV_ANIM_OFF);
  }
  virtual bool isActiveTile(lv_obj_t *tileview) const noexcept override {
    return tile == lv_tileview_get_tile_act(tileview);
  }
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  void render() noexcept;
};

//
class Clock final : public Interface {
  lv_obj_t *tile{nullptr};
  lv_obj_t *meter{nullptr};
  lv_meter_scale_t *sec_scale{nullptr};
  lv_meter_scale_t *min_scale{nullptr};
  lv_meter_scale_t *hour_scale{nullptr};
  lv_meter_indicator_t *sec_indic{nullptr};
  lv_meter_indicator_t *min_indic{nullptr};
  lv_meter_indicator_t *hour_indic{nullptr};

public:
  Clock(Init init) noexcept;
  Clock(Clock &&) = delete;
  Clock &operator=(const Clock &) = delete;
  virtual ~Clock() noexcept;
  virtual void setActiveTile(lv_obj_t *tileview) noexcept override {
    lv_obj_set_tile(tileview, tile, LV_ANIM_OFF);
  }
  virtual bool isActiveTile(lv_obj_t *tileview) const noexcept override {
    return tile == lv_tileview_get_tile_act(tileview);
  }
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  void render(const std::tm &tm) noexcept;
};

//
class SystemHealth final : public Interface {
  lv_obj_t *tile{nullptr};
  lv_obj_t *cont_col{nullptr};
  //
  lv_style_t style_label;
  //
  lv_obj_t *time_label{nullptr};
  lv_obj_t *status_label{nullptr};
  lv_obj_t *power_source_label{nullptr};
  lv_obj_t *battery_label{nullptr};

public:
  SystemHealth(Init init) noexcept;
  SystemHealth(SystemHealth &&) = delete;
  SystemHealth &operator=(const SystemHealth &) = delete;
  virtual ~SystemHealth() noexcept;
  virtual void setActiveTile(lv_obj_t *tileview) noexcept override {
    lv_obj_set_tile(tileview, tile, LV_ANIM_OFF);
  }
  virtual bool isActiveTile(lv_obj_t *tileview) const noexcept override {
    return tile == lv_tileview_get_tile_act(tileview);
  }
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  void render() noexcept;
};

//
template <typename T> class BasicChart : public Interface {
protected:
  lv_obj_t *tile{nullptr};
  lv_obj_t *container{nullptr};
  lv_obj_t *chart{nullptr};
  lv_obj_t *title{nullptr};
  lv_obj_t *label{nullptr};
  lv_chart_series_t *ser_one{nullptr};
  //
  using ValueT = T;
  using ValuePair = std::pair<system_clock::time_point, ValueT>;
  std::string subheading{};
  std::optional<ValuePair> latest{};

public:
  BasicChart(Init init, const std::string &inSubheading) noexcept;
  BasicChart(BasicChart &&) = delete;
  BasicChart &operator=(const BasicChart &) = delete;
  virtual ~BasicChart() noexcept;
  virtual void setActiveTile(lv_obj_t *tileview) noexcept override {
    lv_obj_set_tile(tileview, tile, LV_ANIM_OFF);
  }
  virtual bool isActiveTile(lv_obj_t *tileview) const noexcept override {
    return tile == lv_tileview_get_tile_act(tileview);
  }
  template <typename U>
  void setHeading(std::string_view strUnit,
                  std::optional<system_clock::time_point> optTP,
                  std::optional<U> optValue) noexcept;
  void
  setChartValue(const std::vector<ValuePair> &histories,
                std::function<lv_coord_t(const ValueT &)> mapping) noexcept;
};

//
class M5Env3TemperatureChart final : public BasicChart<Sensor::M5Env3> {
public:
  M5Env3TemperatureChart(Init init) noexcept;
  M5Env3TemperatureChart(M5Env3TemperatureChart &&) = delete;
  M5Env3TemperatureChart &operator=(const M5Env3TemperatureChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static lv_coord_t map_to_coord_t(const ValueT &in) noexcept;
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
class Bme280TemperatureChart final : public BasicChart<Sensor::Bme280> {
public:
  Bme280TemperatureChart(Init init) noexcept;
  Bme280TemperatureChart(Bme280TemperatureChart &&) = delete;
  Bme280TemperatureChart &operator=(const Bme280TemperatureChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static lv_coord_t map_to_coord_t(const ValueT &in) noexcept;
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
class Scd30TemperatureChart final : public BasicChart<Sensor::Scd30> {
public:
  Scd30TemperatureChart(Init init) noexcept;
  Scd30TemperatureChart(Scd30TemperatureChart &&) = delete;
  Scd30TemperatureChart &operator=(const Scd30TemperatureChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static lv_coord_t map_to_coord_t(const ValueT &in) noexcept;
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
class Scd41TemperatureChart final : public BasicChart<Sensor::Scd41> {
public:
  Scd41TemperatureChart(Init init) noexcept;
  Scd41TemperatureChart(Scd41TemperatureChart &&) = delete;
  Scd41TemperatureChart &operator=(const Scd30TemperatureChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static lv_coord_t map_to_coord_t(const ValueT &in) noexcept;
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
class M5Env3RelativeHumidityChart final : public BasicChart<Sensor::M5Env3> {
public:
  M5Env3RelativeHumidityChart(Init init) noexcept;
  M5Env3RelativeHumidityChart(M5Env3RelativeHumidityChart &&) = delete;
  M5Env3RelativeHumidityChart &
  operator=(const M5Env3RelativeHumidityChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static lv_coord_t map_to_coord_t(const ValueT &in) noexcept;
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
class Bme280RelativeHumidityChart final : public BasicChart<Sensor::Bme280> {
public:
  Bme280RelativeHumidityChart(Init init) noexcept;
  Bme280RelativeHumidityChart(Bme280RelativeHumidityChart &&) = delete;
  Bme280RelativeHumidityChart &
  operator=(const Bme280RelativeHumidityChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static lv_coord_t map_to_coord_t(const ValueT &in) noexcept;
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
class Scd30RelativeHumidityChart final : public BasicChart<Sensor::Scd30> {
public:
  Scd30RelativeHumidityChart(Init init) noexcept;
  Scd30RelativeHumidityChart(Scd30RelativeHumidityChart &&) = delete;
  Scd30RelativeHumidityChart &
  operator=(const Scd30RelativeHumidityChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  //
  virtual void timerHook() noexcept override;
  static lv_coord_t map_to_coord_t(const ValueT &in) noexcept;
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
class Scd41RelativeHumidityChart final : public BasicChart<Sensor::Scd41> {
public:
  Scd41RelativeHumidityChart(Init init) noexcept;
  Scd41RelativeHumidityChart(Scd41RelativeHumidityChart &&) = delete;
  Scd41RelativeHumidityChart &
  operator=(const Scd41RelativeHumidityChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  //
  virtual void timerHook() noexcept override;
  static lv_coord_t map_to_coord_t(const ValueT &in) noexcept;
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
class M5Env3PressureChart final : public BasicChart<Sensor::M5Env3> {
public:
  M5Env3PressureChart(Init init) noexcept;
  M5Env3PressureChart(M5Env3PressureChart &&) = delete;
  M5Env3PressureChart &operator=(const M5Env3PressureChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  constexpr static DeciPa BIAS = round<DeciPa>(HectoPa{1000});
  static lv_coord_t map_to_coord_t(const ValueT &in) noexcept;
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
class Bme280PressureChart final : public BasicChart<Sensor::Bme280> {
public:
  Bme280PressureChart(Init init) noexcept;
  Bme280PressureChart(Bme280PressureChart &&) = delete;
  Bme280PressureChart &operator=(const Bme280PressureChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  constexpr static DeciPa BIAS = round<DeciPa>(HectoPa{1000});
  static lv_coord_t map_to_coord_t(const ValueT &in) noexcept;
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
class Sgp30TotalVocChart final : public BasicChart<Sensor::Sgp30> {
public:
  Sgp30TotalVocChart(Init init) noexcept;
  Sgp30TotalVocChart(Sgp30TotalVocChart &&) = delete;
  Sgp30TotalVocChart &operator=(const Sgp30TotalVocChart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static lv_coord_t map_to_coord_t(const ValueT &in) noexcept;
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
class Sgp30Eco2Chart final : public BasicChart<Sensor::Sgp30> {
public:
  Sgp30Eco2Chart(Init init) noexcept;
  Sgp30Eco2Chart(Sgp30Eco2Chart &&) = delete;
  Sgp30Eco2Chart &operator=(const Sgp30Eco2Chart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
  //
  static lv_coord_t map_to_coord_t(const ValueT &in) noexcept;
  static void drawEventHook(lv_event_t *event) noexcept;
};

//
class Scd30Co2Chart final : public BasicChart<Sensor::Scd30> {
public:
  Scd30Co2Chart(Init init) noexcept;
  Scd30Co2Chart(Scd30Co2Chart &&) = delete;
  Scd30Co2Chart &operator=(const Scd30Co2Chart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
};

//
class Scd41Co2Chart final : public BasicChart<Sensor::Scd41> {
public:
  Scd41Co2Chart(Init init) noexcept;
  Scd41Co2Chart(Scd41Co2Chart &&) = delete;
  Scd41Co2Chart &operator=(const Scd41Co2Chart &) = delete;
  virtual void valueChangedEventHook(lv_event_t *event) noexcept override;
  virtual void timerHook() noexcept override;
};
} // namespace GUI::Tile
