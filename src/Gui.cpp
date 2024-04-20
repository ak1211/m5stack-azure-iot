// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "Gui.hpp"
#include "Application.hpp"
#include "Database.hpp"
#include "Time.hpp"
#include <WiFi.h>
#include <esp_system.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <functional>
#include <iomanip>
#include <sstream>
#include <tuple>

#include <M5Unified.h>

using namespace std::chrono;
using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

//
void Gui::display_flush_callback(lv_disp_drv_t *disp_drv, const lv_area_t *area,
                                 lv_color_t *color_p) noexcept {
  M5GFX &gfx = *static_cast<M5GFX *>(disp_drv->user_data);

  int32_t width = area->x2 - area->x1 + 1;
  int32_t height = area->y2 - area->y1 + 1;

  gfx.startWrite();
  gfx.setAddrWindow(area->x1, area->y1, width, height);
  gfx.writePixels(reinterpret_cast<uint16_t *>(color_p), width * height, true);
  gfx.endWrite();

  /*IMPORTANT!!!
   *Inform the graphics library that you are ready with the flushing*/
  lv_disp_flush_ready(disp_drv);
}

//
void Gui::touchpad_read_callback(lv_indev_drv_t *indev_drv,
                                 lv_indev_data_t *data) noexcept {
  M5GFX &gfx = *static_cast<M5GFX *>(indev_drv->user_data);

  lgfx::touch_point_t tp;
  bool touched = gfx.getTouch(&tp);

  if (!touched) {
    data->state = LV_INDEV_STATE_REL;
  } else {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = tp.x;
    data->point.y = tp.y;
  }
}

//
bool Gui::begin() noexcept {
  // LVGL init
  lv_init();
  // LVGL draw buffer
  const int32_t DRAW_BUFFER_SIZE = gfx.width() * (gfx.height() / 10);
  lvgl_use.draw_buf_1 = std::make_unique<lv_color_t[]>(DRAW_BUFFER_SIZE);
  lvgl_use.draw_buf_2 = std::make_unique<lv_color_t[]>(DRAW_BUFFER_SIZE);
  if (lvgl_use.draw_buf_1 == nullptr || lvgl_use.draw_buf_2 == nullptr) {
    M5_LOGE("memory allocation error");
    return false;
  }
  lv_disp_draw_buf_init(&lvgl_use.draw_buf_dsc, lvgl_use.draw_buf_1.get(),
                        lvgl_use.draw_buf_2.get(), DRAW_BUFFER_SIZE);

  // LVGL display driver
  lv_disp_drv_init(&lvgl_use.disp_drv);
  lvgl_use.disp_drv.user_data = &gfx;
  lvgl_use.disp_drv.hor_res = gfx.width();
  lvgl_use.disp_drv.ver_res = gfx.height();
  lvgl_use.disp_drv.flush_cb = display_flush_callback;
  lvgl_use.disp_drv.draw_buf = &lvgl_use.draw_buf_dsc;
  // register the display driver
  lv_disp_drv_register(&lvgl_use.disp_drv);

  // LVGL (touchpad) input device driver
  lv_indev_drv_init(&lvgl_use.indev_drv);
  lvgl_use.indev_drv.user_data = &gfx;
  lvgl_use.indev_drv.type = LV_INDEV_TYPE_POINTER;
  lvgl_use.indev_drv.read_cb = touchpad_read_callback;
  // register the input device driver
  lv_indev_drv_register(&lvgl_use.indev_drv);

  // tileview init
  tileview = lv_tileview_create(lv_scr_act());
  // make the first tile
  add_tile<Widget::BootMessage>({tileview, 0, 0, LV_DIR_RIGHT});
  tiles.front()->setActiveTile(tileview);
  // set value changed callback
  lv_obj_add_event_cb(
      tileview,
      [](lv_event_t *event) noexcept -> void {
        auto itr = std::find_if(_instance->tiles.cbegin(),
                                _instance->tiles.cend(), check_if_active_tile);
        if (itr == _instance->tiles.cend()) {
          return;
        }
        if (auto found = itr->get(); found) {
          found->valueChangedEventHook(event);
        }
      },
      LV_EVENT_VALUE_CHANGED, nullptr);
  // set timer callback
  periodic_timer = lv_timer_create(
      [](lv_timer_t *) noexcept -> void {
        auto itr = std::find_if(_instance->tiles.cbegin(),
                                _instance->tiles.cend(), check_if_active_tile);
        if (itr == _instance->tiles.cend()) {
          return;
        }
        if (auto found = itr->get(); found) {
          found->timerHook();
        }
      },
      MILLISECONDS_OF_PERIODIC_TIMER, nullptr);

  return true;
}

