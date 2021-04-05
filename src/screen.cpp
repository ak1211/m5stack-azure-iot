// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#define _USE_MATH_DEFINES
#include "screen.hpp"
#include "local_database.hpp"
#include "system_status.hpp"

#include <M5Core2.h>
#include <cmath>
#include <ctime>
#include <queue>

#include <LovyanGFX.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Coord {
  int16_t x;
  int16_t y;
};

//
class SystemHealthView : public Screen::View {
public:
  SystemHealthView() : View(TFT_WHITE, TFT_BLACK) {}
  //
  void render(const System::Status &status, const struct tm &local,
              const Bme280::TempHumiPres *bme, const Sgp30::TvocEco2 *sgp,
              const Scd30::Co2TempHumi *scd) override {
    //
    Screen::lcd.setCursor(0, 0);
    Screen::lcd.setTextDatum(textdatum_t::top_left);
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    if (status.has_WIFI_connection) {
      Screen::lcd.setTextColor(TFT_GREEN, background_color);
      Screen::lcd.printf("  Wifi");
    } else {
      Screen::lcd.setTextColor(TFT_RED, background_color);
      Screen::lcd.printf("NoWifi");
    }
    Screen::lcd.setTextColor(message_text_color, background_color);
    //
    if (status.is_freestanding_mode) {
      Screen::lcd.printf("/FREESTAND");
    } else {
      Screen::lcd.printf("/CONNECTED");
    }
    //
    {
      auto up = uptime(status);
      Screen::lcd.printf(" up %2ddays,%2d:%2d\n", up.days, up.hours,
                         up.minutes);
    }
    //
    auto batt_info = System::getBatteryStatus();
    char sign = ' ';
    if (System::isBatteryCharging(batt_info)) {
      sign = '+';
    } else if (System::isBatteryDischarging(batt_info)) {
      sign = '-';
    } else {
      sign = ' ';
    }
    Screen::lcd.printf("Batt %4.0f%% %4.2fV %c%5.3fA", batt_info.percentage,
                       batt_info.voltage, sign,
                       abs(batt_info.current / 1000.0f));
    Screen::lcd.print("\n");
    //
    {
      char date_time[50] = "";
      strftime(date_time, sizeof(date_time), "%Y-%m-%d %H:%M:%SJST", &local);
      Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
      Screen::lcd.printf("%s\n", date_time);
    }
    //
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    if (bme) {
      Screen::lcd.printf("温度 %6.1f ℃\n", bme->temperature);
      Screen::lcd.printf("湿度 %6.1f ％\n", bme->relative_humidity);
      Screen::lcd.printf("気圧 %6.1f hPa\n", bme->pressure);
    }
    if (sgp) {
      Screen::lcd.printf("eCO2 %6d ppm\n", sgp->eCo2);
      Screen::lcd.printf("TVOC %6d ppb\n", sgp->tvoc);
    }
    if (scd) {
      Screen::lcd.printf("CO2 %6d ppm\n", scd->co2);
      Screen::lcd.printf("温度 %6.1f ℃\n", scd->temperature);
      Screen::lcd.printf("湿度 %6.1f ％\n", scd->relative_humidity);
    }
  }

private:
};

//
class ClockView : public Screen::View {
public:
  ClockView() : View(TFT_WHITE, TFT_BLACK) {}
  //
  void render(const System::Status &, const struct tm &local,
              const Bme280::TempHumiPres *, const Sgp30::TvocEco2 *,
              const Scd30::Co2TempHumi *) override {
    const auto half_width = Screen::lcd.width() / 2;
    const auto half_height = Screen::lcd.height() / 2;
    const auto line_height = 40;

    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_32);

