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
  _tileview_obj.reset(lv_tileview_create(nullptr), lv_obj_del);
  if (!_tileview_obj) {
    M5_LOGE("memory allocation error");
    return false;
  }
  //
  auto col = 0;
  // make the first tile
  add_tile<Widget::BootMessage>({_tileview_obj, col++, 0, LV_DIR_RIGHT});
  // TileWidget::BootMessageの次からこの並び順
  if constexpr (false) {
    add_tile<Widget::Clock>({_tileview_obj, col++, 0, LV_DIR_HOR});
  }
  add_tile<Widget::SystemHealthy>({_tileview_obj, col++, 0, LV_DIR_HOR});
  add_tile<Widget::Summary>({_tileview_obj, col++, 0, LV_DIR_HOR});
  add_tile<Widget::TemperatureChart>({_tileview_obj, col++, 0, LV_DIR_HOR});
  add_tile<Widget::RelativeHumidityChart>(
      {_tileview_obj, col++, 0, LV_DIR_HOR});
  add_tile<Widget::PressureChart>({_tileview_obj, col++, 0, LV_DIR_HOR});
  add_tile<Widget::CarbonDeoxidesChart>({_tileview_obj, col++, 0, LV_DIR_HOR});
  // 最後のタイルだけ右移動を禁止する
  add_tile<Widget::TotalVocChart>({_tileview_obj, col++, 0, LV_DIR_LEFT});
  //
  if (_startup_widget) {
    _startup_widget.reset();
  }
  //
  lv_scr_load(_tileview_obj.get());
  //
  home();
  return true;
}