//
void Gui::startUi() noexcept {
  using Fn = std::function<void(lv_obj_t * tv, int16_t col, uint8_t dir)>;
  // Clock
  if constexpr (false) {
    Fn addClock = [this](lv_obj_t *tv, int16_t col, uint8_t dir) {
      add_tile<Widget::Clock>({tv, col, 0, dir});
    };
  }
  // SystemHealth
  Fn addSystemHealth = [this](lv_obj_t *tv, int16_t col, uint8_t dir) {
    add_tile<Widget::SystemHealth>({tv, col, 0, dir});
  };
  // Summary
  Fn addSummary = [this](lv_obj_t *tv, int16_t col, uint8_t dir) {
    add_tile<Widget::Summary>({tv, col, 0, dir});
  };
  // M5 unit ENV3
  Fn addM5Env3 = [this](lv_obj_t *tv, int16_t col, uint8_t dir) {
    auto row = 0;
    dir = dir | LV_DIR_BOTTOM;
    add_tile<Widget::M5Env3TemperatureChart>({tv, col, row++, dir});
    dir = dir | LV_DIR_TOP;
    add_tile<Widget::M5Env3RelativeHumidityChart>({tv, col, row++, dir});
    dir = dir & ~LV_DIR_BOTTOM;
    add_tile<Widget::M5Env3PressureChart>({tv, col, row++, dir});
  };
  // BME280
  Fn addBme280 = [this](lv_obj_t *tv, int16_t col, uint8_t dir) {
    auto row = 0;
    dir = dir | LV_DIR_BOTTOM;
    add_tile<Widget::Bme280TemperatureChart>({tv, col, row++, dir});
    dir = dir | LV_DIR_TOP;
    add_tile<Widget::Bme280RelativeHumidityChart>({tv, col, row++, dir});
    dir = dir & ~LV_DIR_BOTTOM;
    add_tile<Widget::Bme280PressureChart>({tv, col, row++, dir});
  };
  // SCD30
  Fn addScd30 = [this](lv_obj_t *tv, int16_t col, uint8_t dir) {
    auto row = 0;
    dir = dir | LV_DIR_BOTTOM;
    add_tile<Widget::Scd30TemperatureChart>({tv, col, row++, dir});
    dir = dir | LV_DIR_TOP;
    add_tile<Widget::Scd30RelativeHumidityChart>({tv, col, row++, dir});
    dir = dir & ~LV_DIR_BOTTOM;
    add_tile<Widget::Scd30Co2Chart>({tv, col, row++, dir});
  };
  // SCD41
  Fn addScd41 = [this](lv_obj_t *tv, int16_t col, uint8_t dir) {
    auto row = 0;
    dir = dir | LV_DIR_BOTTOM;
    add_tile<Widget::Scd41TemperatureChart>({tv, col, row++, dir});
    dir = dir | LV_DIR_TOP;
    add_tile<Widget::Scd41RelativeHumidityChart>({tv, col, row++, dir});
    dir = dir & ~LV_DIR_BOTTOM;
    add_tile<Widget::Scd41Co2Chart>({tv, col, row++, dir});
  };
  // SGP30
  Fn addSgp30 = [this](lv_obj_t *tv, int16_t col, uint8_t dir) {
    auto row = 0;
    dir = dir | LV_DIR_BOTTOM;
    add_tile<Widget::Sgp30Eco2Chart>({tv, col, row++, dir});
    dir = dir | LV_DIR_TOP;
    //
    dir = dir & ~LV_DIR_BOTTOM;
    add_tile<Widget::Sgp30TotalVocChart>({tv, col, row++, dir});
  };
  //
  // ここでタイルを用意する
  //
  std::vector<Fn> functions;
  // TileWidget::BootMessageの次からこの並び順
  //  functions.push_back(addClock);
  functions.push_back(addSystemHealth);
  functions.push_back(addSummary);
  if (Application::historiesM5Env3) {
    functions.push_back(addM5Env3);
  }
  if (Application::historiesBme280) {
    functions.push_back(addBme280);
  }
  if (Application::historiesScd30) {
    functions.push_back(addScd30);
  }
  if (Application::historiesScd41) {
    functions.push_back(addScd41);
  }
  if (Application::historiesSgp30) {
    functions.push_back(addSgp30);
  }
  //
  auto col = 1;
  for (auto it = functions.begin(); it != functions.end(); ++it) {
    Fn &f = *it;
    uint8_t dir = LV_DIR_HOR;
    if (std::next(it) == functions.end()) {
      // 最後のタイルだけ右移動を禁止する
      dir = dir & ~LV_DIR_RIGHT;
    }
    f(tileview, col++, dir);
  }
  //
  home();
}

//
void Gui::home() noexcept {
  vibrate();
  if (periodic_timer) {
    lv_timer_reset(periodic_timer);
  }
  constexpr auto COL_ID = 2;
  constexpr auto ROW_ID = 0;
  lv_obj_set_tile_id(tileview, COL_ID, ROW_ID, LV_ANIM_OFF);
}

//
void Gui::movePrev() noexcept {
  vibrate();
  auto itr = std::find_if(tiles.begin(), tiles.end(), check_if_active_tile);
  if (itr == tiles.end() || itr == tiles.begin()) {
    return;
  }
  if (auto p = std::prev(itr)->get(); p) {
    if (periodic_timer) {
      lv_timer_reset(periodic_timer);
    }
    p->setActiveTile(tileview);
  }
}

//
void Gui::moveNext() noexcept {
  vibrate();
  auto itr = std::find_if(tiles.begin(), tiles.end(), check_if_active_tile);
  if (itr == tiles.end() || std::next(itr) == tiles.end()) {
    return;
  }
  if (auto p = std::next(itr)->get(); p) {
    if (periodic_timer) {
      lv_timer_reset(periodic_timer);
    }
    p->setActiveTile(tileview);
  }
}

//
void Gui::vibrate() noexcept {
  constexpr auto MILLISECONDS = 100;
  lv_timer_t *timer = lv_timer_create(
      [](lv_timer_t *) noexcept -> void { M5.Power.setVibration(0); },
      MILLISECONDS, nullptr);
  lv_timer_set_repeat_count(timer, 1); // one-shot timer
  M5.Power.setVibration(255);          // start vibrate
}

//
//
//
Widget::BootMessage::BootMessage(Widget::InitArg init) noexcept {
  tile = std::apply(lv_tileview_add_tile, init);
  message_label = lv_label_create(tile);
  lv_label_set_long_mode(message_label, LV_LABEL_LONG_WRAP);
  lv_label_set_text_static(message_label, Application::boot_log.c_str());
}

Widget::BootMessage::~BootMessage() { lv_obj_del(tile); }

void Widget::BootMessage::valueChangedEventHook(lv_event_t *) noexcept {
  lv_label_set_text_static(message_label, Application::boot_log.c_str());
}

void Widget::BootMessage::timerHook() noexcept {
  if (auto x = Application::boot_log.size(); x != count) {
    Gui::getInstance()->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
    count = x;
  }
}