    // 月日表示
    {
      char buffer[16];
      sprintf(buffer, "%2d月%2d日", local.tm_mon + 1, local.tm_mday);
      Screen::lcd.drawString(buffer, half_width, half_height - line_height);
      // 時分表示
      sprintf(buffer, "%2d時%2d分", local.tm_hour, local.tm_min);
      Screen::lcd.drawString(buffer, half_width, half_height + line_height);
    }
    // 秒表示
    constexpr int dot_radius = 4;
    const int16_t radius =
        static_cast<int16_t>(floor(min(half_width, half_height) * 0.9));
    //
    uint32_t bgcol = Screen::lcd.color888(77, 77, 77);
    uint32_t fgcol = Screen::lcd.color888(232, 107, 107);
    //
    for (int s = 0; s < 60; s++) {
      Coord p = calculate_pos(radius, s);
      auto w = dot_radius + dot_radius + 1;
      Screen::lcd.fillRect(p.x - dot_radius, p.y - dot_radius, w, w,
                           background_color);
    }
    //
    Screen::lcd.drawCircle(half_width, half_height, radius, bgcol);
    //
    Coord p = calculate_pos(radius, local.tm_sec);
    Screen::lcd.fillCircle(p.x, p.y, dot_radius, fgcol);
  }

private:
  //
  Coord calculate_pos(int r, int tm_sec) {
    const auto half_width = Screen::lcd.width() / 2;
    const auto half_height = Screen::lcd.height() / 2;
    constexpr double r90degrees = M_PI / 2.0;
    constexpr double delta_radian = 2.0 * M_PI / 60.0;
    const double rad =
        delta_radian * static_cast<double>(60 - tm_sec) + r90degrees;
    return {
        .x = static_cast<int16_t>(half_width + r * cos(rad)),
        .y = static_cast<int16_t>(half_height - r * sin(rad)),
    };
  };
};

//
void tile(const char *title, float now, float *before, const char *unit,
          int16_t sx, int16_t sy, int16_t width, int16_t height,
          int32_t text_col, int32_t bg_col) {
  if (now == *before) {
    return;
  }
  int16_t cx = sx + width / 2;
  int16_t cy = sy + height / 2;
  Screen::lcd.setFont(&fonts::lgfxJapanGothic_32);
  Screen::lcd.setTextColor(bg_col, bg_col);
  Screen::lcd.drawFloat(*before, 1, cx, cy);
  Screen::lcd.setTextColor(text_col, bg_col);
  Screen::lcd.drawFloat(now, 1, cx, cy);
  //
  *before = now;
  //
  Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
  Screen::lcd.drawString(title, cx - 32, cy - 32);
  Screen::lcd.drawString(unit, cx + 32, cy + 32);
}
//
void tile(const char *title, uint16_t now, uint16_t *before, const char *unit,
          int16_t sx, int16_t sy, int16_t width, int16_t height,
          int32_t text_col, int32_t bg_col) {
  if (now == *before) {
    return;
  }
  int16_t cx = sx + width / 2;
  int16_t cy = sy + height / 2;
  Screen::lcd.setFont(&fonts::lgfxJapanGothic_32);
  Screen::lcd.setTextColor(bg_col, bg_col);
  Screen::lcd.drawNumber(*before, cx, cy);
  Screen::lcd.setTextColor(text_col, bg_col);
  Screen::lcd.drawNumber(now, cx, cy);
  //
  *before = now;
  //
  Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
  Screen::lcd.drawString(title, cx - 32, cy - 32);
  Screen::lcd.drawString(unit, cx + 32, cy + 32);
}

//
class SummaryView : public Screen::View {
public:
  //
  class Tile {
  public:
    char caption[7];
    char unit[7];
    char before[7];
    char now[7];
    int32_t text_color;
    int32_t background_color;
    Coord left_top;
    int16_t width;
    int16_t height;
    Tile(const char *caption_, const char *unit_, int32_t text_col,
         int32_t bg_col) {
      strncpy(caption, caption_, sizeof(caption));
      strncpy(unit, unit_, sizeof(unit));
      strncpy(before, "", sizeof(before));
      strncpy(now, "", sizeof(now));
      text_color = text_col;
      background_color = bg_col;
    }
    //
    Tile &setCoord(Coord xy, int16_t w, int16_t h) {
      left_top = xy;
      width = w;
      height = h;
      return *this;
    }
    //
    Tile &prepare() {
      int16_t cx = left_top.x + width / 2;
      int16_t cy = left_top.y + height / 2;
      Screen::lcd.fillRect(left_top.x, left_top.y, width, height,
                           background_color);
      //
      Screen::lcd.setTextDatum(textdatum_t::middle_center);
      Screen::lcd.setTextColor(text_color, background_color);
      Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
      Screen::lcd.drawString(caption, cx - 32, cy - 32);
      Screen::lcd.drawString(unit, cx + 32, cy + 32);
      return *this;
    }
    //
    void render_notavailable() {
      strncpy(now, "N/A", sizeof(now));
      render_sub();
    }
    //
    void render_value_uint16(uint16_t v) {
      snprintf(now, sizeof(now), "%d", v);
      render_sub();
    }
    //
    void render_value_float(float v) {
      snprintf(now, sizeof(now), "%.1f", v);
      render_sub();
    }

