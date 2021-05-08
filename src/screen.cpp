// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#define _USE_MATH_DEFINES
#include "peripherals.hpp"
#include <M5Core2.h>
#include <ctime>

#include <LovyanGFX.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Coord {
  int16_t x;
  int16_t y;
};

struct Rectangle {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  //
  inline Coord center() {
    return {
        .x = static_cast<int16_t>(x + w / 2),
        .y = static_cast<int16_t>(y + h / 2),
    };
  }
  //
  inline bool isInside(Coord c) {
    bool xx = (x <= c.x) && (c.x <= x + w);
    bool yy = (y <= c.y) && (c.y <= y + h);
    return xx && yy;
  }
};

//
enum RegisteredViewId : uint32_t {
  IdSystemHealthView,
  IdClockView,
  IdSummaryView,
  IdTemperatureGraphView,
  IdRelativeHumidityGraphView,
  IdPressureGraphView,
  IdTotalVocGraphView,
  IdEquivalentCo2GraphView,
  IdCo2GraphView,
};

//
class SystemHealthView : public Screen::View {
public:
  SystemHealthView() : View(IdSystemHealthView, TFT_WHITE, TFT_BLACK) {}
  //
  void render(const struct tm &local) override {
    Peripherals &peri = Peripherals::getInstance();
    //
    Screen::lcd.setCursor(0, 0);
    Screen::lcd.setTextDatum(textdatum_t::top_left);
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    if (peri.wifi_launcher.hasWifiConnection()) {
      Screen::lcd.setTextColor(TFT_GREEN, background_color);
      Screen::lcd.printf("  Wifi");
    } else {
      Screen::lcd.setTextColor(TFT_RED, background_color);
      Screen::lcd.printf("NoWifi");
    }
    Screen::lcd.setTextColor(message_text_color, background_color);
    //
    {
      TickTack::Uptime up = peri.ticktack.uptime();
      Screen::lcd.printf(" uptime %2ddays,%2dh:%2dm\n", up.days, up.hours,
                         up.minutes);
    }
    //
    if (peri.system_power.needToUpdate()) {
      peri.system_power.update();
    }
    char sign = ' ';
    if (peri.system_power.isBatteryCharging()) {
      sign = '+';
    } else if (peri.system_power.isBatteryDischarging()) {
      sign = '-';
    } else {
      sign = ' ';
    }
    Screen::lcd.printf("Batt %4.0f%% %4.2fV %c%5.3fA",
                       peri.system_power.getBatteryPercentage(),
                       peri.system_power.getBatteryVoltage().value, sign,
                       peri.system_power.getBatteryChargingCurrent().value);
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
    Bme280 bme = peri.bme280.calculateSMA();
    if (bme.good()) {
      Screen::lcd.printf("温度 %6.1f ℃\n", bme.get().temperature.value);
      Screen::lcd.printf("湿度 %6.1f ％\n", bme.get().relative_humidity.value);
      Screen::lcd.printf("気圧 %6.1f hPa\n", bme.get().pressure.value);
    } else {
      Screen::lcd.printf("温度 ------ ℃\n");
      Screen::lcd.printf("湿度 ------ ％\n");
      Screen::lcd.printf("気圧 ------ hPa\n");
    }
    Sgp30 sgp = peri.sgp30.calculateSMA();
    if (sgp.good()) {
      Screen::lcd.printf("eCO2 %6d ppm\n", sgp.get().eCo2.value);
      Screen::lcd.printf("TVOC %6d ppb\n", sgp.get().tvoc.value);
    } else {
      Screen::lcd.printf("eCO2 ------ ppm\n");
      Screen::lcd.printf("TVOC ------ ppb\n");
    }
    Scd30 scd = peri.scd30.calculateSMA();
    if (scd.good()) {
      Screen::lcd.printf("CO2  %6d ppm\n", scd.get().co2.value);
      Screen::lcd.printf("温度 %6.1f ℃\n", scd.get().temperature.value);
      Screen::lcd.printf("湿度 %6.1f ％\n", scd.get().relative_humidity.value);
    } else {
      Screen::lcd.printf("CO2  ------ ppm\n");
      Screen::lcd.printf("温度 ------ ℃\n");
      Screen::lcd.printf("湿度 ------ ％\n");
    }
  }
};