//
//
//
Widget::SystemHealth::SystemHealth(Widget::InitArg init) noexcept {
  tile = std::apply(lv_tileview_add_tile, init);
  lv_style_init(&style_label);
  lv_style_set_text_font(&style_label, &lv_font_montserrat_16);
  //
  cont_col = lv_obj_create(tile);
  lv_obj_set_size(cont_col, lv_obj_get_content_width(tile),
                  lv_obj_get_content_height(tile));
  lv_obj_align(cont_col, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_flex_flow(cont_col, LV_FLEX_FLOW_COLUMN);
  //
  auto create = [&](lv_style_t *style,
                    lv_text_align_t align = LV_TEXT_ALIGN_LEFT) -> lv_obj_t * {
    lv_obj_t *label = lv_label_create(cont_col);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(label, align, 0);
    lv_obj_add_style(label, style, 0);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    return label;
  };
  //
  time_label = create(&style_label);
  status_label = create(&style_label);
  battery_label = create(&style_label);
  battery_charging_label = create(&style_label);
  available_heap_label = create(&style_label);
  available_internal_heap_label = create(&style_label);
  minimum_free_heap_label = create(&style_label);
  //
  render();
}

Widget::SystemHealth::~SystemHealth() { lv_obj_del(tile); }

void Widget::SystemHealth::valueChangedEventHook(lv_event_t *) noexcept {
  render();
}

void Widget::SystemHealth::timerHook() noexcept { render(); }

void Widget::SystemHealth::render() noexcept {
  const seconds uptime = Time::uptime();
  const int16_t sec = uptime.count() % 60;
  const int16_t min = uptime.count() / 60 % 60;
  const int16_t hour = uptime.count() / (60 * 60) % 24;
  const int32_t days = uptime.count() / (60 * 60 * 24);
  //
  { // now time
    std::ostringstream oss;
    const auto now = system_clock::now();
    std::time_t time =
        (Time::sync_completed()) ? system_clock::to_time_t(now) : 0;
    {
      std::tm utc_time;
      gmtime_r(&time, &utc_time);
      oss << std::put_time(&utc_time, "%F %T UTC");
    }
    oss << std::endl;
    {
      std::tm local_time;
      localtime_r(&time, &local_time);
      oss << std::put_time(&local_time, "%F %T %Z");
    }
    oss << std::endl;
    if (Time::sync_completed()) {
      oss << "Sync completed, based on SNTP";
    } else {
      oss << "Time is not synced";
    }
    lv_label_set_text(time_label, oss.str().c_str());
  }
  { // status
    std::ostringstream oss;
    if (WiFi.status() == WL_CONNECTED) {
      oss << "WiFi connected.IP : ";
      oss << WiFi.localIP().toString().c_str();
    } else {
      oss << "No WiFi connection.";
    }
    oss << std::endl                     //
        << std::setfill(' ')             //
        << "uptime " << days << " days," //
        << std::setfill('0')             //
        << std::setw(2) << hour << ":"   //
        << std::setw(2) << min << ":"    //
        << std::setw(2) << sec;          //
    lv_label_set_text(status_label, oss.str().c_str());
  }
  { // battery status
    auto battery_level = M5.Power.getBatteryLevel();
    auto battery_current = M5.Power.getBatteryCurrent();
    auto battery_voltage = M5.Power.getBatteryVoltage();
    std::ostringstream oss;
    oss << "Battery "                                                  //
        << std::setfill(' ') << std::setw(3) << +battery_level << "% " //
        << std::setfill(' ') << std::fixed                             //
        << std::setw(5) << std::setprecision(3)                        //
        << battery_voltage << "mV "                                    //
        << std::setw(5) << std::setprecision(3)                        //
        << battery_current << "mA ";                                   //
    lv_label_set_text(battery_label, oss.str().c_str());
  }
  { // battery charging
    std::ostringstream oss;
    switch (M5.Power.isCharging()) {
    case M5.Power.is_charging_t::is_discharging: {
      oss << "DISCHARGING"sv;
    } break;
    case M5.Power.is_charging_t::is_charging: {
      oss << "CHARGING"sv;
    } break;
    default: {
    } break;
    }
    lv_label_set_text(battery_charging_label, oss.str().c_str());
  }
  { // available heap memory
    uint32_t memfree = esp_get_free_heap_size();
    std::ostringstream oss;
    oss << "available heap size: " << memfree;
    lv_label_set_text(available_heap_label, oss.str().c_str());
  }
  { // available internal heap memory
    uint32_t memfree = esp_get_free_internal_heap_size();
    std::ostringstream oss;
    oss << "available internal heap size: " << memfree;
    lv_label_set_text(available_internal_heap_label, oss.str().c_str());
  }
  { // minumum free heap memory
    uint32_t memfree = esp_get_minimum_free_heap_size();
    std::ostringstream oss;
    oss << "minimum free heap size: " << memfree;
    lv_label_set_text(minimum_free_heap_label, oss.str().c_str());
  }
}

//
//
//
Widget::Clock::Clock(Widget::InitArg init) noexcept {
  tile = std::apply(lv_tileview_add_tile, init);
  meter = lv_meter_create(tile);
  auto size =
      std::min(lv_obj_get_content_width(tile), lv_obj_get_content_height(tile));
  lv_obj_set_size(meter, size * 0.9, size * 0.9);
  lv_obj_center(meter);
  //
  sec_scale = lv_meter_add_scale(meter);
  lv_meter_set_scale_ticks(meter, sec_scale, 61, 1, 10,
                           lv_palette_main(LV_PALETTE_GREY));
  lv_meter_set_scale_range(meter, sec_scale, 0, 60, 360, 270);
  //
  min_scale = lv_meter_add_scale(meter);
  lv_meter_set_scale_ticks(meter, min_scale, 61, 1, 10,
                           lv_palette_main(LV_PALETTE_GREY));
  lv_meter_set_scale_range(meter, min_scale, 0, 60, 360, 270);
  //
  hour_scale = lv_meter_add_scale(meter);
  lv_meter_set_scale_ticks(meter, hour_scale, 12, 0, 0,
                           lv_palette_main(LV_PALETTE_GREY));
  lv_meter_set_scale_major_ticks(meter, hour_scale, 1, 2, 20, lv_color_black(),
                                 10);
  lv_meter_set_scale_range(meter, hour_scale, 1, 12, 330, 300);
  //
  hour_indic = lv_meter_add_needle_line(
      meter, min_scale, 9, lv_palette_darken(LV_PALETTE_INDIGO, 2), -50);
  min_indic = lv_meter_add_needle_line(
      meter, min_scale, 4, lv_palette_darken(LV_PALETTE_INDIGO, 2), -25);
  sec_indic = lv_meter_add_needle_line(meter, sec_scale, 2,
                                       lv_palette_main(LV_PALETTE_RED), -10);
  //
  timerHook();
}

Widget::Clock::~Clock() { lv_obj_del(tile); }

void Widget::Clock::valueChangedEventHook(lv_event_t *) noexcept {
  timerHook();
}

void Widget::Clock::timerHook() noexcept {
  if (Time::sync_completed()) {
    std::time_t now = system_clock::to_time_t(system_clock::now());
    std::tm local;
    localtime_r(&now, &local);
    render(local);
  } else {
    std::time_t now = 0;
    std::tm utc;
    gmtime_r(&now, &utc);
    render(utc);
  }
}
void Widget::Clock::render(const std::tm &tm) noexcept {
  lv_meter_set_indicator_value(meter, sec_indic, tm.tm_sec);
  lv_meter_set_indicator_value(meter, min_indic, tm.tm_min);
  const float h = (60.0f / 12.0f) * (tm.tm_hour % 12);
  const float m = (60.0f / 12.0f) * tm.tm_min / 60.0f;
  lv_meter_set_indicator_value(meter, hour_indic, std::floor(h + m));
}

//
//
//
Widget::Summary::Summary(Widget::InitArg init) noexcept {
  tile = std::apply(lv_tileview_add_tile, init);
  table = lv_table_create(tile);
  //
  latest = Measurements{};
  //
  render();
}

Widget::Summary::~Summary() { lv_obj_del(tile); }

void Widget::Summary::valueChangedEventHook(lv_event_t *) noexcept { render(); }

void Widget::Summary::timerHook() noexcept {
  auto bme = Application::historiesBme280
                 ? Application::historiesBme280->getLatestValue()
                 : std::nullopt;
  auto sgp = Application::historiesSgp30
                 ? Application::historiesSgp30->getLatestValue()
                 : std::nullopt;
  auto scd = Application::historiesScd30
                 ? Application::historiesScd30->getLatestValue()
                 : std::nullopt;
  auto m5env3 = Application::historiesM5Env3
                    ? Application::historiesM5Env3->getLatestValue()
                    : std::nullopt;
  auto now = Measurements{std::move(bme), std::move(sgp), std::move(scd),
                          std::move(m5env3)};
  if (now != latest) {
    Gui::getInstance()->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
    latest = now;
  }
}

void Widget::Summary::render() noexcept {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  auto row = 0;
  // M5 unit ENV3
  if (Application::historiesM5Env3) {
    auto history = Application::historiesM5Env3->getLatestValue();
    {
      lv_table_set_cell_value(table, row, 0, "ENV3 Temp");
      if (history.has_value()) {
        const DegC t = history->second.temperature;
        oss.str("");
        oss << +t.count();
        lv_table_set_cell_value(table, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table, row, 1, "-");
      }
      lv_table_set_cell_value(table, row, 2, "C");
      row++;
    }
    {
      lv_table_set_cell_value(table, row, 0, "ENV3 Humi");
      if (history.has_value()) {
        const PctRH rh = history->second.relative_humidity;
        oss.str("");
        oss << +rh.count();
        lv_table_set_cell_value(table, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table, row, 1, "-");
      }
      lv_table_set_cell_value(table, row, 2, "%RH");
      row++;
    }
    {
      lv_table_set_cell_value(table, row, 0, "ENV3 Pres");
      if (history.has_value()) {
        const HectoPa hpa = history->second.pressure;
        oss.str("");
        oss << +hpa.count();
        lv_table_set_cell_value(table, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table, row, 1, "-");
      }
      lv_table_set_cell_value(table, row, 2, "hPa");
      row++;
    }
  }
  // BME280
  if (Application::historiesBme280) {
    auto history = Application::historiesBme280->getLatestValue();
    {
      lv_table_set_cell_value(table, row, 0, "BME280 Temp");
      if (history.has_value()) {
        const DegC t = history->second.temperature;
        oss.str("");
        oss << +t.count();
        lv_table_set_cell_value(table, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table, row, 1, "-");
      }
      lv_table_set_cell_value(table, row, 2, "C");
      row++;
    }
    {
      lv_table_set_cell_value(table, row, 0, "BME280 Humi");
      if (history.has_value()) {
        const PctRH rh = history->second.relative_humidity;
        oss.str("");
        oss << +rh.count();
        lv_table_set_cell_value(table, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table, row, 1, "-");
      }
      lv_table_set_cell_value(table, row, 2, "%RH");
      row++;
    }
    {
      lv_table_set_cell_value(table, row, 0, "BME280 Pres");
      if (history.has_value()) {
        const HectoPa hpa = history->second.pressure;
        oss.str("");
        oss << +hpa.count();
        lv_table_set_cell_value(table, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table, row, 1, "-");
      }
      lv_table_set_cell_value(table, row, 2, "hPa");
      row++;
    }
  }
  // SCD30
  if (Application::historiesScd30) {
    auto history = Application::historiesScd30->getLatestValue();
    {
      lv_table_set_cell_value(table, row, 0, "SCD30 Temp");
      if (history.has_value()) {
        const DegC t = history->second.temperature;
        oss.str("");
        oss << +t.count();
        lv_table_set_cell_value(table, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table, row, 1, "-");
      }
      lv_table_set_cell_value(table, row, 2, "C");
      row++;
    }
    {
      lv_table_set_cell_value(table, row, 0, "SCD30 Humi");
      if (history.has_value()) {
        const PctRH rh = history->second.relative_humidity;
        oss.str("");
        oss << +rh.count();
        lv_table_set_cell_value(table, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table, row, 1, "-");
      }
      lv_table_set_cell_value(table, row, 2, "%RH");
      row++;
    }
    {
      lv_table_set_cell_value(table, row, 0, "SCD30 CO2");
      if (history.has_value()) {
        const Ppm co2 = history->second.co2;
        oss.str("");
        oss << +co2.value;
        lv_table_set_cell_value(table, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table, row, 1, "-");
      }
      lv_table_set_cell_value(table, row, 2, "ppm");
      row++;
    }
  }
  // SCD41
  if (Application::historiesScd41) {
    auto history = Application::historiesScd41->getLatestValue();
    {
      lv_table_set_cell_value(table, row, 0, "SCD41 Temp");
      if (history.has_value()) {
        const DegC t = history->second.temperature;
        oss.str("");
        oss << +t.count();
        lv_table_set_cell_value(table, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table, row, 1, "-");
      }
      lv_table_set_cell_value(table, row, 2, "C");
      row++;
    }
    {
      lv_table_set_cell_value(table, row, 0, "SCD41 Humi");
      if (history.has_value()) {
        const PctRH rh = history->second.relative_humidity;
        oss.str("");
        oss << +rh.count();
        lv_table_set_cell_value(table, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table, row, 1, "-");
      }
      lv_table_set_cell_value(table, row, 2, "%RH");
      row++;
    }
    {
      lv_table_set_cell_value(table, row, 0, "SCD41 CO2");
      if (history.has_value()) {
        const Ppm co2 = history->second.co2;
        oss.str("");
        oss << +co2.value;
        lv_table_set_cell_value(table, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table, row, 1, "-");
      }
      lv_table_set_cell_value(table, row, 2, "ppm");
      row++;
    }
  }
  // SGP30
  if (Application::historiesSgp30) {
    auto history = Application::historiesSgp30->getLatestValue();
    {
      lv_table_set_cell_value(table, row, 0, "SGP30 eCO2");
      if (history.has_value()) {
        const Ppm eco2 = history->second.eCo2;
        oss.str("");
        oss << +eco2.value;
        lv_table_set_cell_value(table, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table, row, 1, "-");
      }
      lv_table_set_cell_value(table, row, 2, "ppm");
      row++;
    }
    {
      lv_table_set_cell_value(table, row, 0, "SGP30 TVOC");
      if (history.has_value()) {
        const Ppb tvoc = history->second.tvoc;
        oss.str("");
        oss << +tvoc.value;
        lv_table_set_cell_value(table, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table, row, 1, "-");
      }
      lv_table_set_cell_value(table, row, 2, "ppb");
      row++;
    }
  }
  //
  auto w = lv_obj_get_content_width(tile);
  lv_table_set_col_width(table, 0, w * 6 / 12);
  lv_table_set_col_width(table, 1, w * 3 / 12);
  lv_table_set_col_width(table, 2, w * 3 / 12);
  lv_obj_set_width(table, w);
  lv_obj_set_height(table, lv_obj_get_content_height(tile));
  lv_obj_center(table);
  lv_obj_set_style_text_font(table, &lv_font_montserrat_18,
                             LV_PART_ITEMS | LV_STATE_DEFAULT);
  //
  lv_obj_add_event_cb(
      table,
      [](lv_event_t *event) -> void {
        lv_obj_t *obj = lv_event_get_target(event);
        lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(event);
        if (dsc->part == LV_PART_ITEMS) {
          uint32_t row = dsc->id / lv_table_get_col_cnt(obj);
          uint32_t col = dsc->id - row * lv_table_get_col_cnt(obj);
          //
          switch (col) {
          case 0:
            dsc->label_dsc->align = LV_TEXT_ALIGN_LEFT;
            break;
          case 1:
            dsc->label_dsc->align = LV_TEXT_ALIGN_RIGHT;
            break;
          case 2:
            [[fallthrough]];
          default:
            dsc->label_dsc->align = LV_TEXT_ALIGN_LEFT;
            break;
          }
          if (row % 2 == 0) {
            dsc->rect_dsc->bg_color = lv_color_mix(
                lv_palette_main(LV_PALETTE_GREY), lv_color_white(), LV_OPA_30);
            dsc->rect_dsc->bg_opa = LV_OPA_COVER;
          } else {
            dsc->rect_dsc->bg_color = lv_color_white();
          }
        }
      },
      LV_EVENT_DRAW_PART_BEGIN, nullptr);
}

//
//
//
template <typename T>
Widget::BasicChart<T>::BasicChart(InitArg init,
                                  const std::string &inSubheading) noexcept {
  tile = std::apply(lv_tileview_add_tile, init);
  subheading = inSubheading;
  //
  container = lv_obj_create(tile);
  lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
  lv_obj_center(container);
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
  //
  title = lv_label_create(container);
  lv_obj_set_size(title, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_LEFT, LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_STATE_DEFAULT);
  lv_obj_center(title);
  lv_label_set_text(title, subheading.c_str());
  //
  label = lv_label_create(container);
  lv_obj_set_size(label, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_30, LV_STATE_DEFAULT);
  lv_obj_center(label);
  //
  chart = lv_chart_create(container);
  //
  // lv_obj_set_size(chart, 220, 140);
  lv_obj_update_layout(tile);
  lv_obj_set_size(chart, lv_obj_get_width(container) - 100, 140);
  lv_obj_center(chart);
  // Do not display points on the data
  //  lv_obj_set_style_size(chart, 0, LV_PART_INDICATOR);
  //
  //  lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y, 0, 1);
  //
  lv_chart_set_axis_tick(chart, LV_CHART_AXIS_SECONDARY_Y, 16, 9, 9, 2, true,
                         100);
  //
  ser_one = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED),
                                LV_CHART_AXIS_SECONDARY_Y);
}