  private:
    void render_sub() {
      if (strcmp(now, before) == 0) {
        return;
      }
      int16_t cx = left_top.x + width / 2;
      int16_t cy = left_top.y + height / 2;
      Screen::lcd.setFont(&fonts::lgfxJapanGothic_32);
      Screen::lcd.setTextDatum(textdatum_t::middle_center);
      Screen::lcd.setTextColor(background_color, background_color);
      Screen::lcd.drawString(before, cx, cy);
      Screen::lcd.setTextColor(text_color, background_color);
      Screen::lcd.drawString(now, cx, cy);
      //
      memcpy(before, now, sizeof(before));
    }
  };
  //
  SummaryView()
      : View(TFT_WHITE, TFT_BLACK),
        tile_temperature("温度", "℃", message_text_color, background_color),
        tile_relative_humidity("湿度", "%RH", message_text_color,
                               background_color),
        tile_pressure("気圧", "hPa", message_text_color, background_color),
        tile_tvoc("TVOC", "ppb", message_text_color, background_color),
        tile_eCo2("eCO2", "ppm", message_text_color, background_color),
        tile_co2("CO2", "ppm", message_text_color, background_color) {}
  //
  bool focusIn() override {
    const int16_t w = Screen::lcd.width() / 3;
    const int16_t h = Screen::lcd.height() / 2;
    //
    const Coord c1 = {.x = 0, .y = 0};
    const Coord c2 = {.x = w, .y = 0};
    const Coord c3 = {.x = w + w, .y = 0};
    const Coord c4 = {.x = 0, .y = h};
    const Coord c5 = {.x = w, .y = h};
    const Coord c6 = {.x = w + w, .y = h};
    //
    if (Screen::View::focusIn()) {
      tile_temperature.setCoord(c1, w, h).prepare();
      tile_relative_humidity.setCoord(c2, w, h).prepare();
      tile_pressure.setCoord(c3, w, h).prepare();
      tile_tvoc.setCoord(c4, w, h).prepare();
      tile_eCo2.setCoord(c5, w, h).prepare();
      tile_co2.setCoord(c6, w, h).prepare();
      return true;
    } else {
      return false;
    }
  }
  //
  void render(const System::Status &status, const struct tm &local,
              const Bme280::TempHumiPres *bme, const Sgp30::TvocEco2 *sgp,
              const Scd30::Co2TempHumi *scd) override {

    if (bme) {
      tile_temperature.render_value_float(bme->temperature);
      tile_relative_humidity.render_value_float(bme->relative_humidity);
      tile_pressure.render_value_float(bme->pressure);
    } else {
      tile_temperature.render_notavailable();
      tile_relative_humidity.render_notavailable();
      tile_pressure.render_notavailable();
    }
    if (sgp) {
      tile_tvoc.render_value_uint16(sgp->tvoc);
      tile_eCo2.render_value_uint16(sgp->eCo2);
    } else {
      tile_tvoc.render_notavailable();
      tile_eCo2.render_notavailable();
    }
    if (scd) {
      tile_co2.render_value_uint16(scd->co2);
    } else {
      tile_co2.render_notavailable();
    }
  }

private:
  Tile tile_temperature;
  Tile tile_relative_humidity;
  Tile tile_pressure;
  Tile tile_tvoc;
  Tile tile_eCo2;
  Tile tile_co2;
};