//
class ClockView : public Screen::View {
public:
  ClockView() : View(IdClockView, TFT_WHITE, TFT_BLACK) {}
  //
  void render(const struct tm &local) override {
    const auto half_width = Screen::lcd.width() / 2;
    const auto half_height = Screen::lcd.height() / 2;
    const auto line_height = 40;

    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_32);

    // 月日表示
    {
      char buffer[20];
      snprintf(buffer, sizeof(buffer), "%2d月%2d日", local.tm_mon + 1,
               local.tm_mday);
      Screen::lcd.drawString(buffer, half_width, half_height - line_height);
      // 時分表示
      snprintf(buffer, sizeof(buffer), "%2d時%2d分", local.tm_hour,
               local.tm_min);
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
      Coord p = calculate(radius, s);
      auto w = dot_radius + dot_radius + 1;
      Screen::lcd.fillRect(p.x - dot_radius, p.y - dot_radius, w, w,
                           background_color);
    }
    //
    Screen::lcd.drawCircle(half_width, half_height, radius, bgcol);
    //
    Coord p = calculate(radius, local.tm_sec);
    Screen::lcd.fillCircle(p.x, p.y, dot_radius, fgcol);
  }

private:
  //
  Coord calculate(int r, int tm_sec) {
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
class Tile {
public:
  typedef std::pair<const char *, const char *> CaptionT;
  //
  uint32_t tap_to_move_view_id;
  const CaptionT &caption;
  char before[7];
  char now[7];
  int32_t text_color;
  int32_t background_color;
  Rectangle rect;
  Tile(uint32_t tapToMoveViewId, const CaptionT &caption_, int32_t text_col,
       int32_t bg_col)
      : tap_to_move_view_id(tapToMoveViewId),
        caption(caption_),
        before{},
        now{} {
    text_color = text_col;
    background_color = bg_col;
  }
  //
  inline bool isEqual(const CaptionT &right) { return caption == right; }
  //
  Tile &setRectangle(Rectangle r) {
    rect = r;
    return *this;
  }
  //
  Tile &prepare() {
    Coord center = rect.center();
    Screen::lcd.fillRect(rect.x, rect.y, rect.w, rect.h, background_color);
    //
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    Screen::lcd.setTextColor(text_color, background_color);
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    Screen::lcd.drawString(std::get<0>(caption), center.x - 32, center.y - 32);
    Screen::lcd.drawString(std::get<1>(caption), center.x + 32, center.y + 32);
    //
    render();
    return *this;
  }
  //
  void render_notavailable() {
    strncpy(now, "-", sizeof(now));
    if (strncmp(now, before, sizeof(before)) != 0) {
      render();
    }
  }
  //
  void render_value_uint16(uint16_t v) {
    snprintf(now, sizeof(now), "%d", v);
    if (strncmp(now, before, sizeof(before)) != 0) {
      render();
    }
  }
  //
  void render_value_float(float v) {
    snprintf(now, sizeof(now), "%.1f", v);
    if (strncmp(now, before, sizeof(before)) != 0) {
      render();
    }
  }
  //
  void render() {
    Coord center = rect.center();
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_32);
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t w = Screen::lcd.textWidth("123456");
    int16_t h = Screen::lcd.fontHeight();
    Screen::lcd.fillRect(center.x - w / 2, center.y - h / 2, w, h,
                         background_color);
    Screen::lcd.setTextColor(text_color);
    Screen::lcd.drawString(now, center.x, center.y);
    strcpy(before, now);
  }
};