template <typename T> Widget::BasicChart<T>::~BasicChart() noexcept {
  lv_obj_del(tile);
}

template <typename T>
template <typename U>
void Widget::BasicChart<T>::setHeading(
    std::string_view strUnit, std::optional<system_clock::time_point> optTP,
    std::optional<U> optValue) noexcept {
  lv_label_set_text(title, subheading.c_str());
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  if (optValue.has_value()) {
    oss << *optValue << " " << strUnit;
  } else {
    oss << "- " << strUnit;
  }
  // time
  if (optTP.has_value()) {
    std::time_t time = system_clock::to_time_t(*optTP);
    std::tm local_time;
    localtime_r(&time, &local_time);
    oss << std::put_time(&local_time, "(%R)");
  }
  lv_label_set_text(label, oss.str().c_str());
}

template <typename T>
void Widget::BasicChart<T>::setChartValue(
    const std::vector<ValuePair> &histories,
    std::function<lv_coord_t(const ValueT &)> mapping) noexcept {
  //
  if (histories.size() > 0) {
    auto min = std::numeric_limits<lv_coord_t>::max();
    auto max = std::numeric_limits<lv_coord_t>::min();
    lv_chart_set_point_count(chart, histories.size());
    for (auto idx = 0; idx < histories.size(); ++idx) {
      auto value = mapping(histories[idx].second);
      min = std::min(min, value);
      max = std::max(max, value);
      lv_chart_set_value_by_id(chart, ser_one, idx, value);
    }
    lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y, std::floor(min),
                       std::ceil(max));
    //
    lv_chart_refresh(chart);
  }
}