//
//
//
class GraphView : public Screen::View {
public:
  const uint32_t grid_color = Screen::lcd.color888(200, 200, 200);
  const uint32_t line_color = Screen::lcd.color888(232, 120, 120);
  //
  static constexpr int16_t graph_margin = 8;
  static constexpr int16_t graph_width = 240;
  static constexpr int16_t graph_height = 180;
  //
  GraphView(int32_t text_color, int32_t bg_color, LocalDatabase &local_database,
            double vmin, double vmax)
      : View(text_color, bg_color), database(local_database), value_min(vmin),
        value_max(vmax) {
    //
    setOffset(0, 0);
    locators.push_back(value_min);
    locators.push_back(value_max);
  }
  //
  inline void setOffset(int16_t x, int16_t y) {
    graph_offset_x = x;
    graph_offset_y = y;
  }
  //
  inline void getGraphLeft(int16_t *lx, int16_t *ly) {
    if (lx)
      *lx = graph_offset_x + graph_margin;
    if (ly)
      *ly = graph_offset_y + graph_margin;
  }
  //
  inline void getGraphRight(int16_t *rx, int16_t *ry) {
    if (rx)
      *rx = graph_offset_x + graph_margin + graph_width;
    if (ry)
      *ry = graph_offset_y + graph_margin + graph_height;
  }
  //
  inline void getGraphCenter(int16_t *cx, int16_t *cy) {
    if (cx)
      *cx = graph_offset_x + graph_margin + graph_width / 2;
    if (cy)
      *cy = graph_offset_y + graph_margin + graph_height / 2;
  }
  //
  inline double normalize_value(double value) {
    double n = (value - value_min) / (value_max - value_min);
    return n;
  }
  //
  inline double clipping_value(double value) {
    return max(min(value, value_max), value_min);
  }
  //
  inline int16_t coord_y(double value) {
    int16_t y = graph_height * normalize_value(clipping_value(value));
    return (graph_offset_y + graph_margin + graph_height - y);
  }
  //
  void grid() {
    int16_t sx, sy, ex, ey;
    getGraphLeft(&sx, &sy);
    getGraphRight(&ex, &ey);

    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    Screen::lcd.setTextColor(message_text_color);
    Screen::lcd.setTextDatum(textdatum_t::middle_right);
    //
    for (auto value : locators) {
      int16_t y = coord_y(value);
      Screen::lcd.drawFastHLine(sx, y, graph_width, grid_color);
      Screen::lcd.drawFloat(value, 1, graph_offset_x, y);
    }
  }
  //
  void progressBar(uint16_t current, uint16_t min, uint16_t max) {
    const int16_t w = Screen::lcd.width();
    const int16_t h = Screen::lcd.height();
    const int16_t cx = w / 2;
    const int16_t cy = h / 2;
    //
    uint32_t bgcol = Screen::lcd.color888(77, 77, 77);
    uint32_t fgcol = Screen::lcd.color888(77, 107, 233);
    //
    uint16_t percentage = 100 * (current - min) / (max - min);
    uint16_t distance = percentage * w / 100;
    Screen::lcd.fillRect(0, cy - 16, distance, 32, fgcol);
    Screen::lcd.fillRect(distance, cy - 16, w - distance, 32, bgcol);
    //
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_32);
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    Screen::lcd.setTextColor(message_text_color);
    Screen::lcd.drawNumber(percentage, cx, cy);
  }

protected:
  LocalDatabase &database;
  int16_t graph_offset_x;
  int16_t graph_offset_y;
  const double value_min;
  const double value_max;
  std::vector<double> locators;
};