//
void Gui::home() {
  if (!_tileview_obj) {
    M5_LOGE("null");
    return;
  }
  vibrate();
  constexpr auto COL_ID = 2;
  constexpr auto ROW_ID = 0;
  lv_obj_set_tile_id(_tileview_obj.get(), COL_ID, ROW_ID, LV_ANIM_OFF);
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
  constexpr auto MILLISECONDS = 120;
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
  _container_obj.reset(lv_obj_create(lv_scr_act()), lv_obj_del);
  if (_container_obj) {
    lv_obj_set_size(_container_obj.get(), display_width, display_height);
    lv_obj_align(_container_obj.get(), LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(_container_obj.get(), LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_container_obj.get(),
                              lv_palette_lighten(LV_PALETTE_GREY, 2),
                              LV_PART_MAIN);
    //
    _title_obj.reset(lv_label_create(_container_obj.get()), lv_obj_del);
    if (_title_obj) {
      const lv_font_t *FONT{&lv_font_montserrat_30};
      lv_obj_set_size(_title_obj.get(), display_width - MARGIN * 2,
                      FONT->line_height);
      lv_obj_align(_title_obj.get(), LV_ALIGN_CENTER, 0,
                   -1 * FONT->line_height);
      lv_obj_set_style_text_font(_title_obj.get(), FONT, LV_STATE_DEFAULT);
      lv_style_init(&_title_style);
      lv_style_set_text_color(&_title_style,
                              lv_palette_darken(LV_PALETTE_BROWN, 3));
      lv_style_set_text_align(&_title_style, LV_TEXT_ALIGN_CENTER);
      lv_obj_add_style(_title_obj.get(), &_title_style, LV_PART_MAIN);
      lv_label_set_text(_title_obj.get(), "HELLO");
    }
    //
    _label_obj.reset(lv_label_create(_container_obj.get()), lv_obj_del);
    if (_label_obj) {
      const lv_font_t *FONT{&lv_font_montserrat_16};
      lv_obj_set_size(_label_obj.get(), display_width - MARGIN * 2,
                      FONT->line_height);
      lv_obj_align(_label_obj.get(), LV_ALIGN_CENTER, 0, FONT->line_height);
      lv_label_set_long_mode(_label_obj.get(), LV_LABEL_LONG_SCROLL);
      lv_label_set_recolor(_label_obj.get(), true);
      lv_obj_set_style_text_font(_label_obj.get(), FONT, LV_STATE_DEFAULT);
      lv_style_init(&_label_style);
      lv_style_set_text_color(&_label_style,
                              lv_palette_darken(LV_PALETTE_BROWN, 3));
      lv_style_set_text_align(&_label_style, LV_TEXT_ALIGN_CENTER);
      lv_obj_add_style(_label_obj.get(), &_label_style, LV_PART_MAIN);
      lv_label_set_text(_label_obj.get(), "");
    }
    //
    _bar_obj.reset(lv_bar_create(_container_obj.get()), lv_obj_del);
    if (_label_obj && _bar_obj) {
      lv_obj_set_width(_bar_obj.get(), display_width - MARGIN * 2);
      lv_obj_align_to(_bar_obj.get(), _label_obj.get(), LV_ALIGN_OUT_BOTTOM_MID,
                      0, MARGIN);
    }
    updateProgress(0);
  }
}

//
void Widget::Startup::updateMessage(const std::string &s) {
  if (_label_obj) {
    lv_label_set_text(_label_obj.get(), s.c_str());
  } else {
    M5_LOGE("null pointer");
  }
}

//
void Widget::Startup::updateProgress(int16_t percent) {
  if (_bar_obj) {
    lv_bar_set_value(_bar_obj.get(), percent, LV_ANIM_ON);
  } else {
    M5_LOGE("null pointer");
  }
}

//
//
//
Widget::BootMessage::BootMessage(Widget::InitArg init) : TileBase{init} {
  if (_tile_obj) {
    // create
    _message_label_obj.reset(lv_label_create(_tile_obj.get()), lv_obj_del);
    if (_message_label_obj) {
      lv_label_set_long_mode(_message_label_obj.get(), LV_LABEL_LONG_WRAP);
    }
  } else {
    M5_LOGE("null pointer");
  }
}

void Widget::BootMessage::update() {
  if (auto x = Application::getInstance()->getStartupLog(); x != _latest) {
    _latest = x;
    render();
  }
}

void Widget::BootMessage::render() {
  if (_message_label_obj) {
    lv_label_set_text_static(_message_label_obj.get(), _latest.c_str());
  } else {
    M5_LOGE("null pointer");
  }
}

//
//
//
Widget::Summary::Summary(Widget::InitArg init) : TileBase{init} {
  if (_tile_obj) {
    // create
    _table_obj.reset(lv_table_create(_tile_obj.get()), lv_obj_del);
    //
    auto w = lv_obj_get_content_width(_tile_obj.get());
    lv_table_set_col_width(_table_obj.get(), 0, w * 6 / 12);
    lv_table_set_col_width(_table_obj.get(), 1, w * 3 / 12);
    lv_table_set_col_width(_table_obj.get(), 2, w * 3 / 12);
    lv_obj_set_width(_table_obj.get(), w);
    lv_obj_set_height(_table_obj.get(),
                      lv_obj_get_content_height(_tile_obj.get()));
    lv_obj_center(_table_obj.get());
    lv_obj_set_style_text_font(_table_obj.get(), &lv_font_montserrat_16,
                               LV_PART_ITEMS | LV_STATE_DEFAULT);
    //
    lv_obj_add_event_cb(_table_obj.get(), event_draw_part_begin_callback,
                        LV_EVENT_DRAW_PART_BEGIN, nullptr);
  } else {
    M5_LOGE("null pointer");
    return;
  }
}

void Widget::Summary::update() {
  auto present = Measurements{
      Application::getMeasurementsDatabase().getLatestMeasurementBme280(),
      Application::getMeasurementsDatabase().getLatestMeasurementSgp30(),
      Application::getMeasurementsDatabase().getLatestMeasurementScd30(),
      Application::getMeasurementsDatabase().getLatestMeasurementScd41(),
      Application::getMeasurementsDatabase().getLatestMeasurementM5Env3()};
  if (present != _latest) {
    render();
    _latest = present;
  }
}

void Widget::Summary::render() {
  if (_table_obj) {
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
        lv_table_set_cell_value(_table_obj.get(), row, 0, "ENV3 Temp");
        if (auto temp = m5env3 ? std::make_optional(m5env3->second.temperature)
                               : std::nullopt;
            temp) {
          const DegC t = static_cast<DegC>(*temp);
          oss.str("");
          oss << +t.count();
          lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
        } else {
          lv_table_set_cell_value(_table_obj.get(), row, 1, "-");
        }
        lv_table_set_cell_value(_table_obj.get(), row, 2, "C");
        row++;
        //
        lv_table_set_cell_value(_table_obj.get(), row, 0, "ENV3 Humi");
        if (auto humi =
                m5env3 ? std::make_optional(m5env3->second.relative_humidity)
                       : std::nullopt;
            humi) {
          const PctRH rh = *humi;
          oss.str("");
          oss << +rh.count();
          lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
        } else {
          lv_table_set_cell_value(_table_obj.get(), row, 1, "-");
        }
        lv_table_set_cell_value(_table_obj.get(), row, 2, "%RH");
        row++;
        //
        lv_table_set_cell_value(_table_obj.get(), row, 0, "ENV3 Pres");
        if (auto pres = m5env3 ? std::make_optional(m5env3->second.pressure)
                               : std::nullopt;
            pres) {
          const HectoPa hpa = *pres;
          oss.str("");
          oss << +hpa.count();
          lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
        } else {
          lv_table_set_cell_value(_table_obj.get(), row, 1, "-");
        }
        lv_table_set_cell_value(_table_obj.get(), row, 2, "hPa");
        row++;
      } break;
      case Application::SENSOR_DESCRIPTOR_BME280: { // BME280
        auto bme280 =
            Application::getMeasurementsDatabase().getLatestMeasurementBme280();
        lv_table_set_cell_value(_table_obj.get(), row, 0, "BME280 Temp");
        if (auto temp = bme280 ? std::make_optional(bme280->second.temperature)
                               : std::nullopt;
            temp) {
          const DegC t{*temp};
          oss.str("");
          oss << +t.count();
          lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
        } else {
          lv_table_set_cell_value(_table_obj.get(), row, 1, "-");
        }
        lv_table_set_cell_value(_table_obj.get(), row, 2, "C");
        row++;
        //
        lv_table_set_cell_value(_table_obj.get(), row, 0, "BME280 Humi");
        if (auto humi =
                bme280 ? std::make_optional(bme280->second.relative_humidity)
                       : std::nullopt;
            humi) {
          const PctRH rh{*humi};
          oss.str("");
          oss << +rh.count();
          lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
        } else {
          lv_table_set_cell_value(_table_obj.get(), row, 1, "-");
        }
        lv_table_set_cell_value(_table_obj.get(), row, 2, "%RH");
        row++;
        //
        lv_table_set_cell_value(_table_obj.get(), row, 0, "BME280 Pres");
        if (auto pres = bme280 ? std::make_optional(bme280->second.pressure)
                               : std::nullopt;
            pres) {
          const HectoPa hpa{*pres};
          oss.str("");
          oss << +hpa.count();
          lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
        } else {
          lv_table_set_cell_value(_table_obj.get(), row, 1, "-");
        }
        lv_table_set_cell_value(_table_obj.get(), row, 2, "hPa");
        row++;
      } break;
      case Application::SENSOR_DESCRIPTOR_SCD30: { // SCD30
        auto scd30 =
            Application::getMeasurementsDatabase().getLatestMeasurementScd30();
        lv_table_set_cell_value(_table_obj.get(), row, 0, "SCD30 Temp");
        if (auto temp = scd30 ? std::make_optional(scd30->second.temperature)
                              : std::nullopt;
            temp) {
          const DegC t{*temp};
          oss.str("");
          oss << +t.count();
          lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
        } else {
          lv_table_set_cell_value(_table_obj.get(), row, 1, "-");
        }
        lv_table_set_cell_value(_table_obj.get(), row, 2, "C");
        row++;
        //
        lv_table_set_cell_value(_table_obj.get(), row, 0, "SCD30 Humi");
        if (auto humi =
                scd30 ? std::make_optional(scd30->second.relative_humidity)
                      : std::nullopt;
            humi) {
          const PctRH rh{*humi};
          oss.str("");
          oss << +rh.count();
          lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
        } else {
          lv_table_set_cell_value(_table_obj.get(), row, 1, "-");
        }
        lv_table_set_cell_value(_table_obj.get(), row, 2, "%RH");
        row++;
        //
        lv_table_set_cell_value(_table_obj.get(), row, 0, "SCD30 CO2");
        if (auto carbon_dioxide =
                scd30 ? std::make_optional(scd30->second.co2) : std::nullopt;
            carbon_dioxide) {
          const Ppm co2{*carbon_dioxide};
          oss.str("");
          oss << +co2.value;
          lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
        } else {
          lv_table_set_cell_value(_table_obj.get(), row, 1, "-");
        }
        lv_table_set_cell_value(_table_obj.get(), row, 2, "ppm");
        row++;
      } break;
      case Application::SENSOR_DESCRIPTOR_SCD41: { // SCD41
        auto scd41 =
            Application::getMeasurementsDatabase().getLatestMeasurementScd41();
        lv_table_set_cell_value(_table_obj.get(), row, 0, "SCD41 Temp");
        if (auto temp = scd41 ? std::make_optional(scd41->second.temperature)
                              : std::nullopt;
            temp) {
          const DegC t{*temp};
          oss.str("");
          oss << +t.count();
          lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
        } else {
          lv_table_set_cell_value(_table_obj.get(), row, 1, "-");
        }
        lv_table_set_cell_value(_table_obj.get(), row, 2, "C");
        row++;
        //
        lv_table_set_cell_value(_table_obj.get(), row, 0, "SCD41 Humi");
        if (auto humi =
                scd41 ? std::make_optional(scd41->second.relative_humidity)
                      : std::nullopt;
            humi) {
          const PctRH rh{*humi};
          oss.str("");
          oss << +rh.count();
          lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
        } else {
          lv_table_set_cell_value(_table_obj.get(), row, 1, "-");
        }
        lv_table_set_cell_value(_table_obj.get(), row, 2, "%RH");
        row++;
        //
        lv_table_set_cell_value(_table_obj.get(), row, 0, "SCD41 CO2");
        if (auto carbon_dioxide =
                scd41 ? std::make_optional(scd41->second.co2) : std::nullopt;
            carbon_dioxide) {
          const Ppm co2{*carbon_dioxide};
          oss.str("");
          oss << +co2.value;
          lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
        } else {
          lv_table_set_cell_value(_table_obj.get(), row, 1, "-");
        }
        lv_table_set_cell_value(_table_obj.get(), row, 2, "ppm");
        row++;
      } break;
      case Application::SENSOR_DESCRIPTOR_SGP30: { // SGP30
        auto sgp30 =
            Application::getMeasurementsDatabase().getLatestMeasurementSgp30();
        lv_table_set_cell_value(_table_obj.get(), row, 0, "SGP30 eCO2");
        if (auto equivalent_carbon_dioxide =
                sgp30 ? std::make_optional(sgp30->second.eCo2) : std::nullopt;
            equivalent_carbon_dioxide) {
          const Ppm eco2{*equivalent_carbon_dioxide};
          oss.str("");
          oss << +eco2.value;
          lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
        } else {
          lv_table_set_cell_value(_table_obj.get(), row, 1, "-");
        }
        lv_table_set_cell_value(_table_obj.get(), row, 2, "ppm");
        row++;
        //
        lv_table_set_cell_value(_table_obj.get(), row, 0, "SGP30 TVOC");
        if (auto total_voc =
                sgp30 ? std::make_optional(sgp30->second.tvoc) : std::nullopt;
            total_voc) {
          const Ppb tvoc{*total_voc};
          oss.str("");
          oss << +tvoc.value;
          lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
        } else {
          lv_table_set_cell_value(_table_obj.get(), row, 1, "-");
        }
        lv_table_set_cell_value(_table_obj.get(), row, 2, "ppb");
        row++;
      } break;
      //
      default:
        break;
      }
    }
  } else {
    M5_LOGE("null pointer");
  }
}