//
//
//
Widget::M5Env3TemperatureChart::M5Env3TemperatureChart(InitArg init) noexcept
    : BasicChart(init, "ENV.III unit Temperature") {
  lv_obj_add_event_cb(chart, drawEventHook, LV_EVENT_DRAW_PART_BEGIN, this);
}

void Widget::M5Env3TemperatureChart::valueChangedEventHook(
    lv_event_t *) noexcept {
  auto optTP =
      latest.has_value() ? std::make_optional(latest->first) : std::nullopt;
  auto optValue =
      latest.has_value()
          ? std::make_optional(DegC(latest->second.temperature).count())
          : std::nullopt;
  setHeading("C", optTP, optValue);
  //
  auto histories = Application::historiesM5Env3
                       ? Application::historiesM5Env3->getHistories()
                       : std::vector<MeasurementM5Env3>{};
  setChartValue(histories, map_to_coord_t);
}

void Widget::M5Env3TemperatureChart::timerHook() noexcept {
  auto now = Application::historiesM5Env3
                 ? Application::historiesM5Env3->getLatestValue()
                 : std::nullopt;
  if (now != latest) {
    latest = now;
    Gui::getInstance()->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
  }
}

lv_coord_t
Widget::M5Env3TemperatureChart::map_to_coord_t(const ValueT &in) noexcept {
  return in.temperature.count();
}

void Widget::M5Env3TemperatureChart::drawEventHook(lv_event_t *event) noexcept {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(event);
  if (lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                  LV_CHART_DRAW_PART_TICK_LABEL)) {
    if (dsc->id == LV_CHART_AXIS_SECONDARY_Y && dsc->text) {
      auto centiC = CentiDegC(dsc->value);
      lv_snprintf(dsc->text, dsc->text_length, "%.2f", DegC(centiC).count());
    }
  }
}

//
//
//
Widget::Bme280TemperatureChart::Bme280TemperatureChart(InitArg init) noexcept
    : BasicChart(init, "BME280 Temperature") {
  lv_obj_add_event_cb(chart, drawEventHook, LV_EVENT_DRAW_PART_BEGIN, this);
}

void Widget::Bme280TemperatureChart::valueChangedEventHook(
    lv_event_t *) noexcept {
  auto optTP =
      latest.has_value() ? std::make_optional(latest->first) : std::nullopt;
  auto optValue =
      latest.has_value()
          ? std::make_optional(DegC(latest->second.temperature).count())
          : std::nullopt;
  setHeading("C", optTP, optValue);
  //
  auto histories = Application::historiesBme280
                       ? Application::historiesBme280->getHistories()
                       : std::vector<MeasurementBme280>{};
  setChartValue(histories, map_to_coord_t);
}

void Widget::Bme280TemperatureChart::timerHook() noexcept {
  auto now = Application::historiesBme280
                 ? Application::historiesBme280->getLatestValue()
                 : std::nullopt;
  if (now != latest) {
    latest = now;
    Gui::getInstance()->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
  }
}

lv_coord_t
Widget::Bme280TemperatureChart::map_to_coord_t(const ValueT &in) noexcept {
  return in.temperature.count();
}