//
class TemperatureGraphView : public GraphView {
public:
  TemperatureGraphView(LocalDatabase &local_database)
      : GraphView(TFT_WHITE, TFT_BLACK, local_database, -10.0, 60.0) {
    locators.push_back(0.0);
    locators.push_back(20.0);
    locators.push_back(40.0);
  }
  //
  bool focusIn() override {
    setOffset(Screen::lcd.width() - (graph_width + graph_margin * 2) - 1,
              Screen::lcd.height() - (graph_height + graph_margin * 2) - 1);
    //
    init_rawid();
    if (!database.healthy()) {
      return false;
    }
    if (Screen::View::focusIn()) {
      prepare();
      return true;
    } else {
      return false;
    }
  }
  //
  void prepare() {
    grid();
    //
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    Screen::lcd.setTextDatum(textdatum_t::top_left);
    Screen::lcd.drawString("温度 ℃", 3, 3);
    //
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
  }
  //
  void render(const System::Status &, const struct tm &local,
              const Bme280::TempHumiPres *bme, const Sgp30::TvocEco2 *,
              const Scd30::Co2TempHumi *) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)まで残り45秒以上ある場合だけ,
    // この後の作業を開始する。
    //
    if (60 - local.tm_sec >= 45) {
      const int16_t y_top = coord_y(value_max);
      const int16_t y_bottom = coord_y(value_min);
      const int16_t y_length = abs(y_bottom - y_top);
      const int16_t y0 = coord_y(0.0);
      const int8_t step = 4;
      int16_t rx;
      getGraphRight(&rx, nullptr);
      int16_t x = step;
      // 最新のデータを得る
      auto callback = [&](size_t counter, time_t at, float degc) {
        // 消去
        Screen::lcd.fillRect(rx - x, y_top, step, y_length, background_color);
        // 書き込む
        int16_t y = coord_y(degc);
        Screen::lcd.fillRect(rx - x, y, step / 2, abs(y - y0), line_color);
        x += step;
        return true;
      };
      //
      prepare();
      database.get_temperatures_desc(bme->sensor_id, graph_width / step,
                                     callback);
      rawid = database.rawid_temperature;
      grid();
    }
  }

private:
  int64_t rawid;
  //
  inline void init_rawid() { rawid = INT64_MIN; }
  inline void update_rawid() { rawid = database.rawid_temperature; }
  inline bool need_for_update() { return rawid != database.rawid_temperature; }
};

//
class RelativeHumidityGraphView : public GraphView {
public:
  RelativeHumidityGraphView(LocalDatabase &local_database)
      : GraphView(TFT_WHITE, TFT_BLACK, local_database, 0.0, 100.0) {
    locators.push_back(25.0);
    locators.push_back(50.0);
    locators.push_back(75.0);
  }
  //
  bool focusIn() override {
    setOffset(Screen::lcd.width() - (graph_width + graph_margin * 2) - 1,
              Screen::lcd.height() - (graph_height + graph_margin * 2) - 1);
    //
    init_rawid();
    if (!database.healthy()) {
      return false;
    }
    if (Screen::View::focusIn()) {
      prepare();
      return true;
    } else {
      return false;
    }
  }
  //
  void prepare() {
    grid();
    //
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    Screen::lcd.setTextDatum(textdatum_t::top_left);
    Screen::lcd.drawString("相対湿度 %RH", 3, 3);
    //
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
  }
  //
  void render(const System::Status &, const struct tm &local,
              const Bme280::TempHumiPres *bme, const Sgp30::TvocEco2 *,
              const Scd30::Co2TempHumi *) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)まで残り45秒以上ある場合だけ,
    // この後の作業を開始する。
    //
    if (60 - local.tm_sec >= 45) {
      const int16_t y_top = coord_y(value_max);
      const int16_t y_bottom = coord_y(value_min);
      const int16_t y_length = abs(y_bottom - y_top);
      const int8_t step = 4;
      int16_t rx;
      getGraphRight(&rx, nullptr);
      int16_t x = step;
      // 最新のデータを得る
      auto callback = [&](size_t counter, time_t at, float rh) {
        // 消去
        Screen::lcd.fillRect(rx - x, y_top, step, y_length, background_color);
        // 書き込む
        int16_t y = coord_y(rh);
        Screen::lcd.fillRect(rx - x, y, step / 2, abs(y - y_bottom),
                             line_color);
        x += step;
        return true;
      };
      //
      prepare();
      database.get_relative_humidities_desc(bme->sensor_id, graph_width / step,
                                            callback);
      update_rawid();
      grid();
    }
  }