//
class SummaryView : public Screen::View {
public:
  //
  SummaryView()
      : View(IdSummaryView, TFT_WHITE, TFT_BLACK),
        t_temperature(std::make_pair("温度", "℃")),
        t_relative_humidity(std::make_pair("湿度", "%RH")),
        t_pressure(std::make_pair("気圧", "hPa")),
        t_tvoc(std::make_pair("TVOC", "ppb")),
        t_eCo2(std::make_pair("eCO2", "ppm")),
        t_co2(std::make_pair("CO2", "ppm")),
        tiles{
            Tile(IdTemperatureGraphView, t_temperature, message_text_color,
                 background_color),
            Tile(IdRelativeHumidityGraphView, t_relative_humidity,
                 message_text_color, background_color),
            Tile(IdPressureGraphView, t_pressure, message_text_color,
                 background_color),
            Tile(IdTotalVocGraphView, t_tvoc, message_text_color,
                 background_color),
            Tile(IdEquivalentCo2GraphView, t_eCo2, message_text_color,
                 background_color),
            Tile(IdCo2GraphView, t_co2, message_text_color, background_color),
        } {}
  //
  bool focusIn() override {
    const int16_t w = Screen::lcd.width() / 3;
    const int16_t h = Screen::lcd.height() / 2;
    //
    if (Screen::View::focusIn()) {
      for (int16_t i = 0; i < tiles.size(); ++i) {
        int16_t col = i % 3;
        int16_t row = i / 3;
        Rectangle r;
        r.x = w * col;
        r.y = h * row;
        r.w = w;
        r.h = h;
        tiles[i].setRectangle(r).prepare();
      }
      return true;
    } else {
      return false;
    }
  }
  //
  void releaseEvent(Screen *screen, Event &e) override {
    Coord c = {
        .x = e.to.x,
        .y = e.to.y,
    };
    for (auto t : tiles) {
      if (t.rect.isInside(c)) {
        screen->moveByViewId(t.tap_to_move_view_id);
      }
    }
  }
  //
  void render(const struct tm &local) override {
    Peripherals &peri = Peripherals::getInstance();
    Bme280 bme = peri.bme280.calculateSMA();
    Sgp30 sgp = peri.sgp30.calculateSMA();
    Scd30 scd = peri.scd30.calculateSMA();
#define BME(_X_)                                                               \
  do {                                                                         \
    if (bme.good()) {                                                          \
      t.render_value_float(bme.get()._X_.value);                               \
    } else {                                                                   \
      t.render_notavailable();                                                 \
    }                                                                          \
  } while (0)
    //
#define SGP(_X_)                                                               \
  do {                                                                         \
    if (sgp.good()) {                                                          \
      t.render_value_uint16(sgp.get()._X_.value);                              \
    } else {                                                                   \
      t.render_notavailable();                                                 \
    }                                                                          \
  } while (0)
    //
#define SCD(_X_)                                                               \
  do {                                                                         \
    if (scd.good()) {                                                          \
      t.render_value_uint16(scd.get()._X_.value);                              \
    } else {                                                                   \
      t.render_notavailable();                                                 \
    }                                                                          \
  } while (0)
    //
    for (auto t : tiles) {
      if (t.isEqual(t_temperature)) {
        BME(temperature);
      } else if (t.isEqual(t_relative_humidity)) {
        BME(relative_humidity);
      } else if (t.isEqual(t_pressure)) {
        BME(pressure);
      } else if (t.isEqual(t_tvoc)) {
        SGP(tvoc);
      } else if (t.isEqual(t_eCo2)) {
        SGP(eCo2);
      } else if (t.isEqual(t_co2)) {
        SCD(co2);
      }
    }
#undef SCD
#undef SGP
#undef BME
  }

private:
  Tile::CaptionT t_temperature;
  Tile::CaptionT t_relative_humidity;
  Tile::CaptionT t_pressure;
  Tile::CaptionT t_tvoc;
  Tile::CaptionT t_eCo2;
  Tile::CaptionT t_co2;
  std::array<Tile, 6> tiles;
};