//
void Widget::Summary::event_draw_part_begin_callback(lv_event_t *event) {
  if (event) {
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
        dsc->rect_dsc->bg_color = lv_color_mix(lv_palette_main(LV_PALETTE_GREY),
                                               lv_color_white(), LV_OPA_30);
        dsc->rect_dsc->bg_opa = LV_OPA_COVER;
      } else {
        dsc->rect_dsc->bg_color = lv_color_white();
      }
    }
  }
}

//
//
//
Widget::Clock::Clock(Widget::InitArg init) : TileBase{init} {
  auto no_op_deleter = [](void *ptr) { /* nothing to do*/ };
  //
  if (_tile_obj) {
    // create
    _meter_obj.reset(lv_meter_create(_tile_obj.get()), lv_obj_del);
    if (_meter_obj) {
      auto size = std::min(lv_obj_get_content_width(_tile_obj.get()),
                           lv_obj_get_content_height(_tile_obj.get()));
      lv_obj_set_size(_meter_obj.get(), size * 0.9, size * 0.9);
      lv_obj_center(_meter_obj.get());
      //
      _sec_scale.reset(lv_meter_add_scale(_meter_obj.get()), no_op_deleter);
      if (_sec_scale) {
        lv_meter_set_scale_ticks(_meter_obj.get(), _sec_scale.get(), 61, 1, 10,
                                 lv_palette_main(LV_PALETTE_GREY));
        lv_meter_set_scale_range(_meter_obj.get(), _sec_scale.get(), 0, 60, 360,
                                 270);
        _sec_indic.reset(
            lv_meter_add_needle_line(_meter_obj.get(), _sec_scale.get(), 2,
                                     lv_palette_main(LV_PALETTE_RED), -10),
            no_op_deleter);
      }
      //
      _min_scale.reset(lv_meter_add_scale(_meter_obj.get()), no_op_deleter);
      if (_min_scale) {
        lv_meter_set_scale_ticks(_meter_obj.get(), _min_scale.get(), 61, 1, 10,
                                 lv_palette_main(LV_PALETTE_GREY));
        lv_meter_set_scale_range(_meter_obj.get(), _min_scale.get(), 0, 60, 360,
                                 270);
        _min_indic.reset(lv_meter_add_needle_line(
                             _meter_obj.get(), _min_scale.get(), 4,
                             lv_palette_darken(LV_PALETTE_INDIGO, 2), -25),
                         no_op_deleter);
      }
      //
      _hour_scale.reset(lv_meter_add_scale(_meter_obj.get()), no_op_deleter);
      if (_hour_scale) {
        lv_meter_set_scale_ticks(_meter_obj.get(), _hour_scale.get(), 12, 0, 0,
                                 lv_palette_main(LV_PALETTE_GREY));
        lv_meter_set_scale_major_ticks(_meter_obj.get(), _hour_scale.get(), 1,
                                       2, 20, lv_color_black(), 10);
        lv_meter_set_scale_range(_meter_obj.get(), _hour_scale.get(), 1, 12,
                                 330, 300);
        //
        _hour_indic.reset(lv_meter_add_needle_line(
                              _meter_obj.get(), _min_scale.get(), 9,
                              lv_palette_darken(LV_PALETTE_INDIGO, 2), -50),
                          no_op_deleter);
      }
    }
  } else {
    M5_LOGE("null pointer");
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
  if (_meter_obj && _hour_indic && _min_indic && _sec_indic) {
    lv_meter_set_indicator_value(_meter_obj.get(), _sec_indic.get(), tm.tm_sec);
    lv_meter_set_indicator_value(_meter_obj.get(), _min_indic.get(), tm.tm_min);
    const float h = (60.0f / 12.0f) * (tm.tm_hour % 12);
    const float m = (60.0f / 12.0f) * tm.tm_min / 60.0f;
    lv_meter_set_indicator_value(_meter_obj.get(), _hour_indic.get(),
                                 std::floor(h + m));
  } else {
    M5_LOGE("null pointer");
  }
}

//
//
//
Widget::SystemHealthy::SystemHealthy(Widget::InitArg init) : TileBase{init} {
  if (_tile_obj) {
    // create
    _table_obj.reset(lv_table_create(_tile_obj.get()), lv_obj_del);
    //
    auto w = lv_obj_get_content_width(_tile_obj.get());
    lv_table_set_col_width(_table_obj.get(), 0, w * 6 / 12);
    lv_table_set_col_width(_table_obj.get(), 1, w * 6 / 12);
    lv_obj_set_width(_table_obj.get(), w);
    lv_obj_set_height(_table_obj.get(),
                      lv_obj_get_content_height(_tile_obj.get()));
    lv_obj_center(_table_obj.get());
    lv_obj_set_style_text_font(_table_obj.get(), &lv_font_montserrat_14,
                               LV_PART_ITEMS | LV_STATE_DEFAULT);
    //
    lv_obj_add_event_cb(_table_obj.get(), event_draw_part_begin_callback,
                        LV_EVENT_DRAW_PART_BEGIN, nullptr);
  } else {
    M5_LOGE("null pointer");
    return;
  }
}

//
void Widget::SystemHealthy::render() {
  if (_table_obj) {
    auto row = 0;
    //
    const auto now = system_clock::now();
    //
    { // Battery Level
      lv_table_set_cell_value(_table_obj.get(), row, 0, "Battery Level");
      std::ostringstream oss;
      auto battery_level = M5.Power.getBatteryLevel();
      oss << std::setfill(' ') << std::setw(3) << +battery_level << "%";
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
    row++;
    { // Battery Charging?
      lv_table_set_cell_value(_table_obj.get(), row, 0, "Battery Status");
      switch (M5.Power.isCharging()) {
      case m5::Power_Class::is_charging_t::is_discharging:
        lv_table_set_cell_value(_table_obj.get(), row, 1, "discharging");
        break;
      case m5::Power_Class::is_charging_t::is_charging:
        lv_table_set_cell_value(_table_obj.get(), row, 1, "charging");
        break;
      case m5::Power_Class::is_charging_t::charge_unknown:
      [[fallthrough]]
      default:
        lv_table_set_cell_value(_table_obj.get(), row, 1, "unknown");
      }
    }
    row++;
    { // Battery Voltage
      lv_table_set_cell_value(_table_obj.get(), row, 0, "Battery Voltage");
      std::ostringstream oss;
      auto battery_voltage = M5.Power.getBatteryVoltage();
      oss << std::setfill(' ') << std::fixed << std::setw(5)
          << std::setprecision(3) << battery_voltage << "mV";
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
    row++;
    { // Battery Current
      lv_table_set_cell_value(_table_obj.get(), row, 0, "Battery Current");
      std::ostringstream oss;
      auto battery_current = M5.Power.getBatteryCurrent();
      oss << std::setw(5) << std::setprecision(3) << battery_current << "mA";
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
    row++;
    { // WiFi
      lv_table_set_cell_value(_table_obj.get(), row, 0, "WiFi");
      std::ostringstream oss;
      if (WiFi.status() == WL_CONNECTED) {
        oss << "connected" << std::endl << WiFi.localIP().toString().c_str();
      } else {
        oss << "No connection";
      }
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
    row++;
    { // Time is Synced?
      lv_table_set_cell_value(_table_obj.get(), row, 0, "Time Server");
      if (Application::isTimeSynced()) {
        lv_table_set_cell_value(_table_obj.get(), row, 1, "sync completed");
      } else {
        lv_table_set_cell_value(_table_obj.get(), row, 1, "NOT synced");
      }
    }
    row++;
    { // UTC time
      lv_table_set_cell_value(_table_obj.get(), row, 0, "Time(UTC)");
      std::ostringstream oss;
      std::time_t time =
          (Application::isTimeSynced()) ? system_clock::to_time_t(now) : 0;
      std::tm utc_time;
      gmtime_r(&time, &utc_time);
      oss << std::put_time(&utc_time, "%F\n%T UTC");
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
    row++;
    { // Local time
      lv_table_set_cell_value(_table_obj.get(), row, 0, "Time (Local)");
      std::ostringstream oss;
      std::time_t time =
          (Application::isTimeSynced()) ? system_clock::to_time_t(now) : 0;
      std::tm local_time;
      localtime_r(&time, &local_time);
      oss << std::put_time(&local_time, "%F\n%T %Z");
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
    row++;
    { // Uptime
      lv_table_set_cell_value(_table_obj.get(), row, 0, "Uptime");
      std::ostringstream oss;
      const seconds uptime = Application::uptime();
      const int16_t sec = uptime.count() % 60;
      const int16_t min = uptime.count() / 60 % 60;
      const int16_t hour = uptime.count() / (60 * 60) % 24;
      const int32_t days = uptime.count() / (60 * 60 * 24);
      oss << std::setfill(' ')           //
          << days << " days"             //
          << std::endl                   //
          << std::setfill('0')           //
          << std::setw(2) << hour << ":" //
          << std::setw(2) << min << ":"  //
          << std::setw(2) << sec;        //
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
    row++;
    { // Free Heap Memory
      lv_table_set_cell_value(_table_obj.get(), row, 0, "Free Heap");
      std::ostringstream oss;
      uint32_t memfree = esp_get_free_heap_size();
      oss << +memfree << "B";
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
    row++;
    { // Free Internal Heap Memory
      lv_table_set_cell_value(_table_obj.get(), row, 0, "Internal Free Heap");
      std::ostringstream oss;
      uint32_t free_heap = esp_get_free_internal_heap_size();
      oss << +free_heap << "B";
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
    row++;
    { // Minimum Free Heap Memory
      lv_table_set_cell_value(_table_obj.get(), row, 0, "Minimum Free Heap");
      std::ostringstream oss;
      uint32_t free_heap = esp_get_minimum_free_heap_size();
      oss << +free_heap << "B";
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
    row++;
    { // SPIRAM Used Percentage
      lv_table_set_cell_value(_table_obj.get(), row, 0, "SPIRAM Used");
      std::ostringstream oss;
      uint32_t total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
      uint32_t free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
      float_t slope = static_cast<float_t>(total_size - free_size) /
                      static_cast<float_t>(total_size);
      oss << std::setw(5) << std::setprecision(3) << +(slope * 100.0f) << "%";
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
    row++;
    { // SPIRAM Total Size
      lv_table_set_cell_value(_table_obj.get(), row, 0, "SPIRAM Total Size");
      std::ostringstream oss;
      uint32_t total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
      oss << +total_size << "B";
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
    row++;
    { // SPIRAM Free Size
      lv_table_set_cell_value(_table_obj.get(), row, 0, "SPIRAM Free Size");
      std::ostringstream oss;
      uint32_t free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
      oss << +free_size << "B";
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
    row++;
    { // SPIRAM Minimum Free Size
      lv_table_set_cell_value(_table_obj.get(), row, 0,
                              "SPIRAM Minimum Free Size");
      std::ostringstream oss;
      uint32_t minimum_free =
          heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
      oss << +minimum_free << "B";
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
    row++;
    { // SPIRAM Largest Free Block
      lv_table_set_cell_value(_table_obj.get(), row, 0,
                              "SPIRAM Largest Free Block");
      std::ostringstream oss;
      uint32_t largest_free =
          heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
      oss << +largest_free << "B";
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
    row++;
    { // LVGL task stack mark
      lv_table_set_cell_value(_table_obj.get(), row, 0,
                              "Stack High Mark(LVGL Task)");
      std::ostringstream oss;
      uint32_t stack_mark =
          uxTaskGetStackHighWaterMark(Application::getLvglTaskHandle());
      oss << +stack_mark << "B";
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
    row++;
    { // Application task stack mark
      lv_table_set_cell_value(_table_obj.get(), row, 0,
                              "Stack High Mark(Main Task)");
      std::ostringstream oss;
      auto stack_mark =
          uxTaskGetStackHighWaterMark(Application::getApplicationTaskHandle());
      oss << +stack_mark << "B";
      lv_table_set_cell_value(_table_obj.get(), row, 1, oss.str().c_str());
    }
  } else {
    M5_LOGE("null pointer");
  }
}

//
void Widget::SystemHealthy::event_draw_part_begin_callback(lv_event_t *event) {
  if (event) {
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
            lv_palette_main(LV_PALETTE_PURPLE), lv_color_white(), LV_OPA_10);
        dsc->rect_dsc->bg_opa = LV_OPA_COVER;
      } else {
        dsc->rect_dsc->bg_color = lv_color_white();
      }
    }
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
Widget::BasicChart<T>::BasicChart(std::shared_ptr<lv_obj_t> parent_obj,
                                  std::shared_ptr<lv_obj_t> title_obj) {
  if (parent_obj && title_obj) {
    // create
    _label_obj.reset(lv_label_create(parent_obj.get()), lv_obj_del);
    if (_label_obj) {
      const lv_font_t *FONT{&lv_font_montserrat_24};
      lv_obj_set_size(_label_obj.get(),
                      lv_obj_get_content_width(parent_obj.get()) - MARGIN * 2,
                      FONT->line_height);
      lv_obj_align_to(_label_obj.get(), title_obj.get(),
                      LV_ALIGN_OUT_BOTTOM_LEFT, 0, MARGIN);
      lv_label_set_long_mode(_label_obj.get(), LV_LABEL_LONG_SCROLL_CIRCULAR);
      lv_label_set_recolor(_label_obj.get(), true);
      lv_obj_set_style_text_font(_label_obj.get(), FONT, LV_STATE_DEFAULT);
      lv_style_init(&_label_style);
      lv_style_set_bg_opa(&_label_style, LV_OPA_COVER);
      lv_style_set_bg_color(&_label_style,
                            lv_palette_darken(LV_PALETTE_BROWN, 4));
      lv_style_set_text_color(&_label_style,
                              lv_palette_lighten(LV_PALETTE_BROWN, 4));
      lv_obj_add_style(_label_obj.get(), &_label_style, LV_PART_MAIN);
      lv_label_set_text(_label_obj.get(), "");
    }
    //
    _chart_obj.reset(lv_chart_create(parent_obj.get()), lv_obj_del);
    if (_chart_obj) {
      lv_obj_set_style_bg_color(_chart_obj.get(),
                                lv_palette_lighten(LV_PALETTE_LIGHT_GREEN, 2),
                                LV_PART_MAIN);
      //
      constexpr auto X_TICK_LABEL_LEN = 30;
      constexpr auto Y_TICK_LABEL_LEN = 60;
      constexpr auto RIGHT_PADDING = 20;
      lv_obj_set_size(_chart_obj.get(),
                      lv_obj_get_content_width(parent_obj.get())             //
                          - MARGIN - RIGHT_PADDING - Y_TICK_LABEL_LEN,       //
                      lv_obj_get_content_height(parent_obj.get())            //
                          - MARGIN * 2 - lv_obj_get_height(title_obj.get())  //
                          - MARGIN * 2 - lv_obj_get_height(_label_obj.get()) //
                          - MARGIN - X_TICK_LABEL_LEN);
      lv_obj_align_to(_chart_obj.get(), _label_obj.get(),
                      LV_ALIGN_OUT_BOTTOM_RIGHT, -MARGIN - RIGHT_PADDING,
                      MARGIN);
      if constexpr (true) {
        // Do not display points on the data
        lv_obj_set_style_size(_chart_obj.get(), 0, LV_PART_INDICATOR);
      }
      lv_chart_set_update_mode(_chart_obj.get(), LV_CHART_UPDATE_MODE_SHIFT);
      lv_chart_set_type(_chart_obj.get(), LV_CHART_TYPE_LINE);
      lv_chart_set_range(_chart_obj.get(), LV_CHART_AXIS_PRIMARY_X, 0,
                         Gui::CHART_X_POINT_COUNT);
      lv_chart_set_range(_chart_obj.get(), LV_CHART_AXIS_PRIMARY_Y, 0, 1);
      //
      lv_chart_set_axis_tick(_chart_obj.get(), LV_CHART_AXIS_PRIMARY_X, 8, 4,
                             X_AXIS_TICK_COUNT, 2, true, X_TICK_LABEL_LEN);
      lv_chart_set_axis_tick(_chart_obj.get(), LV_CHART_AXIS_PRIMARY_Y, 4, 2,
                             Y_AXIS_TICK_COUNT, 2, true, Y_TICK_LABEL_LEN);
      // callback
      lv_obj_add_event_cb(_chart_obj.get(), event_draw_part_begin_callback,
                          LV_EVENT_DRAW_PART_BEGIN, this);
      //
      for (const auto &sensor : Application::getSensors()) {
        if (sensor) {
          SensorId sensor_id{sensor->getSensorDescriptor()};
          auto opt_color = ChartLineColor::getAssignedColor(sensor_id);
          auto color = opt_color ? *opt_color : lv_color_black();
          auto series = lv_chart_add_series(_chart_obj.get(), color,
                                            LV_CHART_AXIS_PRIMARY_Y);
          if (series) {
            auto chart_series_wrapper =
                ChartSeriesWrapper{series, Gui::CHART_X_POINT_COUNT};
            lv_chart_set_ext_y_array(_chart_obj.get(), series,
                                     chart_series_wrapper.y_points.data());
            _chart_series_map.emplace(sensor_id,
                                      std::move(chart_series_wrapper));
          }
        }
      }
      //
      lv_chart_set_point_count(_chart_obj.get(), Gui::CHART_X_POINT_COUNT);
    }
  } else {
    M5_LOGE("null pointer");
  }
}

//
template <typename T>
void Widget::BasicChart<T>::event_draw_part_begin_callback(lv_event_t *event) {
  if (event) {
    auto it = static_cast<Widget::BasicChart<T> *>(event->user_data);
    if (it == nullptr) {
      M5_LOGE("user_data had null");
    }
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(event);
    if (lv_obj_draw_part_check_type(dsc, &lv_chart_class,
                                    LV_CHART_DRAW_PART_TICK_LABEL)) {
      it->chart_draw_part_tick_label(dsc);
    }
  }
}

//
template <typename T> void Widget::BasicChart<T>::render() {
  if (_chart_obj) {
    // 初期化
    for (auto &pair : _chart_series_map) {
      pair.second.fill(LV_CHART_POINT_NONE);
    }
    //
    system_clock::time_point now_min = floor<minutes>(system_clock::now());
    _begin_x_tp = now_min - minutes(Gui::CHART_X_POINT_COUNT);

    // データーベースより測定データーを得て
    // 各々のsensoridのchart seriesにセットする関数
    auto coordinateChartSeries = [this](size_t counter, DataType item) -> bool {
      auto &[sensorid, tp, value] = item;
      // 各々のsensoridのchart seriesにセットする。
      if (auto found_itr = _chart_series_map.find(sensorid);
          found_itr != _chart_series_map.end()) {
        auto coord = coordinateXY(_begin_x_tp, item);
        M5_LOGV("%d,%d", coord.x, coord.y);
        found_itr->second.y_points.at(coord.x) = coord.y;
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
    if (read_measurements_from_database(Database::OrderByAtAsc, _begin_x_tp,
                                        coordinateChartSeries) >= 1) {
      // 下限、上限
      auto y_min = std::numeric_limits<lv_coord_t>::max();
      auto y_max = std::numeric_limits<lv_coord_t>::min();
      for (auto &pair : _chart_series_map) {
        auto [min, max] = pair.second.getMinMaxOfYPoints();
        y_min = std::min(y_min, min);
        y_max = std::max(y_max, max);
      }
      //
      if (y_min == std::numeric_limits<lv_coord_t>::max() &&
          y_max == std::numeric_limits<lv_coord_t>::min()) {
        lv_chart_set_range(_chart_obj.get(), LV_CHART_AXIS_PRIMARY_Y, 0, 1);
      } else {
        // 5刻みにまるめる
        y_min = floor(y_min / 500.0f) * 500.0f;
        y_max = ceil(y_max / 500.0f) * 500.0f;
        lv_chart_set_range(_chart_obj.get(), LV_CHART_AXIS_PRIMARY_Y, y_min,
                           y_max);
        M5_LOGV("y_min:%d, y_max:%d", y_min, y_max);
      }
    }
    //
    lv_chart_refresh(_chart_obj.get());
  } else {
    M5_LOGE("null pointer");
  }
}

//
// 気温
//
Widget::TemperatureChart::TemperatureChart(InitArg init) : TileBase{init} {
  if (_tile_obj) {
    // create
    _title_obj.reset(lv_label_create(_tile_obj.get()), lv_obj_del);
    if (_title_obj) {
      lv_obj_set_style_text_align(_title_obj.get(), LV_TEXT_ALIGN_LEFT,
                                  LV_STATE_DEFAULT);
      lv_obj_set_style_text_font(_title_obj.get(), &lv_font_montserrat_16,
                                 LV_STATE_DEFAULT);
      lv_obj_set_size(_title_obj.get(), LV_PCT(100),
                      lv_font_montserrat_16.line_height);
      lv_obj_align(_title_obj.get(), LV_ALIGN_TOP_LEFT, BC::MARGIN, BC::MARGIN);
      lv_label_set_text(_title_obj.get(), "Temperature");
    } else {
      M5_LOGE("null pointer");
    }
  } else {
    M5_LOGE("null pointer");
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
    auto label_obj = basic_chart->getLabelObj();
    if (label_obj) {
      lv_label_set_text(label_obj.get(), oss.str().c_str());
    }
    //
    basic_chart->render();
  }
}

// データーベースからデーターを得る
size_t Widget::TemperatureChart::BC::read_measurements_from_database(
    Database::OrderBy order, system_clock::time_point at_begin,
    Database::ReadCallback<Database::TimePointAndDouble> callback) {
  return Application::getMeasurementsDatabase().read_temperatures(
      order, at_begin, callback);
}

// データを座標に変換する関数
lv_point_t Widget::TemperatureChart::BC::coordinateXY(
    system_clock::time_point tp_zero, const Database::TimePointAndDouble &in) {
  auto [sensorid, timepoint, fp_value] = in;
  auto degc = DegC(fp_value);
  auto centi_degc = std::chrono::duration_cast<CentiDegC>(degc);
  uint32_t timediff =
      duration_cast<minutes>(floor<minutes>(timepoint) - tp_zero).count();
  lv_coord_t x =
      std::clamp(timediff, 0U, uint32_t(Gui::CHART_X_POINT_COUNT - 1));
  lv_coord_t y = static_cast<lv_coord_t>(centi_degc.count());
  return lv_point_t{.x = x, .y = y};
};

//
void Widget::TemperatureChart::BC::chart_draw_part_tick_label(
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
      lv_snprintf(dsc->text, dsc->text_length, "%02d/%02d %02d:%02d",
                  local_time.tm_mon, local_time.tm_mday, local_time.tm_hour,
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
  if (_tile_obj) {
    // create
    _title_obj.reset(lv_label_create(_tile_obj.get()), lv_obj_del);
    if (_title_obj) {
      lv_obj_set_style_text_align(_title_obj.get(), LV_TEXT_ALIGN_LEFT,
                                  LV_STATE_DEFAULT);
      lv_obj_set_style_text_font(_title_obj.get(), &lv_font_montserrat_16,
                                 LV_STATE_DEFAULT);
      lv_obj_set_size(_title_obj.get(), LV_PCT(100),
                      lv_font_montserrat_16.line_height);
      lv_obj_align(_title_obj.get(), LV_ALIGN_TOP_LEFT, BC::MARGIN, BC::MARGIN);
      lv_label_set_text(_title_obj.get(), "Relative Humidity");
    } else {
      M5_LOGE("null pointer");
    }
  } else {
    M5_LOGE("null pointer");
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
    auto label_obj = basic_chart->getLabelObj();
    if (label_obj) {
      lv_label_set_text(label_obj.get(), oss.str().c_str());
    }
    //
    basic_chart->render();
  }
}

// データーベースからデーターを得る
size_t Widget::RelativeHumidityChart::BC::read_measurements_from_database(
    Database::OrderBy order, system_clock::time_point at_begin,
    Database::ReadCallback<Database::TimePointAndDouble> callback) {
  return Application::getMeasurementsDatabase().read_relative_humidities(
      order, at_begin, callback);
}

// データを座標に変換する関数
lv_point_t Widget::RelativeHumidityChart::BC::coordinateXY(
    system_clock::time_point tp_zero, const Database::TimePointAndDouble &in) {
  auto [sensorid, timepoint, fp_value] = in;
  auto percentRelativeHumidity = PctRH(fp_value);
  auto rh = std::chrono::duration_cast<CentiRH>(percentRelativeHumidity);
  uint32_t timediff =
      duration_cast<minutes>(floor<minutes>(timepoint) - tp_zero).count();
  lv_coord_t x =
      std::clamp(timediff, 0U, uint32_t(Gui::CHART_X_POINT_COUNT - 1));
  lv_coord_t y = static_cast<lv_coord_t>(rh.count());
  return lv_point_t{.x = x, .y = y};
};

void Widget::RelativeHumidityChart::BC::chart_draw_part_tick_label(
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
      lv_snprintf(dsc->text, dsc->text_length, "%02d/%02d %02d:%02d",
                  local_time.tm_mon, local_time.tm_mday, local_time.tm_hour,
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
  if (_tile_obj) {
    // create
    _title_obj.reset(lv_label_create(_tile_obj.get()), lv_obj_del);
    if (_title_obj) {
      lv_obj_set_style_text_align(_title_obj.get(), LV_TEXT_ALIGN_LEFT,
                                  LV_STATE_DEFAULT);
      lv_obj_set_style_text_font(_title_obj.get(), &lv_font_montserrat_16,
                                 LV_STATE_DEFAULT);
      lv_obj_set_size(_title_obj.get(), LV_PCT(100),
                      lv_font_montserrat_16.line_height);
      lv_obj_align(_title_obj.get(), LV_ALIGN_TOP_LEFT, BC::MARGIN, BC::MARGIN);
      lv_label_set_text(_title_obj.get(), "Pressure");
    } else {
      M5_LOGE("null pointer");
    }
  } else {
    M5_LOGE("null pointer");
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
    auto label_obj = basic_chart->getLabelObj();
    if (label_obj) {
      lv_label_set_text(label_obj.get(), oss.str().c_str());
    }
    //
    basic_chart->render();
  }
}

// データーベースからデーターを得る
size_t Widget::PressureChart::BC::read_measurements_from_database(
    Database::OrderBy order, system_clock::time_point at_begin,
    Database::ReadCallback<Database::TimePointAndDouble> callback) {
  return Application::getMeasurementsDatabase().read_pressures(order, at_begin,
                                                               callback);
}

// データを座標に変換する関数
lv_point_t Widget::PressureChart::BC::coordinateXY(
    system_clock::time_point tp_zero, const Database::TimePointAndDouble &in) {
  auto [sensorid, timepoint, fp_value] = in;
  auto hpa = HectoPa(fp_value);
  auto pascal = std::chrono::duration_cast<Pascal>(hpa - BIAS);
  uint32_t timediff =
      duration_cast<minutes>(floor<minutes>(timepoint) - tp_zero).count();
  lv_coord_t x =
      std::clamp(timediff, 0U, uint32_t(Gui::CHART_X_POINT_COUNT - 1));
  lv_coord_t y = static_cast<lv_coord_t>(pascal.count());
  return lv_point_t{.x = x, .y = y};
};

//
void Widget::PressureChart::BC::chart_draw_part_tick_label(
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
      lv_snprintf(dsc->text, dsc->text_length, "%02d/%02d %02d:%02d",
                  local_time.tm_mon, local_time.tm_mday, local_time.tm_hour,
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
  if (_tile_obj) {
    // create
    _title_obj.reset(lv_label_create(_tile_obj.get()), lv_obj_del);
    if (_title_obj) {
      lv_obj_set_style_text_align(_title_obj.get(), LV_TEXT_ALIGN_LEFT,
                                  LV_STATE_DEFAULT);
      lv_obj_set_style_text_font(_title_obj.get(), &lv_font_montserrat_16,
                                 LV_STATE_DEFAULT);
      lv_obj_set_size(_title_obj.get(), LV_PCT(100),
                      lv_font_montserrat_16.line_height);
      lv_obj_align(_title_obj.get(), LV_ALIGN_TOP_LEFT, BC::MARGIN, BC::MARGIN);
      lv_label_set_text(_title_obj.get(), "CO2");
    } else {
      M5_LOGE("null pointer");
    }
  } else {
    M5_LOGE("null pointer");
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
    auto label_obj = basic_chart->getLabelObj();
    if (label_obj) {
      lv_label_set_text(label_obj.get(), oss.str().c_str());
    }
    //
    basic_chart->render();
  }
}

// データーベースからデーターを得る
size_t Widget::CarbonDeoxidesChart::BC::read_measurements_from_database(
    Database::OrderBy order, system_clock::time_point at_begin,
    Database::ReadCallback<Database::TimePointAndUInt16> callback) {
  return Application::getMeasurementsDatabase().read_carbon_deoxides(
      order, at_begin, callback);
}

// データを座標に変換する関数
lv_point_t Widget::CarbonDeoxidesChart::BC::coordinateXY(
    system_clock::time_point tp_zero, const Database::TimePointAndUInt16 &in) {
  auto [sensorid, timepoint, int_value] = in;
  uint32_t timediff =
      duration_cast<minutes>(floor<minutes>(timepoint) - tp_zero).count();
  lv_coord_t x =
      std::clamp(timediff, 0U, uint32_t(Gui::CHART_X_POINT_COUNT - 1));
  lv_coord_t y = static_cast<lv_coord_t>(int_value);
  return lv_point_t{.x = x, .y = y};
};

//
void Widget::CarbonDeoxidesChart::BC::chart_draw_part_tick_label(
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
      lv_snprintf(dsc->text, dsc->text_length, "%02d/%02d %02d:%02d",
                  local_time.tm_mon, local_time.tm_mday, local_time.tm_hour,
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
  if (_tile_obj) {
    // create
    _title_obj.reset(lv_label_create(_tile_obj.get()), lv_obj_del);
    if (_title_obj) {
      lv_obj_set_style_text_align(_title_obj.get(), LV_TEXT_ALIGN_LEFT,
                                  LV_STATE_DEFAULT);
      lv_obj_set_style_text_font(_title_obj.get(), &lv_font_montserrat_16,
                                 LV_STATE_DEFAULT);
      lv_obj_set_size(_title_obj.get(), LV_PCT(100),
                      lv_font_montserrat_16.line_height);
      lv_obj_align(_title_obj.get(), LV_ALIGN_TOP_LEFT, BC::MARGIN, BC::MARGIN);
      lv_label_set_text(_title_obj.get(), "Total VOC");
    } else {
      M5_LOGE("null pointer");
    }
  } else {
    M5_LOGE("null pointer");
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
    auto label_obj = basic_chart->getLabelObj();
    if (label_obj) {
      lv_label_set_text(label_obj.get(), oss.str().c_str());
    }
    //
    basic_chart->render();
  }
}

// データーベースからデーターを得る
size_t Widget::TotalVocChart::BC::read_measurements_from_database(
    Database::OrderBy order, system_clock::time_point at_begin,
    Database::ReadCallback<Database::TimePointAndUInt16> callback) {
  return Application::getMeasurementsDatabase().read_total_vocs(order, at_begin,
                                                                callback);
}

// データを座標に変換する関数
lv_point_t Widget::TotalVocChart::BC::coordinateXY(
    system_clock::time_point tp_zero, const Database::TimePointAndUInt16 &in) {
  auto [sensorid, timepoint, int_value] = in;
  uint32_t timediff =
      duration_cast<minutes>(floor<minutes>(timepoint) - tp_zero).count();
  lv_coord_t x =
      std::clamp(timediff, 0U, uint32_t(Gui::CHART_X_POINT_COUNT - 1));
  lv_coord_t y = static_cast<lv_coord_t>(int_value / DIVIDER);
  return lv_point_t{.x = x, .y = y};
};

//
void Widget::TotalVocChart::BC::chart_draw_part_tick_label(
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
      lv_snprintf(dsc->text, dsc->text_length, "%02d/%02d %02d:%02d",
                  local_time.tm_mon, local_time.tm_mday, local_time.tm_hour,
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