private:
  int64_t rawid;
  //
  inline void init_rawid() { rawid = INT64_MIN; }
  inline void update_rawid() { rawid = database.rawid_relative_humidity; }
  inline bool need_for_update() {
    return rawid != database.rawid_relative_humidity;
  }
};

//
class PressureGraphView : public GraphView {
public:
  PressureGraphView(LocalDatabase &local_database)
      : GraphView(TFT_WHITE, TFT_BLACK, local_database, 940.0, 1060.0) {
    locators.push_back(970.0);
    locators.push_back(1000.0);
    locators.push_back(1030.0);
  }
  //
  bool focusIn() override {
    setOffset(Screen::lcd.width() - (graph_width + graph_margin * 2) - 1,
              Screen::lcd.height() - (graph_height + graph_margin * 2) - 1);
    //
    init_rawid();
    if (!database.healthy()) {
      return false;
    }
    if (Screen::View::focusIn()) {
      prepare();
      return true;
    } else {
      return false;
    }
  }
  //
  void prepare() {
    grid();
    //
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    Screen::lcd.setTextDatum(textdatum_t::top_left);
    Screen::lcd.drawString("気圧 hPa", 3, 3);
    //
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
  }
  //
  void render(const System::Status &, const struct tm &local,
              const Bme280::TempHumiPres *bme, const Sgp30::TvocEco2 *,
              const Scd30::Co2TempHumi *) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)まで残り45秒以上ある場合だけ,
    // この後の作業を開始する。
    //
    if (60 - local.tm_sec >= 45) {
      const int16_t y_top = coord_y(value_max);
      const int16_t y_bottom = coord_y(value_min);
      const int16_t y_length = abs(y_bottom - y_top);
      const int8_t step = 4;
      int16_t rx;
      getGraphRight(&rx, nullptr);
      int16_t x = step;
      // 最新のデータを得る
      auto callback = [&](size_t counter, time_t at, float hpa) {
        // 消去
        Screen::lcd.fillRect(rx - x, y_top, step, y_length, background_color);
        // 書き込む
        int16_t y = coord_y(hpa);
        Screen::lcd.fillRect(rx - x, y, step / 2, abs(y - y_bottom),
                             line_color);
        x += step;
        return true;
      };
      //
      prepare();
      database.get_pressures_desc(bme->sensor_id, graph_width / step, callback);
      update_rawid();
      grid();
    }
  }

private:
  int64_t rawid;
  //
  inline void init_rawid() { rawid = INT64_MIN; }
  inline void update_rawid() { rawid = database.rawid_pressure; }
  inline bool need_for_update() { return rawid != database.rawid_pressure; }
};

//
class Co2GraphView : public GraphView {
public:
  Co2GraphView(LocalDatabase &local_database)
      : GraphView(TFT_WHITE, TFT_BLACK, local_database, 0.0, 4000.0) {
    locators.push_back(400.0);
    locators.push_back(1000.0);
    locators.push_back(2000.0);
    locators.push_back(3000.0);
  }
  //
  bool focusIn() override {
    setOffset(Screen::lcd.width() - (graph_width + graph_margin * 2) - 1,
              Screen::lcd.height() - (graph_height + graph_margin * 2) - 1);
    //
    init_rawid();
    if (!database.healthy()) {
      return false;
    }
    if (Screen::View::focusIn()) {
      prepare();
      return true;
    } else {
      return false;
    }
  }
  //
  void prepare() {
    grid();
    //
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    Screen::lcd.setTextDatum(textdatum_t::top_left);
    Screen::lcd.drawString("二酸化炭素(CO2) ppm", 3, 3);
    //
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
  }
  //
  void render(const System::Status &, const struct tm &local,
              const Bme280::TempHumiPres *, const Sgp30::TvocEco2 *,
              const Scd30::Co2TempHumi *scd) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)まで残り45秒以上ある場合だけ,
    // この後の作業を開始する。
    //
    if (60 - local.tm_sec >= 45) {
      const int16_t y_top = coord_y(value_max);
      const int16_t y_bottom = coord_y(value_min);
      const int16_t y_length = abs(y_bottom - y_top);
      const int8_t step = 4;
      int16_t rx;
      getGraphRight(&rx, nullptr);
      int16_t x = step;
      // 最新のデータを得る
      auto callback = [&](size_t counter, time_t at, uint16_t co2, uint16_t,
                          bool) {
        // 消去
        Screen::lcd.fillRect(rx - x, y_top, step, y_length, background_color);
        // 書き込む
        int16_t y = coord_y(co2);
        Screen::lcd.fillRect(rx - x, y, step / 2, abs(y - y_bottom),
                             line_color);
        x += step;
        return true;
      };
      //
      prepare();
      database.get_carbon_deoxides_desc(scd->sensor_id, graph_width / step,
                                        callback);
      update_rawid();
      grid();
    }
  }