//
//
//
class GraphView : public Screen::View {
public:
  const uint32_t grid_color = Screen::lcd.color888(200, 200, 200);
  const uint32_t line_color = Screen::lcd.color888(232, 120, 120);
  //
  constexpr static int16_t graph_margin = 8;
  constexpr static int16_t graph_width = 240;
  constexpr static int16_t graph_height = 180;
  //
  GraphView(uint32_t id, int32_t text_color, int32_t bg_color,
            LocalDatabase &local_database, double vmin, double vmax)
      : View(id, text_color, bg_color),
        database(local_database),
        value_min(vmin),
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
      : GraphView(IdTemperatureGraphView, TFT_WHITE, TFT_BLACK, local_database,
                  -10.0, 60.0) {
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
    if (!database.available()) {
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
  void showUpdateMessage() {
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
  }
  //
  void hideUpdateMessage() {
    Screen::lcd.setTextColor(background_color);
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
    Screen::lcd.setTextColor(message_text_color, background_color);
  }
  //
  void prepare() {
    grid();
    //
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    Screen::lcd.setTextDatum(textdatum_t::top_left);
    Screen::lcd.drawString("温度 ℃", 3, 3);
  }
  //
  void render(const struct tm &local) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)を邪魔しない時間を選んで作業を開始する。
    //
    if (1 <= local.tm_sec && local.tm_sec <= 55) {
      const int16_t y_top = coord_y(value_max);
      const int16_t y_bottom = coord_y(value_min);
      const int16_t y_length = abs(y_bottom - y_top);
      const int16_t y0 = coord_y(0.0);
      const int8_t step = 2;
      int16_t rx;
      getGraphRight(&rx, nullptr);
      int16_t x = step;
      uint16_t counter = 0;
      (void)counter;
      // 最新のデータを得る
      auto callback = [&](size_t counter, time_t at, float degc) {
        if (counter == 0) {
          hideUpdateMessage();
        }
        counter = counter + 1;
        // 消去
        Screen::lcd.fillRect(rx - x, y_top, step, y_length, background_color);
        // 書き込む
        int16_t y = coord_y(degc);
        Screen::lcd.fillRect(rx - x, y, step / 2, abs(y - y0), line_color);
        x += step;
        return true;
      };
      //
      Peripherals &peri = Peripherals::getInstance();
      showUpdateMessage();
      database.get_temperatures_desc(peri.bme280.getSensorDescriptor().id,
                                     graph_width / step, callback);
      rawid = database.rawid_temperature;
      grid();
    } else {
      showUpdateMessage();
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
      : GraphView(IdRelativeHumidityGraphView, TFT_WHITE, TFT_BLACK,
                  local_database, 0.0, 100.0) {
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
    if (!database.available()) {
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
  void showUpdateMessage() {
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
  }
  //
  void hideUpdateMessage() {
    Screen::lcd.setTextColor(background_color);
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
    Screen::lcd.setTextColor(message_text_color, background_color);
  }
  //
  void prepare() {
    grid();
    //
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    Screen::lcd.setTextDatum(textdatum_t::top_left);
    Screen::lcd.drawString("相対湿度 %RH", 3, 3);
  }
  //
  void render(const struct tm &local) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)を邪魔しない時間を選んで作業を開始する。
    //
    if (1 <= local.tm_sec && local.tm_sec <= 55) {
      const int16_t y_top = coord_y(value_max);
      const int16_t y_bottom = coord_y(value_min);
      const int16_t y_length = abs(y_bottom - y_top);
      const int8_t step = 2;
      int16_t rx;
      getGraphRight(&rx, nullptr);
      int16_t x = step;
      uint16_t counter = 0;
      (void)counter;
      // 最新のデータを得る
      auto callback = [&](size_t counter, time_t at, float rh) {
        if (counter == 0) {
          hideUpdateMessage();
        }
        counter = counter + 1;
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
      showUpdateMessage();
      Peripherals &peri = Peripherals::getInstance();
      database.get_relative_humidities_desc(
          peri.bme280.getSensorDescriptor().id, graph_width / step, callback);
      update_rawid();
      grid();
    } else {
      showUpdateMessage();
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
      : GraphView(IdPressureGraphView, TFT_WHITE, TFT_BLACK, local_database,
                  940.0, 1060.0) {
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
    if (!database.available()) {
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
  void showUpdateMessage() {
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
  }
  //
  void hideUpdateMessage() {
    Screen::lcd.setTextColor(background_color);
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
    Screen::lcd.setTextColor(message_text_color, background_color);
  }
  //
  void prepare() {
    grid();
    //
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    Screen::lcd.setTextDatum(textdatum_t::top_left);
    Screen::lcd.drawString("気圧 hPa", 3, 3);
  }
  //
  void render(const struct tm &local) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)を邪魔しない時間を選んで作業を開始する。
    //
    if (1 <= local.tm_sec && local.tm_sec <= 55) {
      const int16_t y_top = coord_y(value_max);
      const int16_t y_bottom = coord_y(value_min);
      const int16_t y_length = abs(y_bottom - y_top);
      const int8_t step = 2;
      int16_t rx;
      getGraphRight(&rx, nullptr);
      int16_t x = step;
      uint16_t counter = 0;
      (void)counter;
      // 最新のデータを得る
      auto callback = [&](size_t counter, time_t at, float hpa) {
        if (counter == 0) {
          hideUpdateMessage();
        }
        counter = counter + 1;
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
      showUpdateMessage();
      Peripherals &peri = Peripherals::getInstance();
      database.get_pressures_desc(peri.bme280.getSensorDescriptor().id,
                                  graph_width / step, callback);
      update_rawid();
      grid();
    } else {
      showUpdateMessage();
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
class TotalVocGraphView : public GraphView {
public:
  TotalVocGraphView(LocalDatabase &local_database)
      : GraphView(IdTotalVocGraphView, TFT_WHITE, TFT_BLACK, local_database,
                  0.0, 6000.0) {
    locators.push_back(2000.0);
    locators.push_back(4000.0);
  }
  //
  bool focusIn() override {
    setOffset(Screen::lcd.width() - (graph_width + graph_margin * 2) - 1,
              Screen::lcd.height() - (graph_height + graph_margin * 2) - 1);
    //
    init_rawid();
    if (!database.available()) {
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
  void showUpdateMessage() {
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
  }
  //
  void hideUpdateMessage() {
    Screen::lcd.setTextColor(background_color);
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
    Screen::lcd.setTextColor(message_text_color, background_color);
  }
  //
  void prepare() {
    grid();
    //
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    Screen::lcd.setTextDatum(textdatum_t::top_left);
    Screen::lcd.drawString("総揮発性有機化合物(TVOC) ppb", 3, 3);
  }
  //
  void render(const struct tm &local) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)を邪魔しない時間を選んで作業を開始する。
    //
    if (1 <= local.tm_sec && local.tm_sec <= 55) {
      const int16_t y_top = coord_y(value_max);
      const int16_t y_bottom = coord_y(value_min);
      const int16_t y_length = abs(y_bottom - y_top);
      const int8_t step = 2;
      int16_t rx;
      getGraphRight(&rx, nullptr);
      int16_t x = step;
      uint16_t counter = 0;
      (void)counter;
      // 最新のデータを得る
      auto callback = [&](size_t counter, time_t at, uint16_t tvoc, uint16_t,
                          bool) {
        if (counter == 0) {
          hideUpdateMessage();
        }
        counter = counter + 1;
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
      showUpdateMessage();
      Peripherals &peri = Peripherals::getInstance();
      database.get_total_vocs_desc(peri.sgp30.getSensorDescriptor().id,
                                   graph_width / step, callback);
      update_rawid();
      grid();
    } else {
      showUpdateMessage();
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
class EquivalentCo2GraphView : public GraphView {
public:
  EquivalentCo2GraphView(LocalDatabase &local_database)
      : GraphView(IdEquivalentCo2GraphView, TFT_WHITE, TFT_BLACK,
                  local_database, 0.0, 4000.0) {
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
    if (!database.available()) {
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
  void showUpdateMessage() {
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
  }
  //
  void hideUpdateMessage() {
    Screen::lcd.setTextColor(background_color);
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
    Screen::lcd.setTextColor(message_text_color, background_color);
  }
  //
  void prepare() {
    grid();
    //
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    Screen::lcd.setTextDatum(textdatum_t::top_left);
    Screen::lcd.drawString("二酸化炭素相当量(eCo2) ppm", 3, 3);
  }
  //
  void render(const struct tm &local) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)を邪魔しない時間を選んで作業を開始する。
    //
    if (1 <= local.tm_sec && local.tm_sec <= 55) {
      const int16_t y_top = coord_y(value_max);
      const int16_t y_bottom = coord_y(value_min);
      const int16_t y_length = abs(y_bottom - y_top);
      const int8_t step = 2;
      int16_t rx;
      getGraphRight(&rx, nullptr);
      int16_t x = step;
      uint16_t counter = 0;
      (void)counter;
      // 最新のデータを得る
      auto callback = [&](size_t counter, time_t at, uint16_t co2, uint16_t,
                          bool) {
        if (counter == 0) {
          hideUpdateMessage();
        }
        counter = counter + 1;
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
      showUpdateMessage();
      Peripherals &peri = Peripherals::getInstance();
      database.get_carbon_deoxides_desc(peri.sgp30.getSensorDescriptor().id,
                                        graph_width / step, callback);
      update_rawid();
      grid();
    } else {
      showUpdateMessage();
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
class Co2GraphView : public GraphView {
public:
  Co2GraphView(LocalDatabase &local_database)
      : GraphView(IdCo2GraphView, TFT_WHITE, TFT_BLACK, local_database, 0.0,
                  4000.0) {
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
    if (!database.available()) {
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
  void showUpdateMessage() {
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
  }
  //
  void hideUpdateMessage() {
    Screen::lcd.setTextColor(background_color);
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t cx, cy;
    getGraphCenter(&cx, &cy);
    Screen::lcd.drawString("更新中", cx, cy);
    Screen::lcd.setTextColor(message_text_color, background_color);
  }
  //
  void prepare() {
    grid();
    //
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    Screen::lcd.setTextDatum(textdatum_t::top_left);
    Screen::lcd.drawString("二酸化炭素(CO2) ppm", 3, 3);
  }
  //
  void render(const struct tm &local) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)を邪魔しない時間を選んで作業を開始する。
    //
    if (1 <= local.tm_sec && local.tm_sec <= 55) {
      const int16_t y_top = coord_y(value_max);
      const int16_t y_bottom = coord_y(value_min);
      const int16_t y_length = abs(y_bottom - y_top);
      const int8_t step = 2;
      int16_t rx;
      getGraphRight(&rx, nullptr);
      int16_t x = step;
      uint16_t counter = 0;
      (void)counter;
      // 最新のデータを得る
      auto callback = [&](size_t counter, time_t at, uint16_t co2, uint16_t,
                          bool) {
        if (counter == 0) {
          hideUpdateMessage();
        }
        counter = counter + 1;
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
      showUpdateMessage();
      Peripherals &peri = Peripherals::getInstance();
      database.get_carbon_deoxides_desc(peri.scd30.getSensorDescriptor().id,
                                        graph_width / step, callback);
      update_rawid();
      grid();
    } else {
      showUpdateMessage();
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
LGFX Screen::lcd;

//
Screen::Screen(LocalDatabase &local_database)
    : views{new ClockView(),
            new SummaryView(),
            new TemperatureGraphView(local_database),
            new RelativeHumidityGraphView(local_database),
            new PressureGraphView(local_database),
            new TotalVocGraphView(local_database),
            new EquivalentCo2GraphView(local_database),
            new Co2GraphView(local_database),
            new SystemHealthView(),
            },
      now_view(0) {}

//
bool Screen::begin(int32_t text_color, int32_t bg_color) {
  Screen::lcd.init();
  Screen::lcd.setBaseColor(TFT_BLACK);
  Screen::lcd.setTextColor(text_color, bg_color);
  Screen::lcd.setCursor(0, 0);
  Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
  return true;
}

//
bool Screen::moveToView(int go_view) {
  if (now_view == go_view) {
    return false;
  }
  if (views[go_view]->focusIn()) {
    views[now_view]->focusOut();
    now_view = go_view;
    return true;
  } else {
    return false;
  }
}

//
void Screen::home() { moveToView(0); }

//
void Screen::prev() {
  int8_t go = (now_view + total_views - 1) % total_views;
  moveToView(go);
}

//
void Screen::next() {
  int8_t go = (now_view + 1) % total_views;
  moveToView(go);
}

//
bool Screen::moveByViewId(uint32_t view_id) {
  for (int16_t i = 0; i < sizeof(views); ++i) {
    if (views[i]->view_id == view_id) {
      return moveToView(i);
    }
  }
  return false;
}

//
void Screen::update(time_t time) {
  // time zone offset UTC+9 = asia/tokyo
  time_t local_time = time + 9 * 60 * 60;
  struct tm local;
  gmtime_r(&local_time, &local);

  views[now_view]->render(local);
}

//
void Screen::repaint(std::time_t time) {
  views[now_view]->focusIn();
  update(time);
}

//
void Screen::releaseEvent(Event &e) { views[now_view]->releaseEvent(this, e); }