void Widget::Bme280TemperatureChart::drawEventHook(lv_event_t *event) noexcept {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(event);
  if (lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                  LV_CHART_DRAW_PART_TICK_LABEL)) {
    if (dsc->id == LV_CHART_AXIS_SECONDARY_Y && dsc->text) {
      auto centiC = CentiDegC(dsc->value);
      lv_snprintf(dsc->text, dsc->text_length, "%.2f", DegC(centiC).count());
    }
  }
}

//
//
//
Widget::Scd30TemperatureChart::Scd30TemperatureChart(InitArg init) noexcept
    : BasicChart(init, "SCD30 Temperature") {
  lv_obj_add_event_cb(chart, drawEventHook, LV_EVENT_DRAW_PART_BEGIN, this);
}

void Widget::Scd30TemperatureChart::valueChangedEventHook(
    lv_event_t *) noexcept {
  auto optTP =
      latest.has_value() ? std::make_optional(latest->first) : std::nullopt;
  auto optValue =
      latest.has_value()
          ? std::make_optional(DegC(latest->second.temperature).count())
          : std::nullopt;
  setHeading("C", optTP, optValue);
  //
  auto histories = Application::historiesScd30
                       ? Application::historiesScd30->getHistories()
                       : std::vector<MeasurementScd30>{};
  setChartValue(histories, map_to_coord_t);
}

void Widget::Scd30TemperatureChart::timerHook() noexcept {
  auto now = Application::historiesScd30
                 ? Application::historiesScd30->getLatestValue()
                 : std::nullopt;
  if (now != latest) {
    latest = now;
    Gui::getInstance()->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
  }
}

lv_coord_t
Widget::Scd30TemperatureChart::map_to_coord_t(const ValueT &in) noexcept {
  return in.temperature.count();
}

void Widget::Scd30TemperatureChart::drawEventHook(lv_event_t *event) noexcept {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(event);
  if (lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                  LV_CHART_DRAW_PART_TICK_LABEL)) {
    if (dsc->id == LV_CHART_AXIS_SECONDARY_Y && dsc->text) {
      auto centiC = CentiDegC(dsc->value);
      lv_snprintf(dsc->text, dsc->text_length, "%.2f", DegC(centiC).count());
    }
  }
}

//
//
//
Widget::Scd41TemperatureChart::Scd41TemperatureChart(InitArg init) noexcept
    : BasicChart(init, "SCD41 Temperature") {
  lv_obj_add_event_cb(chart, drawEventHook, LV_EVENT_DRAW_PART_BEGIN, this);
}

void Widget::Scd41TemperatureChart::valueChangedEventHook(
    lv_event_t *) noexcept {
  auto optTP =
      latest.has_value() ? std::make_optional(latest->first) : std::nullopt;
  auto optValue =
      latest.has_value()
          ? std::make_optional(DegC(latest->second.temperature).count())
          : std::nullopt;
  setHeading("C", optTP, optValue);
  //
  auto histories = Application::historiesScd41
                       ? Application::historiesScd41->getHistories()
                       : std::vector<MeasurementScd41>{};
  setChartValue(histories, map_to_coord_t);
}

void Widget::Scd41TemperatureChart::timerHook() noexcept {
  auto now = Application::historiesScd41
                 ? Application::historiesScd41->getLatestValue()
                 : std::nullopt;
  if (now != latest) {
    latest = now;
    Gui::getInstance()->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
  }
}

lv_coord_t
Widget::Scd41TemperatureChart::map_to_coord_t(const ValueT &in) noexcept {
  return in.temperature.count();
}

void Widget::Scd41TemperatureChart::drawEventHook(lv_event_t *event) noexcept {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(event);
  if (lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                  LV_CHART_DRAW_PART_TICK_LABEL)) {
    if (dsc->id == LV_CHART_AXIS_SECONDARY_Y && dsc->text) {
      auto centiC = CentiDegC(dsc->value);
      lv_snprintf(dsc->text, dsc->text_length, "%.2f", DegC(centiC).count());
    }
  }
}

//
//
//
Widget::M5Env3RelativeHumidityChart::M5Env3RelativeHumidityChart(
    InitArg init) noexcept
    : BasicChart(init, "ENV.III unit Relative Humidity") {
  lv_obj_add_event_cb(chart, drawEventHook, LV_EVENT_DRAW_PART_BEGIN, this);
}

void Widget::M5Env3RelativeHumidityChart::valueChangedEventHook(
    lv_event_t *) noexcept {
  auto optTP =
      latest.has_value() ? std::make_optional(latest->first) : std::nullopt;
  auto optValue =
      latest.has_value()
          ? std::make_optional(PctRH(latest->second.relative_humidity).count())
          : std::nullopt;
  setHeading("%RH", optTP, optValue);
  //
  auto histories = Application::historiesM5Env3
                       ? Application::historiesM5Env3->getHistories()
                       : std::vector<MeasurementM5Env3>{};
  setChartValue(histories, map_to_coord_t);
}

void Widget::M5Env3RelativeHumidityChart::timerHook() noexcept {
  auto now = Application::historiesM5Env3
                 ? Application::historiesM5Env3->getLatestValue()
                 : std::nullopt;
  if (now != latest) {
    latest = now;
    Gui::getInstance()->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
  }
}

lv_coord_t
Widget::M5Env3RelativeHumidityChart::map_to_coord_t(const ValueT &in) noexcept {
  return in.relative_humidity.count();
}

void Widget::M5Env3RelativeHumidityChart::drawEventHook(
    lv_event_t *event) noexcept {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(event);
  if (lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                  LV_CHART_DRAW_PART_TICK_LABEL)) {
    if (dsc->id == LV_CHART_AXIS_SECONDARY_Y && dsc->text) {
      auto centiRH = CentiRH(dsc->value);
      lv_snprintf(dsc->text, dsc->text_length, "%.2f", PctRH(centiRH).count());
    }
  }
}

//
//
//
Widget::Bme280RelativeHumidityChart::Bme280RelativeHumidityChart(
    InitArg init) noexcept
    : BasicChart(init, "BME280 Relative Humidity") {
  lv_obj_add_event_cb(chart, drawEventHook, LV_EVENT_DRAW_PART_BEGIN, this);
}

void Widget::Bme280RelativeHumidityChart::valueChangedEventHook(
    lv_event_t *) noexcept {
  auto optTP =
      latest.has_value() ? std::make_optional(latest->first) : std::nullopt;
  auto optValue =
      latest.has_value()
          ? std::make_optional(PctRH(latest->second.relative_humidity).count())
          : std::nullopt;
  setHeading("%RH", optTP, optValue);
  //
  auto histories = Application::historiesBme280
                       ? Application::historiesBme280->getHistories()
                       : std::vector<MeasurementBme280>{};
  setChartValue(histories, map_to_coord_t);
}

void Widget::Bme280RelativeHumidityChart::timerHook() noexcept {
  auto now = Application::historiesBme280
                 ? Application::historiesBme280->getLatestValue()
                 : std::nullopt;
  if (now != latest) {
    latest = now;
    Gui::getInstance()->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
  }
}