private:
  int64_t rawid;
  //
  inline void init_rawid() { rawid = INT64_MIN; }
  inline void update_rawid() { rawid = database.rawid_carbon_dioxide; }
  inline bool need_for_update() {
    return rawid != database.rawid_carbon_dioxide;
  }
};

//
class EquivalentCo2GraphView : public GraphView {
public:
  EquivalentCo2GraphView(LocalDatabase &local_database)
      : GraphView(TFT_WHITE, TFT_BLACK, local_database, 0.0, 4000.0) {
    locators.push_back(400.0);
    locators.push_back(1000.0);
    locators.push_back(2000.0);
    locators.push_back(3000.0);
  }
  //
  bool focusIn() override {
    setOffset(Screen::lcd.width() - (graph_width + graph_margin * 2) - 1,
              Screen::lcd.height() - (graph_height + graph_margin * 2) - 1);
    //
    init_rawid();
    if (!database.healthy()) {
      return false;
    }
    if (Screen::View::focusIn()) {
      prepare();
      return true;
    } else {
      return false;
    }
  }
  //
  void prepare() {
    grid();
    //
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    Screen::lcd.setTextDatum(textdatum_t::top_left);
    Screen::lcd.drawString("二酸化炭素相当量(eCo2) ppm", 3, 3);
    //
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
  }
  //
  void render(const System::Status &, const struct tm &local,
              const Bme280::TempHumiPres *, const Sgp30::TvocEco2 *sgp,
              const Scd30::Co2TempHumi *) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)まで残り45秒以上ある場合だけ,
    // この後の作業を開始する。
    //
    if (60 - local.tm_sec >= 45) {
      const int16_t y_top = coord_y(value_max);
      const int16_t y_bottom = coord_y(value_min);
      const int16_t y_length = abs(y_bottom - y_top);
      const int8_t step = 4;
      int16_t rx;
      getGraphRight(&rx, nullptr);
      int16_t x = step;
      // 最新のデータを得る
      auto callback = [&](size_t counter, time_t at, uint16_t co2, uint16_t,
                          bool) {
        // 消去
        Screen::lcd.fillRect(rx - x, y_top, step, y_length, background_color);
        // 書き込む
        int16_t y = coord_y(co2);
        Screen::lcd.fillRect(rx - x, y, step / 2, abs(y - y_bottom),
                             line_color);
        x += step;
        return true;
      };
      //
      prepare();
      database.get_carbon_deoxides_desc(sgp->sensor_id, graph_width / step,
                                        callback);
      update_rawid();
      grid();
    }
  }

private:
  int64_t rawid;
  //
  inline void init_rawid() { rawid = INT64_MIN; }
  inline void update_rawid() { rawid = database.rawid_carbon_dioxide; }
  inline bool need_for_update() {
    return rawid != database.rawid_carbon_dioxide;
  }
};

