// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#include "Gui.hpp"
#include "Application.hpp"
#include "Database.hpp"
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
#include <unordered_map>

#include <M5Unified.h>

using namespace std::chrono;
using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

//
lv_color_t Gui::LvglUseArea::draw_buf_1[];
lv_color_t Gui::LvglUseArea::draw_buf_2[];

//
void Gui::lvgl_use_display_flush_callback(lv_disp_drv_t *disp_drv,
                                          const lv_area_t *area,
                                          lv_color_t *color_p) {
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
void Gui::lvgl_use_touchpad_read_callback(lv_indev_drv_t *indev_drv,
                                          lv_indev_data_t *data) {
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
bool Gui::begin() {
  // LVGL init
  lv_init();
  // LVGL draw buffer
  lv_disp_draw_buf_init(&lvgl_use.draw_buf_dsc, lvgl_use.draw_buf_1,
                        lvgl_use.draw_buf_2, std::size(lvgl_use.draw_buf_1));

  // LVGL display driver
  lv_disp_drv_init(&lvgl_use.disp_drv);
  lvgl_use.disp_drv.user_data = &gfx;
  lvgl_use.disp_drv.hor_res = gfx.width();
  lvgl_use.disp_drv.ver_res = gfx.height();
  lvgl_use.disp_drv.flush_cb = lvgl_use_display_flush_callback;
  lvgl_use.disp_drv.draw_buf = &lvgl_use.draw_buf_dsc;
  // register the display driver
  lv_disp_drv_register(&lvgl_use.disp_drv);

  // LVGL (touchpad) input device driver
  lv_indev_drv_init(&lvgl_use.indev_drv);
  lvgl_use.indev_drv.user_data = &gfx;
  lvgl_use.indev_drv.type = LV_INDEV_TYPE_POINTER;
  lvgl_use.indev_drv.read_cb = lvgl_use_touchpad_read_callback;
  // register the input device driver
  lv_indev_drv_register(&lvgl_use.indev_drv);

  // 起動画面
  _startup_widget =
      std::make_unique<Widget::Startup>(gfx.width(), gfx.height());
  //
  return true;
}

//
bool Gui::startUi() {
  //
  if (tileview_obj = lv_tileview_create(nullptr); tileview_obj == nullptr) {
    M5_LOGE("memory allocation error");
    return false;
  }
  //
  auto col = 0;
  // make the first tile
  add_tile<Widget::BootMessage>({tileview_obj, col++, 0, LV_DIR_RIGHT});
  // TileWidget::BootMessageの次からこの並び順
  if constexpr (false) {
    add_tile<Widget::Clock>({tileview_obj, col++, 0, LV_DIR_HOR});
  }
  add_tile<Widget::SystemHealthy>({tileview_obj, col++, 0, LV_DIR_HOR});
  add_tile<Widget::Summary>({tileview_obj, col++, 0, LV_DIR_HOR});
  add_tile<Widget::TemperatureChart>({tileview_obj, col++, 0, LV_DIR_HOR});
  add_tile<Widget::RelativeHumidityChart>({tileview_obj, col++, 0, LV_DIR_HOR});
  add_tile<Widget::PressureChart>({tileview_obj, col++, 0, LV_DIR_HOR});
  add_tile<Widget::CarbonDeoxidesChart>({tileview_obj, col++, 0, LV_DIR_HOR});
  // 最後のタイルだけ右移動を禁止する
  add_tile<Widget::TotalVocChart>({tileview_obj, col++, 0, LV_DIR_LEFT});
  //
  if (_startup_widget) {
    _startup_widget.reset();
  }
  //
  lv_scr_load(tileview_obj);
  //
  home();
  return true;
}

//
void Gui::home() {
  if (tileview_obj == nullptr) {
    M5_LOGE("tileview had null");
    return;
  }
  vibrate();
  constexpr auto COL_ID = 2;
  constexpr auto ROW_ID = 0;
  lv_obj_set_tile_id(tileview_obj, COL_ID, ROW_ID, LV_ANIM_OFF);
}

//
void Gui::movePrev() {
  vibrate();
  if (auto found = std::find_if(tile_vector.begin(), tile_vector.end(),
                                check_if_active_tile);
      found != tile_vector.end()) {
    if (found != tile_vector.begin()) {
      std::vector<std::unique_ptr<Widget::TileBase>>::iterator itr =
          std::prev(found);
      itr->get()->setActiveTile();
    }
  }
}

//
void Gui::moveNext() {
  vibrate();
  if (auto found = std::find_if(tile_vector.begin(), tile_vector.end(),
                                check_if_active_tile);
      found != tile_vector.end()) {
    std::vector<std::unique_ptr<Widget::TileBase>>::iterator itr =
        std::next(found);
    if (itr != tile_vector.end()) {
      itr->get()->setActiveTile();
    }
  }
}

//
void Gui::vibrate() {
  constexpr auto MILLISECONDS = 100;
  auto stop_the_vibration = [](lv_timer_t *) -> void {
    M5.Power.setVibration(0);
  };
  if (lv_timer_t *timer =
          lv_timer_create(stop_the_vibration, MILLISECONDS, nullptr);
      timer) {
    lv_timer_set_repeat_count(timer, 1); // one-shot timer
    M5.Power.setVibration(255);          // start vibrate
  }
}

//
Widget::Startup::Startup(lv_coord_t display_width, lv_coord_t display_height) {
  // create
  constexpr auto MARGIN{24};
  //
  if (container_obj = lv_obj_create(lv_scr_act()); container_obj == nullptr) {
    return;
  }
  lv_obj_set_size(container_obj, display_width, display_height);
  lv_obj_align(container_obj, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_opa(container_obj, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(
      container_obj, lv_palette_lighten(LV_PALETTE_GREY, 2), LV_PART_MAIN);
  //
  {
    const lv_font_t *FONT{&lv_font_montserrat_30};
    if (title_obj = lv_label_create(container_obj); title_obj == nullptr) {
      return;
    }
    lv_obj_set_size(title_obj, display_width - MARGIN * 2, FONT->line_height);
    lv_obj_align(title_obj, LV_ALIGN_CENTER, 0, -1 * FONT->line_height);
    lv_obj_set_style_text_font(title_obj, FONT, LV_STATE_DEFAULT);
    lv_style_init(&title_style);
    lv_style_set_text_color(&title_style,
                            lv_palette_darken(LV_PALETTE_BROWN, 3));
    lv_style_set_text_align(&title_style, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_style(title_obj, &title_style, LV_PART_MAIN);
    lv_label_set_text(title_obj, "HELLO");
  }
  //
  {
    const lv_font_t *FONT{&lv_font_montserrat_16};
    if (label_obj = lv_label_create(container_obj); label_obj == nullptr) {
      return;
    }
    lv_obj_set_size(label_obj, display_width - MARGIN * 2, FONT->line_height);
    lv_obj_align(label_obj, LV_ALIGN_CENTER, 0, FONT->line_height);
    lv_label_set_long_mode(label_obj, LV_LABEL_LONG_SCROLL);
    lv_label_set_recolor(label_obj, true);
    lv_obj_set_style_text_font(label_obj, FONT, LV_STATE_DEFAULT);
    lv_style_init(&label_style);
    lv_style_set_text_color(&label_style,
                            lv_palette_darken(LV_PALETTE_BROWN, 3));
    lv_style_set_text_align(&label_style, LV_TEXT_ALIGN_CENTER);
    lv_obj_add_style(label_obj, &label_style, LV_PART_MAIN);
    lv_label_set_text(label_obj, "");
  }
  //
  if (bar_obj = lv_bar_create(container_obj); bar_obj == nullptr) {
    return;
  }
  lv_obj_set_width(bar_obj, display_width - MARGIN * 2);
  lv_obj_align_to(bar_obj, label_obj, LV_ALIGN_OUT_BOTTOM_MID, 0, MARGIN);
  updateProgress(0);
}

//
Widget::Startup::~Startup() { lv_obj_del(container_obj); }

//
void Widget::Startup::updateMessage(const std::string &s) {
  lv_label_set_text(label_obj, s.c_str());
}

//
void Widget::Startup::updateProgress(int16_t percent) {
  lv_bar_set_value(bar_obj, percent, LV_ANIM_ON);
}

//
//
//
Widget::BootMessage::BootMessage(Widget::InitArg init) : TileBase{init} {
  if (tileview_obj == nullptr) {
    M5_LOGE("tileview had null");
    return;
  }
  if (tile_obj == nullptr) {
    M5_LOGE("tile had null");
    return;
  }
  // create
  if (message_label_obj = lv_label_create(tile_obj); message_label_obj) {
    lv_label_set_long_mode(message_label_obj, LV_LABEL_LONG_WRAP);
  }
}

void Widget::BootMessage::update() {
  if (auto x = Application::getInstance()->getStartupLog(); x != latest) {
    latest = x;
    render();
  }
}

void Widget::BootMessage::render() {
  lv_label_set_text_static(message_label_obj, latest.c_str());
}

//
//
//
Widget::Summary::Summary(Widget::InitArg init) : TileBase{init} {
  if (tileview_obj == nullptr) {
    M5_LOGE("tileview had null");
    return;
  }
  if (tile_obj == nullptr) {
    M5_LOGE("tile had null");
    return;
  }
  // create
  table_obj = lv_table_create(tile_obj);
}

void Widget::Summary::update() {
  auto present = Measurements{
      Application::getMeasurementsDatabase().getLatestMeasurementBme280(),
      Application::getMeasurementsDatabase().getLatestMeasurementSgp30(),
      Application::getMeasurementsDatabase().getLatestMeasurementScd30(),
      Application::getMeasurementsDatabase().getLatestMeasurementScd41(),
      Application::getMeasurementsDatabase().getLatestMeasurementM5Env3()};
  if (present != latest) {
    render();
    latest = present;
  }
}

void Widget::Summary::render() {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(2);
  auto row = 0;
  for (const auto &p : Application::getSensors()) {
    if (p.get() == nullptr) {
      continue;
    }
    switch (p->getSensorDescriptor()) {
    case Application::SENSOR_DESCRIPTOR_M5ENV3: { // M5 unit ENV3
      auto m5env3 =
          Application::getMeasurementsDatabase().getLatestMeasurementM5Env3();
      lv_table_set_cell_value(table_obj, row, 0, "ENV3 Temp");
      if (auto temp = m5env3 ? std::make_optional(m5env3->second.temperature)
                             : std::nullopt;
          temp) {
        const DegC t = static_cast<DegC>(*temp);
        oss.str("");
        oss << +t.count();
        lv_table_set_cell_value(table_obj, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table_obj, row, 1, "-");
      }
      lv_table_set_cell_value(table_obj, row, 2, "C");
      row++;
      //
      lv_table_set_cell_value(table_obj, row, 0, "ENV3 Humi");
      if (auto humi = m5env3
                          ? std::make_optional(m5env3->second.relative_humidity)
                          : std::nullopt;
          humi) {
        const PctRH rh = *humi;
        oss.str("");
        oss << +rh.count();
        lv_table_set_cell_value(table_obj, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table_obj, row, 1, "-");
      }
      lv_table_set_cell_value(table_obj, row, 2, "%RH");
      row++;
      //
      lv_table_set_cell_value(table_obj, row, 0, "ENV3 Pres");
      if (auto pres = m5env3 ? std::make_optional(m5env3->second.pressure)
                             : std::nullopt;
          pres) {
        const HectoPa hpa = *pres;
        oss.str("");
        oss << +hpa.count();
        lv_table_set_cell_value(table_obj, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table_obj, row, 1, "-");
      }
      lv_table_set_cell_value(table_obj, row, 2, "hPa");
      row++;
    } break;
    case Application::SENSOR_DESCRIPTOR_BME280: { // BME280
      auto bme280 =
          Application::getMeasurementsDatabase().getLatestMeasurementBme280();
      lv_table_set_cell_value(table_obj, row, 0, "BME280 Temp");
      if (auto temp = bme280 ? std::make_optional(bme280->second.temperature)
                             : std::nullopt;
          temp) {
        const DegC t{*temp};
        oss.str("");
        oss << +t.count();
        lv_table_set_cell_value(table_obj, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table_obj, row, 1, "-");
      }
      lv_table_set_cell_value(table_obj, row, 2, "C");
      row++;
      //
      lv_table_set_cell_value(table_obj, row, 0, "BME280 Humi");
      if (auto humi = bme280
                          ? std::make_optional(bme280->second.relative_humidity)
                          : std::nullopt;
          humi) {
        const PctRH rh{*humi};
        oss.str("");
        oss << +rh.count();
        lv_table_set_cell_value(table_obj, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table_obj, row, 1, "-");
      }
      lv_table_set_cell_value(table_obj, row, 2, "%RH");
      row++;
      //
      lv_table_set_cell_value(table_obj, row, 0, "BME280 Pres");
      if (auto pres = bme280 ? std::make_optional(bme280->second.pressure)
                             : std::nullopt;
          pres) {
        const HectoPa hpa{*pres};
        oss.str("");
        oss << +hpa.count();
        lv_table_set_cell_value(table_obj, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table_obj, row, 1, "-");
      }
      lv_table_set_cell_value(table_obj, row, 2, "hPa");
      row++;
    } break;
    case Application::SENSOR_DESCRIPTOR_SCD30: { // SCD30
      auto scd30 =
          Application::getMeasurementsDatabase().getLatestMeasurementScd30();
      lv_table_set_cell_value(table_obj, row, 0, "SCD30 Temp");
      if (auto temp = scd30 ? std::make_optional(scd30->second.temperature)
                            : std::nullopt;
          temp) {
        const DegC t{*temp};
        oss.str("");
        oss << +t.count();
        lv_table_set_cell_value(table_obj, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table_obj, row, 1, "-");
      }
      lv_table_set_cell_value(table_obj, row, 2, "C");
      row++;
      //
      lv_table_set_cell_value(table_obj, row, 0, "SCD30 Humi");
      if (auto humi = scd30
                          ? std::make_optional(scd30->second.relative_humidity)
                          : std::nullopt;
          humi) {
        const PctRH rh{*humi};
        oss.str("");
        oss << +rh.count();
        lv_table_set_cell_value(table_obj, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table_obj, row, 1, "-");
      }
      lv_table_set_cell_value(table_obj, row, 2, "%RH");
      row++;
      //
      lv_table_set_cell_value(table_obj, row, 0, "SCD30 CO2");
      if (auto carbon_dioxide =
              scd30 ? std::make_optional(scd30->second.co2) : std::nullopt;
          carbon_dioxide) {
        const Ppm co2{*carbon_dioxide};
        oss.str("");
        oss << +co2.value;
        lv_table_set_cell_value(table_obj, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table_obj, row, 1, "-");
      }
      lv_table_set_cell_value(table_obj, row, 2, "ppm");
      row++;
    } break;
    case Application::SENSOR_DESCRIPTOR_SCD41: { // SCD41
      auto scd41 =
          Application::getMeasurementsDatabase().getLatestMeasurementScd41();
      lv_table_set_cell_value(table_obj, row, 0, "SCD41 Temp");
      if (auto temp = scd41 ? std::make_optional(scd41->second.temperature)
                            : std::nullopt;
          temp) {
        const DegC t{*temp};
        oss.str("");
        oss << +t.count();
        lv_table_set_cell_value(table_obj, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table_obj, row, 1, "-");
      }
      lv_table_set_cell_value(table_obj, row, 2, "C");
      row++;
      //
      lv_table_set_cell_value(table_obj, row, 0, "SCD41 Humi");
      if (auto humi = scd41
                          ? std::make_optional(scd41->second.relative_humidity)
                          : std::nullopt;
          humi) {
        const PctRH rh{*humi};
        oss.str("");
        oss << +rh.count();
        lv_table_set_cell_value(table_obj, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table_obj, row, 1, "-");
      }
      lv_table_set_cell_value(table_obj, row, 2, "%RH");
      row++;
      //
      lv_table_set_cell_value(table_obj, row, 0, "SCD41 CO2");
      if (auto carbon_dioxide =
              scd41 ? std::make_optional(scd41->second.co2) : std::nullopt;
          carbon_dioxide) {
        const Ppm co2{*carbon_dioxide};
        oss.str("");
        oss << +co2.value;
        lv_table_set_cell_value(table_obj, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table_obj, row, 1, "-");
      }
      lv_table_set_cell_value(table_obj, row, 2, "ppm");
      row++;
    } break;
    case Application::SENSOR_DESCRIPTOR_SGP30: { // SGP30
      auto sgp30 =
          Application::getMeasurementsDatabase().getLatestMeasurementSgp30();
      lv_table_set_cell_value(table_obj, row, 0, "SGP30 eCO2");
      if (auto equivalent_carbon_dioxide =
              sgp30 ? std::make_optional(sgp30->second.eCo2) : std::nullopt;
          equivalent_carbon_dioxide) {
        const Ppm eco2{*equivalent_carbon_dioxide};
        oss.str("");
        oss << +eco2.value;
        lv_table_set_cell_value(table_obj, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table_obj, row, 1, "-");
      }
      lv_table_set_cell_value(table_obj, row, 2, "ppm");
      row++;
      //
      lv_table_set_cell_value(table_obj, row, 0, "SGP30 TVOC");
      if (auto total_voc =
              sgp30 ? std::make_optional(sgp30->second.tvoc) : std::nullopt;
          total_voc) {
        const Ppb tvoc{*total_voc};
        oss.str("");
        oss << +tvoc.value;
        lv_table_set_cell_value(table_obj, row, 1, oss.str().c_str());
      } else {
        lv_table_set_cell_value(table_obj, row, 1, "-");
      }
      lv_table_set_cell_value(table_obj, row, 2, "ppb");
      row++;
    } break;
    //
    default:
      break;
    }
  }
  //
  auto w = lv_obj_get_content_width(tile_obj);
  lv_table_set_col_width(table_obj, 0, w * 6 / 12);
  lv_table_set_col_width(table_obj, 1, w * 3 / 12);
  lv_table_set_col_width(table_obj, 2, w * 3 / 12);
  lv_obj_set_width(table_obj, w);
  lv_obj_set_height(table_obj, lv_obj_get_content_height(tile_obj));
  lv_obj_center(table_obj);
  lv_obj_set_style_text_font(table_obj, &lv_font_montserrat_16,
                             LV_PART_ITEMS | LV_STATE_DEFAULT);
  //
  lv_obj_add_event_cb(
      table_obj,
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
Widget::Clock::Clock(Widget::InitArg init) : TileBase{init} {
  if (tileview_obj == nullptr) {
    M5_LOGE("tileview had null");
    return;
  }
  if (tile_obj == nullptr) {
    M5_LOGE("tile had null");
    return;
  }
  // create
  if (meter_obj = lv_meter_create(tile_obj); meter_obj) {
    auto size = std::min(lv_obj_get_content_width(tile_obj),
                         lv_obj_get_content_height(tile_obj));
    lv_obj_set_size(meter_obj, size * 0.9, size * 0.9);
    lv_obj_center(meter_obj);
    //
    sec_scale = lv_meter_add_scale(meter_obj);
    lv_meter_set_scale_ticks(meter_obj, sec_scale, 61, 1, 10,
                             lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_range(meter_obj, sec_scale, 0, 60, 360, 270);
    //
    min_scale = lv_meter_add_scale(meter_obj);
    lv_meter_set_scale_ticks(meter_obj, min_scale, 61, 1, 10,
                             lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_range(meter_obj, min_scale, 0, 60, 360, 270);
    //
    hour_scale = lv_meter_add_scale(meter_obj);
    lv_meter_set_scale_ticks(meter_obj, hour_scale, 12, 0, 0,
                             lv_palette_main(LV_PALETTE_GREY));
    lv_meter_set_scale_major_ticks(meter_obj, hour_scale, 1, 2, 20,
                                   lv_color_black(), 10);
    lv_meter_set_scale_range(meter_obj, hour_scale, 1, 12, 330, 300);
    //
    hour_indic = lv_meter_add_needle_line(
        meter_obj, min_scale, 9, lv_palette_darken(LV_PALETTE_INDIGO, 2), -50);
    min_indic = lv_meter_add_needle_line(
        meter_obj, min_scale, 4, lv_palette_darken(LV_PALETTE_INDIGO, 2), -25);
    sec_indic = lv_meter_add_needle_line(meter_obj, sec_scale, 2,
                                         lv_palette_main(LV_PALETTE_RED), -10);
  }
}

void Widget::Clock::update() {
  if (Application::isTimeSynced()) {
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

void Widget::Clock::render(const std::tm &tm) {
  if (meter_obj) {
    lv_meter_set_indicator_value(meter_obj, sec_indic, tm.tm_sec);
    lv_meter_set_indicator_value(meter_obj, min_indic, tm.tm_min);
    const float h = (60.0f / 12.0f) * (tm.tm_hour % 12);
    const float m = (60.0f / 12.0f) * tm.tm_min / 60.0f;
    lv_meter_set_indicator_value(meter_obj, hour_indic, std::floor(h + m));
  }
}

//
//
//
Widget::SystemHealthy::SystemHealthy(Widget::InitArg init) : TileBase{init} {
  if (tileview_obj == nullptr) {
    M5_LOGE("tileview had null");
    return;
  }
  if (tile_obj == nullptr) {
    M5_LOGE("tile had null");
    return;
  }
  // create
  if (cont_col_obj = lv_obj_create(tile_obj); cont_col_obj) {
    lv_obj_set_size(cont_col_obj, LV_PCT(100), LV_PCT(100));
    lv_obj_align(cont_col_obj, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_flex_flow(cont_col_obj, LV_FLEX_FLOW_COLUMN);
    //
    auto create = [](lv_obj_t *parent, lv_style_t *style,
                     lv_text_align_t align = LV_TEXT_ALIGN_LEFT) -> lv_obj_t * {
      if (lv_obj_t *label = lv_label_create(parent); label) {
        lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(label, align, 0);
        lv_obj_add_style(label, style, 0);
        lv_obj_set_width(label, LV_PCT(100));
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        return label;
      } else {
        return nullptr;
      }
    };
    //
    lv_style_init(&label_style);
    lv_style_set_text_font(&label_style, &lv_font_montserrat_16);
    //
    if (time_label_obj = create(cont_col_obj, &label_style);
        time_label_obj == nullptr) {
      M5_LOGE("time_label_obj had null");
      return;
    }
    if (status_label_obj = create(cont_col_obj, &label_style);
        status_label_obj == nullptr) {
      M5_LOGE("status_label_obj had null");
      return;
    }
    if (battery_label_obj = create(cont_col_obj, &label_style);
        battery_label_obj == nullptr) {
      M5_LOGE("battery_label_obj had null");
      return;
    }
    if (battery_charging_label_obj = create(cont_col_obj, &label_style);
        battery_charging_label_obj == nullptr) {
      M5_LOGE("battery_charging_label_obj had null");
      return;
    }
    if (available_heap_label_obj = create(cont_col_obj, &label_style);
        available_heap_label_obj == nullptr) {
      M5_LOGE("available_heap_label_obj had null");
      return;
    }
    if (available_internal_heap_label_obj = create(cont_col_obj, &label_style);
        available_internal_heap_label_obj == nullptr) {
      M5_LOGE("available_heap_label_obj had null");
      return;
    }
    if (minimum_free_heap_label_obj = create(cont_col_obj, &label_style);
        minimum_free_heap_label_obj == nullptr) {
      M5_LOGE("minimum_free_heap_label_obj had null");
      return;
    }
    if (lvgl_task_stack_mark_label_obj = create(cont_col_obj, &label_style);
        lvgl_task_stack_mark_label_obj == nullptr) {
      M5_LOGE("lvgl_task_stack_mark_label_obj had null");
      return;
    }
    if (app_task_stack_mark_label_obj = create(cont_col_obj, &label_style);
        app_task_stack_mark_label_obj == nullptr) {
      M5_LOGE("app_task_stack_mark_label_obj had null");
      return;
    }
  }
}

//
void Widget::SystemHealthy::render() {
  if (time_label_obj == nullptr) {
    M5_LOGE("time_label_obj had null");
    return;
  }
  if (status_label_obj == nullptr) {
    M5_LOGE("status_label_obj had null");
    return;
  }
  if (battery_label_obj == nullptr) {
    M5_LOGE("battery_label_obj had null");
    return;
  }
  if (battery_charging_label_obj == nullptr) {
    M5_LOGE("battery_charging_label_obj had null");
    return;
  }
  if (available_heap_label_obj == nullptr) {
    M5_LOGE("available_heap_label_obj had null");
    return;
  }
  if (available_internal_heap_label_obj == nullptr) {
    M5_LOGE("available_heap_label_obj had null");
    return;
  }
  if (minimum_free_heap_label_obj == nullptr) {
    M5_LOGE("minimum_free_heap_label_obj had null");
    return;
  }
  if (lvgl_task_stack_mark_label_obj == nullptr) {
    M5_LOGE("lvgl_stack_mark_label_obj had null");
    return;
  }
  if (app_task_stack_mark_label_obj == nullptr) {
    M5_LOGE("app_stack_mark_label_obj had null");
    return;
  }
  const seconds uptime = Application::uptime();
  const int16_t sec = uptime.count() % 60;
  const int16_t min = uptime.count() / 60 % 60;
  const int16_t hour = uptime.count() / (60 * 60) % 24;
  const int32_t days = uptime.count() / (60 * 60 * 24);
  //
  { // now time
    std::ostringstream oss;
    const auto now = system_clock::now();
    std::time_t time =
        (Application::isTimeSynced()) ? system_clock::to_time_t(now) : 0;
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
    if (Application::isTimeSynced()) {
      oss << "Sync completed, based on SNTP";
    } else {
      oss << "Time is not synced";
    }
    lv_label_set_text(time_label_obj, oss.str().c_str());
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
    lv_label_set_text(status_label_obj, oss.str().c_str());
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
    lv_label_set_text(battery_label_obj, oss.str().c_str());
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
    lv_label_set_text(battery_charging_label_obj, oss.str().c_str());
  }
  { // available heap memory
    uint32_t memfree = esp_get_free_heap_size();
    std::ostringstream oss;
    oss << "available heap size: " << memfree;
    lv_label_set_text(available_heap_label_obj, oss.str().c_str());
  }
  { // available internal heap memory
    uint32_t memfree = esp_get_free_internal_heap_size();
    std::ostringstream oss;
    oss << "available internal heap size: " << memfree;
    lv_label_set_text(available_internal_heap_label_obj, oss.str().c_str());
  }
  { // minumum free heap memory
    uint32_t memfree = esp_get_minimum_free_heap_size();
    std::ostringstream oss;
    oss << "minimum free heap size: " << memfree;
    lv_label_set_text(minimum_free_heap_label_obj, oss.str().c_str());
  }
  { // lvgl task stack mark
    auto stack_mark =
        uxTaskGetStackHighWaterMark(Application::getLvglTaskHandle());
    std::ostringstream oss;
    oss << "LVGL task stack high water mark: " << stack_mark;
    lv_label_set_text(lvgl_task_stack_mark_label_obj, oss.str().c_str());
  }
  { // application task stack mark
    auto stack_mark =
        uxTaskGetStackHighWaterMark(Application::getApplicationTaskHandle());
    std::ostringstream oss;
    oss << "APP task stack high water mark: " << stack_mark;
    lv_label_set_text(app_task_stack_mark_label_obj, oss.str().c_str());
  }
}

//
template <>
std::ostream &Widget::operator<<(std::ostream &os,
                                 const Widget::ShowMeasured<DegC> &rhs) {
  os << "#";
  os << std::hex << std::setfill('0') << std::setw(2) << +rhs.color.ch.red
     << std::hex << std::setfill('0') << std::setw(2) << +rhs.color.ch.green
     << std::hex << std::setfill('0') << std::setw(2) << +rhs.color.ch.blue
     << " ";
  os << rhs.name << "  " << std::dec << std::fixed << std::setprecision(2)
     << rhs.meas.count() << rhs.unit;
  os << "#";
  return os;
}

//
template <>
std::ostream &Widget::operator<<(std::ostream &os,
                                 const Widget::ShowMeasured<HectoPa> &rhs) {
  os << "#";
  os << std::hex << std::setfill('0') << std::setw(2) << +rhs.color.ch.red
     << std::hex << std::setfill('0') << std::setw(2) << +rhs.color.ch.green
     << std::hex << std::setfill('0') << std::setw(2) << +rhs.color.ch.blue
     << " ";
  os << rhs.name << "  " << std::dec << std::fixed << std::setprecision(2)
     << rhs.meas.count() << rhs.unit;
  os << "#";
  return os;
}

//
template <>
std::ostream &Widget::operator<<(std::ostream &os,
                                 const Widget::ShowMeasured<Ppm> &rhs) {
  os << "#";
  os << std::hex << std::setfill('0') << std::setw(2) << +rhs.color.ch.red
     << std::hex << std::setfill('0') << std::setw(2) << +rhs.color.ch.green
     << std::hex << std::setfill('0') << std::setw(2) << +rhs.color.ch.blue
     << " ";
  os << rhs.name << "  " << std::dec << +rhs.meas.value << rhs.unit;
  os << "#";
  return os;
}

//
template <>
std::ostream &Widget::operator<<(std::ostream &os,
                                 const Widget::ShowMeasured<Ppb> &rhs) {
  os << "#";
  os << std::hex << std::setfill('0') << std::setw(2) << +rhs.color.ch.red
     << std::hex << std::setfill('0') << std::setw(2) << +rhs.color.ch.green
     << std::hex << std::setfill('0') << std::setw(2) << +rhs.color.ch.blue
     << " ";
  os << rhs.name << "  " << std::dec << +rhs.meas.value << rhs.unit;
  os << "#";
  return os;
}

//
//
//
const std::unordered_map<SensorId, lv_color_t>
    Widget::ChartLineColor::LINE_COLOR_MAP{
        {Application::SENSOR_DESCRIPTOR_BME280,
         lv_palette_lighten(LV_PALETTE_RED, 1)},
        {Application::SENSOR_DESCRIPTOR_SGP30,
         lv_palette_lighten(LV_PALETTE_ORANGE, 1)},
        {Application::SENSOR_DESCRIPTOR_SCD30,
         lv_palette_lighten(LV_PALETTE_INDIGO, 1)},
        {Application::SENSOR_DESCRIPTOR_SCD41,
         lv_palette_lighten(LV_PALETTE_PURPLE, 1)},
        {Application::SENSOR_DESCRIPTOR_M5ENV3,
         lv_palette_lighten(LV_PALETTE_BROWN, 1)}};

//
std::optional<lv_color_t>
Widget::ChartLineColor::getAssignedColor(SensorId sensor_id) {
  auto itr = LINE_COLOR_MAP.find(sensor_id);
  return itr != LINE_COLOR_MAP.end() ? std::make_optional(itr->second)
                                     : std::nullopt;
}

//
std::optional<lv_color32_t>
Widget::ChartLineColor::getAssignedColor32(SensorId sensor_id) {
  auto c = getAssignedColor(sensor_id);
  return c ? std::make_optional(lv_color32_t{.full = lv_color_to32(*c)})
           : std::nullopt;
}

//
template <typename T>
Widget::BasicChart<T>::BasicChart(lv_obj_t *parent_obj,
                                  std::string_view inSubheading) {
  if (parent_obj == nullptr) {
    M5_LOGE("null");
    return;
  }
  constexpr auto MARGIN{8};
  subheading.assign(inSubheading);
  // create
  if (title_obj = lv_label_create(parent_obj); title_obj == nullptr) {
    M5_LOGE("title had null");
    return;
  }
  lv_obj_set_style_text_align(title_obj, LV_TEXT_ALIGN_LEFT, LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(title_obj, &lv_font_montserrat_16,
                             LV_STATE_DEFAULT);
  lv_obj_set_size(title_obj, LV_PCT(100), lv_font_montserrat_16.line_height);
  lv_obj_align(title_obj, LV_ALIGN_TOP_LEFT, MARGIN, MARGIN);
  lv_label_set_text(title_obj, subheading.c_str());
  //
  if (label_obj = lv_label_create(parent_obj); label_obj == nullptr) {
    M5_LOGE("label had null");
    return;
  }
  const lv_font_t *FONT{&lv_font_montserrat_24};
  lv_obj_set_size(label_obj, lv_obj_get_content_width(parent_obj) - MARGIN * 2,
                  FONT->line_height);
  lv_obj_align_to(label_obj, title_obj, LV_ALIGN_OUT_BOTTOM_LEFT, 0, MARGIN);
  lv_label_set_long_mode(label_obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_label_set_recolor(label_obj, true);
  lv_obj_set_style_text_font(label_obj, FONT, LV_STATE_DEFAULT);
  lv_style_init(&label_style);
  lv_style_set_bg_opa(&label_style, LV_OPA_COVER);
  lv_style_set_bg_color(&label_style, lv_palette_darken(LV_PALETTE_BROWN, 4));
  lv_style_set_text_color(&label_style,
                          lv_palette_lighten(LV_PALETTE_BROWN, 4));
  lv_obj_add_style(label_obj, &label_style, LV_PART_MAIN);
  lv_label_set_text(label_obj, "");
  //
  if (chart_obj = lv_chart_create(parent_obj); chart_obj == nullptr) {
    M5_LOGE("chart had null");
    return;
  }
  lv_obj_set_style_bg_color(
      chart_obj, lv_palette_lighten(LV_PALETTE_LIGHT_GREEN, 2), LV_PART_MAIN);
  //
  constexpr auto X_TICK_LABEL_LEN = 30;
  constexpr auto Y_TICK_LABEL_LEN = 50;
  lv_obj_set_size(chart_obj,
                  lv_obj_get_content_width(parent_obj) //
                      - MARGIN - Y_TICK_LABEL_LEN,
                  lv_obj_get_content_height(parent_obj)           //
                      - MARGIN * 2 - lv_obj_get_height(title_obj) //
                      - MARGIN * 2 - lv_obj_get_height(label_obj) //
                      - MARGIN - X_TICK_LABEL_LEN);
  lv_obj_align_to(chart_obj, label_obj, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, MARGIN);
  if constexpr (true) {
    // Do not display points on the data
    lv_obj_set_style_size(chart_obj, 0, LV_PART_INDICATOR);
  }
  lv_chart_set_update_mode(chart_obj, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_type(chart_obj, LV_CHART_TYPE_LINE);
  lv_chart_set_range(chart_obj, LV_CHART_AXIS_PRIMARY_X, 0,
                     Gui::CHART_X_POINT_COUNT);
  lv_chart_set_range(chart_obj, LV_CHART_AXIS_PRIMARY_Y, 0, 1);
  //
  lv_chart_set_axis_tick(chart_obj, LV_CHART_AXIS_PRIMARY_X, 8, 4,
                         X_AXIS_TICK_COUNT, 2, true, X_TICK_LABEL_LEN);
  lv_chart_set_axis_tick(chart_obj, LV_CHART_AXIS_PRIMARY_Y, 4, 2,
                         Y_AXIS_TICK_COUNT, 2, true, Y_TICK_LABEL_LEN);
  // callback
  lv_obj_add_event_cb(
      chart_obj,
      [](lv_event_t *event) -> void {
        auto it = static_cast<Widget::BasicChart<T> *>(event->user_data);
        if (it == nullptr) {
          M5_LOGE("user_data had null");
        }
        lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(event);
        if (lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                        LV_CHART_DRAW_PART_TICK_LABEL)) {
          it->chart_draw_part_tick_label(dsc);
        }
      },
      LV_EVENT_DRAW_PART_BEGIN, this);
  //
  for (const auto &sensor : Application::getSensors()) {
    if (const auto p = sensor.get(); p) {
      chart_series_vect.emplace_back(
          ChartSeriesWrapper{p->getSensorDescriptor(), chart_obj});
      //
    }
  }
  for (auto &item : chart_series_vect) {
    item.chart_add_series();
  }
}

//
template <typename T> Widget::BasicChart<T>::~BasicChart() {
  //
  if (chart_obj) {
    lv_obj_remove_event_cb(chart_obj, nullptr);
    chart_series_vect.clear();
    lv_obj_del(chart_obj);
  }
  if (label_obj) {
    lv_obj_del(label_obj);
  }
  if (title_obj) {
    lv_obj_del(title_obj);
  }
}

//
template <typename T> void Widget::BasicChart<T>::render() {
  if (chart_obj == nullptr) {
    M5_LOGE("chart had null");
    return;
  }
  // 初期化
  for (auto &item : chart_series_vect) {
    if (item.available() == false) {
      M5_LOGE("chart series not available");
      return;
    }
    item.chart_set_point_count(Gui::CHART_X_POINT_COUNT);
    item.chart_set_all_value(LV_CHART_POINT_NONE);
  }
  //
  system_clock::time_point now_min = floor<minutes>(system_clock::now());
  begin_x_tp = now_min - minutes(Gui::CHART_X_POINT_COUNT);

  // データーベースより測定データーを得て
  // 各々のsensoridのchart seriesにセットする関数
  auto coordinateChartSeries = [this](size_t counter, DataType item) -> bool {
    auto &[sensorid, tp, value] = item;
    // 各々のsensoridのchart seriesにセットする。
    if (auto found_itr = std::find(chart_series_vect.begin(),
                                   chart_series_vect.end(), sensorid);
        found_itr != chart_series_vect.end()) {
      auto coord = coordinateXY(begin_x_tp, item);
      M5_LOGV("%d,%d", coord.x, coord.y);
      found_itr->chart_set_value_by_id(coord.x, coord.y);
    }
    // データー表示(デバッグ用)
    if constexpr (CORE_DEBUG_LEVEL >= ESP_LOG_VERBOSE) {
      //
      std::time_t time = system_clock::to_time_t(tp);
      std::tm local_time;
      localtime_r(&time, &local_time);
      std::ostringstream oss;
      //
      oss << "(" << +counter << ") ";
      oss << SensorDescriptor(sensorid).str() << ", ";
      oss << std::put_time(&local_time, "%F %T %Z");
      oss << ", " << +value;
      M5_LOGV("%s", oss.str().c_str());
    }
    return true; // always true
  };

  // データーベースより測定データーを得る
  if (read_measurements_from_database(Database::OrderByAtAsc, begin_x_tp,
                                      coordinateChartSeries) >= 1) {
    // 下限、上限
    auto y_min = std::numeric_limits<lv_coord_t>::max();
    auto y_max = std::numeric_limits<lv_coord_t>::min();
    for (auto &item : chart_series_vect) {
      auto [min, max] = item.getMinMaxOfYPoints();
      y_min = std::min(y_min, min);
      y_max = std::max(y_max, max);
    }
    //
    if (y_min == std::numeric_limits<lv_coord_t>::max() &&
        y_max == std::numeric_limits<lv_coord_t>::min()) {
      lv_chart_set_range(chart_obj, LV_CHART_AXIS_PRIMARY_Y, 0, 1);
    } else {
      y_min = floor(y_min / 500.0f) * 500.0f;
      y_max = ceil(y_max / 500.0f) * 500.0f;
      lv_chart_set_range(chart_obj, LV_CHART_AXIS_PRIMARY_Y, y_min, y_max);
      M5_LOGV("y_min:%d, y_max:%d", y_min, y_max);
    }
  }
  // 横軸の開始位置
  uint16_t x_start_point = 0;
  for (auto &item : chart_series_vect) {
    if (auto opt_x = item.getXStartPointForValidValues(); opt_x) {
      x_start_point = std::max(x_start_point, *opt_x);
    }
  }
  for (auto &item : chart_series_vect) {
    item.chart_set_x_start_point(x_start_point);
  }
  //
  lv_chart_refresh(chart_obj);
}

//
// 気温
//
Widget::TemperatureChart::TemperatureChart(InitArg init) : TileBase{init} {
  if (tileview_obj == nullptr) {
    M5_LOGE("tileview had null");
    return;
  }
}

//
void Widget::TemperatureChart::update() {
  auto bme280 =
      Application::getMeasurementsDatabase().getLatestMeasurementBme280();
  auto scd30 =
      Application::getMeasurementsDatabase().getLatestMeasurementScd30();
  auto scd41 =
      Application::getMeasurementsDatabase().getLatestMeasurementScd41();
  auto m5env3 =
      Application::getMeasurementsDatabase().getLatestMeasurementM5Env3();
  auto present = Measurements{bme280, scd30, scd41, m5env3};

  //
  if (present != latest) {
    latest = present;
    render();
  }
}

//
void Widget::TemperatureChart::render() {
  auto &[bme280, scd30, scd41, m5env3] = latest;
  //
  auto toColor32 = [](SensorId sensor_id) -> lv_color32_t {
    auto c = ChartLineColor::getAssignedColor32(sensor_id);
    return c ? *c : lv_color32_t{.full = lv_color_to32(lv_color_white())};
  };

  //
  std::ostringstream oss;
  if (bme280.has_value()) {
    ShowMeasured<DegC> s{.color = toColor32(bme280->second.sensor_descriptor),
                         .name = "BME280",
                         .unit = "C",
                         .meas = bme280->second.temperature};
    oss << s << "  ";
  }
  if (scd30.has_value()) {
    ShowMeasured<DegC> s{.color = toColor32(scd30->second.sensor_descriptor),
                         .name = "SCD30",
                         .unit = "C",
                         .meas = scd30->second.temperature};
    oss << s << "  ";
  }
  if (scd41.has_value()) {
    ShowMeasured<DegC> s{.color = toColor32(scd41->second.sensor_descriptor),
                         .name = "SCD41",
                         .unit = "C",
                         .meas = scd41->second.temperature};
    oss << s << "  ";
  }
  if (m5env3.has_value()) {
    ShowMeasured<DegC> s{.color = toColor32(m5env3->second.sensor_descriptor),
                         .name = "ENV.III",
                         .unit = "C",
                         .meas = m5env3->second.temperature};
    oss << s << "  ";
  }

  //
  if (basic_chart) {
    lv_label_set_text(basic_chart->getLabelObj(), oss.str().c_str());
    //
    basic_chart->render();
  }
}

// データーベースからデーターを得る
size_t Widget::TemperatureChart::C::read_measurements_from_database(
    Database::OrderBy order, system_clock::time_point at_begin,
    Database::ReadCallback<Database::TimePointAndDouble> callback) {
  return Application::getMeasurementsDatabase().read_temperatures(
      order, at_begin, callback);
}

// データを座標に変換する関数
lv_point_t Widget::TemperatureChart::C::coordinateXY(
    system_clock::time_point tp_zero, const Database::TimePointAndDouble &in) {
  auto [sensorid, timepoint, fp_value] = in;
  auto degc = DegC(fp_value);
  auto centi_degc = std::chrono::duration_cast<CentiDegC>(degc);
  uint32_t timediff =
      duration_cast<minutes>(floor<minutes>(timepoint) - tp_zero).count();
  lv_coord_t x = std::clamp(timediff, 0U, uint32_t(Gui::CHART_X_POINT_COUNT));
  lv_coord_t y = static_cast<lv_coord_t>(centi_degc.count());
  return lv_point_t{.x = x, .y = y};
};

//
void Widget::TemperatureChart::C::chart_draw_part_tick_label(
    lv_obj_draw_part_dsc_t *dsc) {
  switch (dsc->id) {
  case LV_CHART_AXIS_PRIMARY_X:
    if (dsc->text) {
      int32_t slope = Gui::CHART_X_POINT_COUNT / (X_AXIS_TICK_COUNT - 1);
      system_clock::time_point tp = getBeginX() + minutes{slope * dsc->value};
      std::time_t time = system_clock::to_time_t(tp);
      std::tm local_time;
      localtime_r(&time, &local_time);
      //
      lv_snprintf(dsc->text, dsc->text_length, "%02d:%02d", local_time.tm_hour,
                  local_time.tm_min);
    }
    break;
  case LV_CHART_AXIS_PRIMARY_Y:
    if (dsc->text) {
      auto centiC = CentiDegC(dsc->value);
      lv_snprintf(dsc->text, dsc->text_length, "%.1f", DegC(centiC).count());
    }
    break;
  default:
    M5_LOGD("axis id:%d is ignored.", dsc->id);
    break;
  }
}

//
// 相対湿度
//
Widget::RelativeHumidityChart::RelativeHumidityChart(InitArg init)
    : TileBase{init} {
  if (tileview_obj == nullptr) {
    M5_LOGE("tileview had null");
    return;
  }
}

//
void Widget::RelativeHumidityChart::update() {
  auto bme280 =
      Application::getMeasurementsDatabase().getLatestMeasurementBme280();
  auto scd30 =
      Application::getMeasurementsDatabase().getLatestMeasurementScd30();
  auto scd41 =
      Application::getMeasurementsDatabase().getLatestMeasurementScd41();
  auto m5env3 =
      Application::getMeasurementsDatabase().getLatestMeasurementM5Env3();
  auto present = Measurements{bme280, scd30, scd41, m5env3};

  //
  if (present != latest) {
    latest = present;
    //
    render();
  }
}

//
void Widget::RelativeHumidityChart::render() {
  auto &[bme280, scd30, scd41, m5env3] = latest;
  //
  auto toColor32 = [](SensorId sensor_id) -> lv_color32_t {
    auto c = ChartLineColor::getAssignedColor32(sensor_id);
    return c ? *c : lv_color32_t{.full = lv_color_to32(lv_color_white())};
  };

  //
  std::ostringstream oss;
  if (bme280.has_value()) {
    ShowMeasured<PctRH> s{.color = toColor32(bme280->second.sensor_descriptor),
                          .name = "BME280",
                          .unit = "%",
                          .meas = bme280->second.relative_humidity};
    oss << s << "  ";
  }
  if (scd30.has_value()) {
    ShowMeasured<PctRH> s{.color = toColor32(scd30->second.sensor_descriptor),
                          .name = "SCD30",
                          .unit = "%",
                          .meas = scd30->second.relative_humidity};
    oss << s << "  ";
  }
  if (scd41.has_value()) {
    ShowMeasured<PctRH> s{.color = toColor32(scd41->second.sensor_descriptor),
                          .name = "SCD41",
                          .unit = "%",
                          .meas = scd41->second.relative_humidity};
    oss << s << "  ";
  }
  if (m5env3.has_value()) {
    ShowMeasured<PctRH> s{.color = toColor32(m5env3->second.sensor_descriptor),
                          .name = "ENV.III",
                          .unit = "%",
                          .meas = m5env3->second.relative_humidity};
    oss << s << "  ";
  }

  //
  if (basic_chart) {
    lv_label_set_text(basic_chart->getLabelObj(), oss.str().c_str());
    //
    basic_chart->render();
  }
}

// データーベースからデーターを得る
size_t Widget::RelativeHumidityChart::C::read_measurements_from_database(
    Database::OrderBy order, system_clock::time_point at_begin,
    Database::ReadCallback<Database::TimePointAndDouble> callback) {
  return Application::getMeasurementsDatabase().read_relative_humidities(
      order, at_begin, callback);
}

// データを座標に変換する関数
lv_point_t Widget::RelativeHumidityChart::C::coordinateXY(
    system_clock::time_point tp_zero, const Database::TimePointAndDouble &in) {
  auto [sensorid, timepoint, fp_value] = in;
  auto percentRelativeHumidity = PctRH(fp_value);
  auto rh = std::chrono::duration_cast<CentiRH>(percentRelativeHumidity);
  uint32_t timediff =
      duration_cast<minutes>(floor<minutes>(timepoint) - tp_zero).count();
  lv_coord_t x = std::clamp(timediff, 0U, uint32_t(Gui::CHART_X_POINT_COUNT));
  lv_coord_t y = static_cast<lv_coord_t>(rh.count());
  return lv_point_t{.x = x, .y = y};
};

void Widget::RelativeHumidityChart::C::chart_draw_part_tick_label(
    lv_obj_draw_part_dsc_t *dsc) {
  switch (dsc->id) {
  case LV_CHART_AXIS_PRIMARY_X:
    if (dsc->text) {
      int32_t slope = Gui::CHART_X_POINT_COUNT / (X_AXIS_TICK_COUNT - 1);
      system_clock::time_point tp = getBeginX() + minutes{slope * dsc->value};
      std::time_t time = system_clock::to_time_t(tp);
      std::tm local_time;
      localtime_r(&time, &local_time);
      //
      lv_snprintf(dsc->text, dsc->text_length, "%02d:%02d", local_time.tm_hour,
                  local_time.tm_min);
    }
    break;
  case LV_CHART_AXIS_PRIMARY_Y:
    if (dsc->text) {
      auto centiRH = CentiRH(dsc->value);
      lv_snprintf(dsc->text, dsc->text_length, "%.1f", PctRH(centiRH).count());
    }
    break;
  default:
    M5_LOGD("axis id:%d is ignored.", dsc->id);
    break;
  }
}

//
// 気圧
//
Widget::PressureChart::PressureChart(InitArg init) : TileBase{init} {
  if (tileview_obj == nullptr) {
    M5_LOGE("tileview had null");
    return;
  }
}

//
void Widget::PressureChart::update() {
  auto bme280 =
      Application::getMeasurementsDatabase().getLatestMeasurementBme280();
  auto m5env3 =
      Application::getMeasurementsDatabase().getLatestMeasurementM5Env3();
  auto present = Measurements{bme280, m5env3};

  //
  if (present != latest) {
    latest = present;
    //
    render();
  }
}

//
void Widget::PressureChart::render() {
  auto &[bme280, m5env3] = latest;
  //
  auto toColor32 = [](SensorId sensor_id) -> lv_color32_t {
    auto c = ChartLineColor::getAssignedColor32(sensor_id);
    return c ? *c : lv_color32_t{.full = lv_color_to32(lv_color_white())};
  };

  //
  std::ostringstream oss;
  if (bme280.has_value()) {
    ShowMeasured<HectoPa> s{.color =
                                toColor32(bme280->second.sensor_descriptor),
                            .name = "BME280",
                            .unit = "hpa",
                            .meas = bme280->second.pressure};
    oss << s << "  ";
  }
  if (m5env3.has_value()) {
    ShowMeasured<HectoPa> s{.color =
                                toColor32(m5env3->second.sensor_descriptor),
                            .name = "ENV.III",
                            .unit = "hpa",
                            .meas = m5env3->second.pressure};
    oss << s << "  ";
  }

  //
  if (basic_chart) {
    lv_label_set_text(basic_chart->getLabelObj(), oss.str().c_str());
    //
    basic_chart->render();
  }
}

// データーベースからデーターを得る
size_t Widget::PressureChart::C::read_measurements_from_database(
    Database::OrderBy order, system_clock::time_point at_begin,
    Database::ReadCallback<Database::TimePointAndDouble> callback) {
  return Application::getMeasurementsDatabase().read_pressures(order, at_begin,
                                                               callback);
}

// データを座標に変換する関数
lv_point_t
Widget::PressureChart::C::coordinateXY(system_clock::time_point tp_zero,
                                       const Database::TimePointAndDouble &in) {
  auto [sensorid, timepoint, fp_value] = in;
  auto hpa = HectoPa(fp_value);
  auto pascal = std::chrono::duration_cast<Pascal>(hpa - BIAS);
  uint32_t timediff =
      duration_cast<minutes>(floor<minutes>(timepoint) - tp_zero).count();
  lv_coord_t x = std::clamp(timediff, 0U, uint32_t(Gui::CHART_X_POINT_COUNT));
  lv_coord_t y = static_cast<lv_coord_t>(pascal.count());
  return lv_point_t{.x = x, .y = y};
};

//
void Widget::PressureChart::C::chart_draw_part_tick_label(
    lv_obj_draw_part_dsc_t *dsc) {
  switch (dsc->id) {
  case LV_CHART_AXIS_PRIMARY_X:
    if (dsc->text) {
      int32_t slope = Gui::CHART_X_POINT_COUNT / (X_AXIS_TICK_COUNT - 1);
      system_clock::time_point tp = getBeginX() + minutes{slope * dsc->value};
      std::time_t time = system_clock::to_time_t(tp);
      std::tm local_time;
      localtime_r(&time, &local_time);
      //
      lv_snprintf(dsc->text, dsc->text_length, "%02d:%02d", local_time.tm_hour,
                  local_time.tm_min);
    }
    break;
  case LV_CHART_AXIS_PRIMARY_Y:
    if (dsc->text) {
      auto pascal = Pascal(dsc->value) + BIAS;
      lv_snprintf(dsc->text, dsc->text_length, "%.1f", HectoPa(pascal).count());
    }
    break;
  default:
    M5_LOGD("axis id:%d is ignored.", dsc->id);
    break;
  }
}

//
// Co2 / ECo2
//
Widget::CarbonDeoxidesChart::CarbonDeoxidesChart(InitArg init)
    : TileBase{init} {
  if (tileview_obj == nullptr) {
    M5_LOGE("tileview had null");
    return;
  }
}

//
void Widget::CarbonDeoxidesChart::update() {
  auto sgp30 =
      Application::getMeasurementsDatabase().getLatestMeasurementSgp30();
  auto scd30 =
      Application::getMeasurementsDatabase().getLatestMeasurementScd30();
  auto scd41 =
      Application::getMeasurementsDatabase().getLatestMeasurementScd41();
  auto present = Measurements{sgp30, scd30, scd41};

  //
  if (present != latest) {
    latest = present;
    //
    render();
  }
}

//
void Widget::CarbonDeoxidesChart::render() {
  auto &[sgp30, scd30, scd41] = latest;
  //
  auto toColor32 = [](SensorId sensor_id) -> lv_color32_t {
    auto c = ChartLineColor::getAssignedColor32(sensor_id);
    return c ? *c : lv_color32_t{.full = lv_color_to32(lv_color_white())};
  };

  //
  std::ostringstream oss;
  if (sgp30.has_value()) {
    ShowMeasured<Ppm> s{.color = toColor32(sgp30->second.sensor_descriptor),
                        .name = "SGP30",
                        .unit = "ppm",
                        .meas = sgp30->second.eCo2};
    oss << s << "  ";
  }
  if (scd30.has_value()) {
    ShowMeasured<Ppm> s{.color = toColor32(scd30->second.sensor_descriptor),
                        .name = "SCD30",
                        .unit = "ppm",
                        .meas = scd30->second.co2};
    oss << s << "  ";
  }
  if (scd41.has_value()) {
    ShowMeasured<Ppm> s{.color = toColor32(scd41->second.sensor_descriptor),
                        .name = "SCD41",
                        .unit = "ppm",
                        .meas = scd41->second.co2};
    oss << s << "  ";
  }

  //
  if (basic_chart) {
    lv_label_set_text(basic_chart->getLabelObj(), oss.str().c_str());
    //
    basic_chart->render();
  }
}

// データーベースからデーターを得る
size_t Widget::CarbonDeoxidesChart::C::read_measurements_from_database(
    Database::OrderBy order, system_clock::time_point at_begin,
    Database::ReadCallback<Database::TimePointAndUInt16> callback) {
  return Application::getMeasurementsDatabase().read_carbon_deoxides(
      order, at_begin, callback);
}

// データを座標に変換する関数
lv_point_t Widget::CarbonDeoxidesChart::C::coordinateXY(
    system_clock::time_point tp_zero, const Database::TimePointAndUInt16 &in) {
  auto [sensorid, timepoint, int_value] = in;
  uint32_t timediff =
      duration_cast<minutes>(floor<minutes>(timepoint) - tp_zero).count();
  lv_coord_t x = std::clamp(timediff, 0U, uint32_t(Gui::CHART_X_POINT_COUNT));
  lv_coord_t y = static_cast<lv_coord_t>(int_value);
  return lv_point_t{.x = x, .y = y};
};

//
void Widget::CarbonDeoxidesChart::C::chart_draw_part_tick_label(
    lv_obj_draw_part_dsc_t *dsc) {
  switch (dsc->id) {
  case LV_CHART_AXIS_PRIMARY_X:
    if (dsc->text) {
      int32_t slope = Gui::CHART_X_POINT_COUNT / (X_AXIS_TICK_COUNT - 1);
      system_clock::time_point tp = getBeginX() + minutes{slope * dsc->value};
      std::time_t time = system_clock::to_time_t(tp);
      std::tm local_time;
      localtime_r(&time, &local_time);
      //
      lv_snprintf(dsc->text, dsc->text_length, "%02d:%02d", local_time.tm_hour,
                  local_time.tm_min);
    }
    break;
  case LV_CHART_AXIS_PRIMARY_Y:
    if (dsc->text) {
      auto ppm = dsc->value;
      lv_snprintf(dsc->text, dsc->text_length, "%d", ppm);
    }
    break;
  default:
    M5_LOGD("axis id:%d is ignored.", dsc->id);
    break;
  }
}

//
// Total VOC
//
Widget::TotalVocChart::TotalVocChart(InitArg init) : TileBase{init} {
  if (tileview_obj == nullptr) {
    M5_LOGE("tileview had null");
    return;
  }
}

//
void Widget::TotalVocChart::update() {
  auto sgp30 =
      Application::getMeasurementsDatabase().getLatestMeasurementSgp30();
  auto present = sgp30;

  //
  if (present != latest) {
    latest = present;
    //
    render();
  }
}

//
void Widget::TotalVocChart::render() {
  auto &sgp30 = latest;
  //
  auto toColor32 = [](SensorId sensor_id) -> lv_color32_t {
    auto c = ChartLineColor::getAssignedColor32(sensor_id);
    return c ? *c : lv_color32_t{.full = lv_color_to32(lv_color_white())};
  };

  //
  std::ostringstream oss;
  if (sgp30.has_value()) {
    ShowMeasured<Ppb> s{.color = toColor32(sgp30->second.sensor_descriptor),
                        .name = "SGP30",
                        .unit = "ppb",
                        .meas = sgp30->second.tvoc};
    oss << s << "  ";
  }

  //
  if (basic_chart) {
    lv_label_set_text(basic_chart->getLabelObj(), oss.str().c_str());
    //
    basic_chart->render();
  }
}

// データーベースからデーターを得る
size_t Widget::TotalVocChart::C::read_measurements_from_database(
    Database::OrderBy order, system_clock::time_point at_begin,
    Database::ReadCallback<Database::TimePointAndUInt16> callback) {
  return Application::getMeasurementsDatabase().read_total_vocs(order, at_begin,
                                                                callback);
}

// データを座標に変換する関数
lv_point_t
Widget::TotalVocChart::C::coordinateXY(system_clock::time_point tp_zero,
                                       const Database::TimePointAndUInt16 &in) {
  auto [sensorid, timepoint, int_value] = in;
  uint32_t timediff =
      duration_cast<minutes>(floor<minutes>(timepoint) - tp_zero).count();
  lv_coord_t x = std::clamp(timediff, 0U, uint32_t(Gui::CHART_X_POINT_COUNT));
  lv_coord_t y = static_cast<lv_coord_t>(int_value / DIVIDER);
  return lv_point_t{.x = x, .y = y};
};

//
void Widget::TotalVocChart::C::chart_draw_part_tick_label(
    lv_obj_draw_part_dsc_t *dsc) {
  switch (dsc->id) {
  case LV_CHART_AXIS_PRIMARY_X:
    if (dsc->text) {
      int32_t slope = Gui::CHART_X_POINT_COUNT / (X_AXIS_TICK_COUNT - 1);
      system_clock::time_point tp = getBeginX() + minutes{slope * dsc->value};
      std::time_t time = system_clock::to_time_t(tp);
      std::tm local_time;
      localtime_r(&time, &local_time);
      //
      lv_snprintf(dsc->text, dsc->text_length, "%02d:%02d", local_time.tm_hour,
                  local_time.tm_min);
    }
    break;
  case LV_CHART_AXIS_PRIMARY_Y:
    if (dsc->text) {
      auto tvoc = dsc->value * DIVIDER;
      lv_snprintf(dsc->text, dsc->text_length, "%d", tvoc);
    }
    break;
  default:
    M5_LOGD("axis id:%d is ignored.", dsc->id);
    break;
  }
}