lv_coord_t
Widget::Bme280RelativeHumidityChart::map_to_coord_t(const ValueT &in) noexcept {
  return in.relative_humidity.count();
}

void Widget::Bme280RelativeHumidityChart::drawEventHook(
    lv_event_t *event) noexcept {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(event);
  if (lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                  LV_CHART_DRAW_PART_TICK_LABEL)) {
    if (dsc->id == LV_CHART_AXIS_SECONDARY_Y && dsc->text) {
      auto centiRH = CentiRH(dsc->value);
      lv_snprintf(dsc->text, dsc->text_length, "%.2f", PctRH(centiRH).count());
    }
  }
}

//
//
//
Widget::Scd30RelativeHumidityChart::Scd30RelativeHumidityChart(
    InitArg init) noexcept
    : BasicChart(init, "SCD30 Relative Humidity") {
  lv_obj_add_event_cb(chart, drawEventHook, LV_EVENT_DRAW_PART_BEGIN, this);
}

void Widget::Scd30RelativeHumidityChart::valueChangedEventHook(
    lv_event_t *) noexcept {
  auto optTP =
      latest.has_value() ? std::make_optional(latest->first) : std::nullopt;
  auto optValue =
      latest.has_value()
          ? std::make_optional(PctRH(latest->second.relative_humidity).count())
          : std::nullopt;
  setHeading("%RH", optTP, optValue);
  //
  auto histories = Application::historiesScd30
                       ? Application::historiesScd30->getHistories()
                       : std::vector<MeasurementScd30>{};
  setChartValue(histories, map_to_coord_t);
}

void Widget::Scd30RelativeHumidityChart::timerHook() noexcept {
  auto now = Application::historiesScd30
                 ? Application::historiesScd30->getLatestValue()
                 : std::nullopt;
  if (now != latest) {
    latest = now;
    Gui::getInstance()->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
  }
}

lv_coord_t
Widget::Scd30RelativeHumidityChart::map_to_coord_t(const ValueT &in) noexcept {
  return in.relative_humidity.count();
}

void Widget::Scd30RelativeHumidityChart::drawEventHook(
    lv_event_t *event) noexcept {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(event);
  if (lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                  LV_CHART_DRAW_PART_TICK_LABEL)) {
    if (dsc->id == LV_CHART_AXIS_SECONDARY_Y && dsc->text) {
      auto centiRH = CentiRH(dsc->value);
      lv_snprintf(dsc->text, dsc->text_length, "%.2f", PctRH(centiRH).count());
    }
  }
}

//
//
//
Widget::Scd41RelativeHumidityChart::Scd41RelativeHumidityChart(
    InitArg init) noexcept
    : BasicChart(init, "SCD41 Relative Humidity") {
  lv_obj_add_event_cb(chart, drawEventHook, LV_EVENT_DRAW_PART_BEGIN, this);
}

void Widget::Scd41RelativeHumidityChart::valueChangedEventHook(
    lv_event_t *) noexcept {
  auto optTP =
      latest.has_value() ? std::make_optional(latest->first) : std::nullopt;
  auto optValue =
      latest.has_value()
          ? std::make_optional(PctRH(latest->second.relative_humidity).count())
          : std::nullopt;
  setHeading("%RH", optTP, optValue);
  //
  auto histories = Application::historiesScd41
                       ? Application::historiesScd41->getHistories()
                       : std::vector<MeasurementScd41>{};
  setChartValue(histories, map_to_coord_t);
}

void Widget::Scd41RelativeHumidityChart::timerHook() noexcept {
  auto now = Application::historiesScd41
                 ? Application::historiesScd41->getLatestValue()
                 : std::nullopt;
  if (now != latest) {
    latest = now;
    Gui::getInstance()->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
  }
}

lv_coord_t
Widget::Scd41RelativeHumidityChart::map_to_coord_t(const ValueT &in) noexcept {
  return in.relative_humidity.count();
}

void Widget::Scd41RelativeHumidityChart::drawEventHook(
    lv_event_t *event) noexcept {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(event);
  if (lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                  LV_CHART_DRAW_PART_TICK_LABEL)) {
    if (dsc->id == LV_CHART_AXIS_SECONDARY_Y && dsc->text) {
      auto centiRH = CentiRH(dsc->value);
      lv_snprintf(dsc->text, dsc->text_length, "%.2f", PctRH(centiRH).count());
    }
  }
}

//
//
//
Widget::M5Env3PressureChart::M5Env3PressureChart(InitArg init) noexcept
    : BasicChart(init, "ENV.III unit Pressure") {
  lv_obj_add_event_cb(chart, drawEventHook, LV_EVENT_DRAW_PART_BEGIN, this);
}

void Widget::M5Env3PressureChart::valueChangedEventHook(lv_event_t *) noexcept {
  auto optTP =
      latest.has_value() ? std::make_optional(latest->first) : std::nullopt;
  auto optValue =
      latest.has_value()
          ? std::make_optional(HectoPa(latest->second.pressure).count())
          : std::nullopt;
  setHeading("hPa", optTP, optValue);
  //
  auto histories = Application::historiesM5Env3
                       ? Application::historiesM5Env3->getHistories()
                       : std::vector<MeasurementM5Env3>{};
  setChartValue(histories, map_to_coord_t);
}

void Widget::M5Env3PressureChart::timerHook() noexcept {
  auto now = Application::historiesM5Env3
                 ? Application::historiesM5Env3->getLatestValue()
                 : std::nullopt;
  if (now != latest) {
    latest = now;
    Gui::getInstance()->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
  }
}

lv_coord_t
Widget::M5Env3PressureChart::map_to_coord_t(const ValueT &in) noexcept {
  return (in.pressure - BIAS).count();
}

void Widget::M5Env3PressureChart::drawEventHook(lv_event_t *event) noexcept {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(event);
  if (lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                  LV_CHART_DRAW_PART_TICK_LABEL)) {
    if (dsc->id == LV_CHART_AXIS_SECONDARY_Y && dsc->text) {
      auto pascal = Pascal(dsc->value) + BIAS;
      lv_snprintf(dsc->text, dsc->text_length, "%.2f", HectoPa(pascal).count());
    }
  }
}

//
//
//
Widget::Bme280PressureChart::Bme280PressureChart(InitArg init) noexcept
    : BasicChart(init, "BME280 Pressure") {
  lv_obj_add_event_cb(chart, drawEventHook, LV_EVENT_DRAW_PART_BEGIN, this);
}

void Widget::Bme280PressureChart::valueChangedEventHook(lv_event_t *) noexcept {
  auto optTP =
      latest.has_value() ? std::make_optional(latest->first) : std::nullopt;
  auto optValue =
      latest.has_value()
          ? std::make_optional(HectoPa(latest->second.pressure).count())
          : std::nullopt;
  setHeading("hPa", optTP, optValue);
  //
  auto histories = Application::historiesBme280
                       ? Application::historiesBme280->getHistories()
                       : std::vector<MeasurementBme280>{};
  setChartValue(histories, map_to_coord_t);
}