//
class TotalVocGraphView : public GraphView {
public:
  TotalVocGraphView(LocalDatabase &local_database)
      : GraphView(TFT_WHITE, TFT_BLACK, local_database, 0.0, 6000.0) {
    locators.push_back(2000.0);
    locators.push_back(4000.0);
  }
  //
  bool focusIn() override {
    setOffset(Screen::lcd.width() - (graph_width + graph_margin * 2) - 1,
              Screen::lcd.height() - (graph_height + graph_margin * 2) - 1);
    //
    init_rawid();
    if (!database.healthy()) {
      return false;
    }
    if (Screen::View::focusIn()) {
      prepare();
      return true;
    } else {
      return false;
    }
  }
  //
  void prepare() {
    grid();
    //
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    Screen::lcd.setTextDatum(textdatum_t::top_left);
    Screen::lcd.drawString("総揮発性有機化合物(TVOC) ppb", 3, 3);
    //
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
  }
  //
  void render(const System::Status &, const struct tm &local,
              const Bme280::TempHumiPres *, const Sgp30::TvocEco2 *sgp,
              const Scd30::Co2TempHumi *) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)まで残り45秒以上ある場合だけ,
    // この後の作業を開始する。
    //
    if (60 - local.tm_sec >= 45) {
      const int16_t y_top = coord_y(value_max);
      const int16_t y_bottom = coord_y(value_min);
      const int16_t y_length = abs(y_bottom - y_top);
      const int8_t step = 4;
      int16_t rx;
      getGraphRight(&rx, nullptr);
      int16_t x = step;
      // 最新のデータを得る
      auto callback = [&](size_t counter, time_t at, uint16_t tvoc, uint16_t,
                          bool) {
        // 消去
        Screen::lcd.fillRect(rx - x, y_top, step, y_length, background_color);
        // 書き込む
        int16_t y = coord_y(tvoc);
        Screen::lcd.fillRect(rx - x, y, step / 2, abs(y - y_bottom),
                             line_color);
        x += step;
        return true;
      };
      //
      prepare();
      database.get_total_vocs_desc(sgp->sensor_id, graph_width / step,
                                   callback);
      update_rawid();
      grid();
    }
  }

private:
  int64_t rawid;
  //
  inline void init_rawid() { rawid = INT64_MIN; }
  inline void update_rawid() { rawid = database.rawid_total_voc; }
  inline bool need_for_update() { return rawid != database.rawid_total_voc; }
};

//
LGFX Screen::lcd;

//
Screen::Screen(LocalDatabase &local_database)
    : views{new ClockView(),
            new SummaryView(),
            new SystemHealthView(),
            new TemperatureGraphView(local_database),
            new RelativeHumidityGraphView(local_database),
            new PressureGraphView(local_database),
            new Co2GraphView(local_database),
            new EquivalentCo2GraphView(local_database),
            new TotalVocGraphView(local_database)},
      now_view(0) {}

//
void Screen::begin(int32_t text_color, int32_t bg_color) {
  Screen::lcd.init();
  Screen::lcd.setBaseColor(bg_color);
  Screen::lcd.setTextColor(text_color, bg_color);
  Screen::lcd.setCursor(0, 0);
  Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
  //
  now_view = 0;
  views[now_view]->focusIn();
}

//
void Screen::home() {
  if (now_view == 0) {
    return;
  }
  int8_t go = 0;
  if (views[go]->focusIn()) {
    views[now_view]->focusOut();
    now_view = go;
  }
}

//
void Screen::prev() {
  int8_t go = (now_view + total_views - 1) % total_views;
  if (views[go]->focusIn()) {
    views[now_view]->focusOut();
    now_view = go;
  }
}

//
void Screen::next() {
  int8_t go = (now_view + 1) % total_views;
  if (views[go]->focusIn()) {
    views[now_view]->focusOut();
    now_view = go;
  }
}

//
void Screen::update(const System::Status &status, time_t time,
                    const Bme280::TempHumiPres *bme, const Sgp30::TvocEco2 *sgp,
                    const Scd30::Co2TempHumi *scd) {
  // time zone offset UTC+9 = asia/tokyo
  time_t local_time = time + 9 * 60 * 60;
  struct tm local;
  gmtime_r(&local_time, &local);

  views[now_view]->render(status, local, bme, sgp, scd);
}
