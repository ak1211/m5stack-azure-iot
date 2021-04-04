// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef SCREEN_HPP
#define SCREEN_HPP

#include "bme280_sensor.hpp"
#include "local_database.hpp"
#include "scd30_sensor.hpp"
#include "sgp30_sensor.hpp"
#include "system_status.hpp"

#include <M5Core2.h>
#include <ctime>

#include <LovyanGFX.hpp>

class Screen {
public:
  //
  class View {
  public:
    //
    View(int32_t text_color, int32_t bg_color)
        : message_text_color(text_color), background_color(bg_color) {}
    //
    virtual ~View() {}
    //
    virtual bool focusIn() {
      Screen::lcd.setBaseColor(background_color);
      Screen::lcd.setTextColor(message_text_color, background_color);
      Screen::lcd.clear();
      return true;
    }
    //
    virtual void focusOut() {}
    //
    virtual void render(const System::Status &status, const struct tm &local,
                        const Bme280::TempHumiPres *bme,
                        const Sgp30::TvocEco2 *sgp,
                        const Scd30::Co2TempHumi *scd) {}
    //
    int32_t message_text_color;
    int32_t background_color;
  };
  //
  static LGFX lcd;
  //
  Screen(LocalDatabase &local_database);
  ~Screen() {
    for (int i = 0; i < total_views; i++) {
      delete views[i];
    }
  }
  //
  void begin(int32_t text_color = TFT_WHITE, int32_t bg_color = TFT_BLACK);
  //
  void update(const System::Status &status, time_t time,
              const Bme280::TempHumiPres *bme, const Sgp30::TvocEco2 *sgp,
              const Scd30::Co2TempHumi *scd);
  //
  void home();
  void prev();
  void next();

private:
  static constexpr int8_t total_views = 9;
  View *views[total_views];
  int8_t now_view;
  //
};

#endif // SCREEN_HPP