void Widget::Bme280PressureChart::timerHook() noexcept {
  auto now = Application::historiesBme280
                 ? Application::historiesBme280->getLatestValue()
                 : std::nullopt;
  if (now != latest) {
    latest = now;
    Gui::getInstance()->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
  }
}

lv_coord_t
Widget::Bme280PressureChart::map_to_coord_t(const ValueT &in) noexcept {
  return (in.pressure - BIAS).count();
}

void Widget::Bme280PressureChart::drawEventHook(lv_event_t *event) noexcept {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(event);
  if (lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                  LV_CHART_DRAW_PART_TICK_LABEL)) {
    if (dsc->id == LV_CHART_AXIS_SECONDARY_Y && dsc->text) {
      auto pascal = Pascal(dsc->value) + BIAS;
      lv_snprintf(dsc->text, dsc->text_length, "%.2f", HectoPa(pascal).count());
    }
  }
}

//
//
//
Widget::Sgp30TotalVocChart::Sgp30TotalVocChart(InitArg init) noexcept
    : BasicChart(init, "SGP30 Total VOC") {
  lv_obj_add_event_cb(chart, drawEventHook, LV_EVENT_DRAW_PART_BEGIN, this);
}

void Widget::Sgp30TotalVocChart::valueChangedEventHook(lv_event_t *) noexcept {
  auto optTP =
      latest.has_value() ? std::make_optional(latest->first) : std::nullopt;
  auto optValue = latest.has_value()
                      ? std::make_optional(Ppb(latest->second.tvoc).value)
                      : std::nullopt;
  setHeading("ppb", optTP, optValue);
  //
  auto histories = Application::historiesSgp30
                       ? Application::historiesSgp30->getHistories()
                       : std::vector<MeasurementSgp30>{};
  setChartValue(histories, map_to_coord_t);
}

void Widget::Sgp30TotalVocChart::timerHook() noexcept {
  auto now = Application::historiesSgp30
                 ? Application::historiesSgp30->getLatestValue()
                 : std::nullopt;
  if (now != latest) {
    latest = now;
    Gui::getInstance()->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
  }
}

lv_coord_t
Widget::Sgp30TotalVocChart::map_to_coord_t(const ValueT &in) noexcept {
  return Ppb(in.tvoc).value / 2;
}

void Widget::Sgp30TotalVocChart::drawEventHook(lv_event_t *event) noexcept {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(event);
  if (lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                  LV_CHART_DRAW_PART_TICK_LABEL)) {
    if (dsc->id == LV_CHART_AXIS_SECONDARY_Y && dsc->text) {
      auto tvoc = dsc->value * 2;
      lv_snprintf(dsc->text, dsc->text_length, "%d", tvoc);
    }
  }
}

//
//
//
Widget::Sgp30Eco2Chart::Sgp30Eco2Chart(InitArg init) noexcept
    : BasicChart(init, "SGP30 equivalent CO2") {
  lv_obj_add_event_cb(chart, drawEventHook, LV_EVENT_DRAW_PART_BEGIN, this);
}

void Widget::Sgp30Eco2Chart::valueChangedEventHook(lv_event_t *) noexcept {
  auto optTP =
      latest.has_value() ? std::make_optional(latest->first) : std::nullopt;
  auto optValue = latest.has_value()
                      ? std::make_optional(Ppm(latest->second.eCo2).value)
                      : std::nullopt;
  setHeading("ppm", optTP, optValue);
  //
  auto histories = Application::historiesSgp30
                       ? Application::historiesSgp30->getHistories()
                       : std::vector<MeasurementSgp30>{};
  setChartValue(histories, map_to_coord_t);
}

void Widget::Sgp30Eco2Chart::timerHook() noexcept {
  auto now = Application::historiesSgp30
                 ? Application::historiesSgp30->getLatestValue()
                 : std::nullopt;
  if (now != latest) {
    latest = now;
    Gui::getInstance()->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
  }
}

lv_coord_t Widget::Sgp30Eco2Chart::map_to_coord_t(const ValueT &in) noexcept {
  return Ppm(in.eCo2).value / 2;
}

void Widget::Sgp30Eco2Chart::drawEventHook(lv_event_t *event) noexcept {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(event);
  if (lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                  LV_CHART_DRAW_PART_TICK_LABEL)) {
    if (dsc->id == LV_CHART_AXIS_SECONDARY_Y && dsc->text) {
      auto eco2 = dsc->value * 2;
      lv_snprintf(dsc->text, dsc->text_length, "%d", eco2);
    }
  }
}

//
//
//
Widget::Scd30Co2Chart::Scd30Co2Chart(InitArg init) noexcept
    : BasicChart(init, "SCD30 CO2") {}

void Widget::Scd30Co2Chart::valueChangedEventHook(lv_event_t *) noexcept {
  auto optTP =
      latest.has_value() ? std::make_optional(latest->first) : std::nullopt;
  auto optValue = latest.has_value()
                      ? std::make_optional(Ppm(latest->second.co2).value)
                      : std::nullopt;
  setHeading("ppm", optTP, optValue);
  //
  auto histories = Application::historiesScd30
                       ? Application::historiesScd30->getHistories()
                       : std::vector<MeasurementScd30>{};
  setChartValue(histories, [](auto x) { return Ppm(x.co2).value; });
}

void Widget::Scd30Co2Chart::timerHook() noexcept {
  auto now = Application::historiesScd30
                 ? Application::historiesScd30->getLatestValue()
                 : std::nullopt;
  if (now != latest) {
    latest = now;
    Gui::getInstance()->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
  }
}

//
//
//
Widget::Scd41Co2Chart::Scd41Co2Chart(InitArg init) noexcept
    : BasicChart(init, "SCD41 CO2") {}

void Widget::Scd41Co2Chart::valueChangedEventHook(lv_event_t *) noexcept {
  auto optTP =
      latest.has_value() ? std::make_optional(latest->first) : std::nullopt;
  auto optValue = latest.has_value()
                      ? std::make_optional(Ppm(latest->second.co2).value)
                      : std::nullopt;
  setHeading("ppm", optTP, optValue);
  //
  auto histories = Application::historiesScd41
                       ? Application::historiesScd41->getHistories()
                       : std::vector<MeasurementScd41>{};
  setChartValue(histories, [](auto x) { return Ppm(x.co2).value; });
}

void Widget::Scd41Co2Chart::timerHook() noexcept {
  auto now = Application::historiesScd41
                 ? Application::historiesScd41->getLatestValue()
                 : std::nullopt;
  if (now != latest) {
    latest = now;
    Gui::getInstance()->send_event_to_tileview(LV_EVENT_VALUE_CHANGED, nullptr);
  }
}
