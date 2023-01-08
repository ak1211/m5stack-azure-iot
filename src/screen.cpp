// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#define _USE_MATH_DEFINES
#include "peripherals.hpp"
#include <M5Core2.h>
#include <array>
#include <chrono>
#include <ctime>
#include <tuple>

#include <LovyanGFX.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using Coord = std::pair<int16_t, int16_t>;

struct Rectangle final {
  int16_t left_x;
  int16_t top_y;
  int16_t width;
  int16_t height;
  //
  inline Coord center() const {
    return {left_x + width / 2, top_y + height / 2};
  }
  //
  inline bool contains(Coord c) const {
    const auto [cx, cy] = c;
    bool xx = (left_x <= cx) && (cx < left_x + width);
    bool yy = (top_y <= cy) && (cy < top_y + height);
    return xx && yy;
  }
};

struct Range final {
  double min;
  double max;
  //
  inline double range() const { return max - min; }
  //
  inline bool contains(double x) const { return min <= x && x <= max; }
};

//
//
//
class Screen::View {
  const RegisteredViewId view_id;

public:
  explicit View(RegisteredViewId id) : view_id{id} {}
  virtual ~View() {}
  RegisteredViewId getId() { return view_id; }
  bool isThisId(RegisteredViewId id) { return id == view_id; }
  virtual bool focusIn() = 0;
  virtual void focusOut() = 0;
  virtual RegisteredViewId releaseEvent(Event &e) = 0;
  virtual void render(const struct tm &local) = 0;
};

//
class SystemHealthView : public Screen::View {
  int32_t message_text_color;
  int32_t background_color;

public:
  explicit SystemHealthView()
      : View(Screen::IdSystemHealthView),
        message_text_color{TFT_WHITE},
        background_color{TFT_BLACK} {}
  //
  bool focusIn() override {
    Screen::lcd.setBaseColor(background_color);
    Screen::lcd.setTextColor(message_text_color, background_color);
    Screen::lcd.clear();
    return true;
  }
  //
  void focusOut() override {}
  //
  Screen::RegisteredViewId releaseEvent(Event &e) override { return getId(); }
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
    //
    const auto [batt, current] = peri.system_power.getBatteryCurrent();
    char sign = (batt == SystemPower::BatteryNow::Charging)      ? '+'
                : (batt == SystemPower::BatteryNow::Discharging) ? '-'
                                                                 : ' ';
    const Voltage volt = peri.system_power.getBatteryVoltage();
    const Ampere amp = current;
    Screen::lcd.printf("Batt %4.0f%% %4.2fV %c%5.3fA",
                       peri.system_power.getBatteryPercentage(),
                       static_cast<float>(volt.count()), sign,
                       static_cast<float>(amp.count()));
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
    std::time_t now = std::time(nullptr);
    std::optional<TempHumiPres> bme = peri.bme280.calculateSMA(now);
    std::optional<TvocEco2> sgp = peri.sgp30.calculateSMA(now);
    std::optional<Co2TempHumi> scd = peri.scd30.calculateSMA(now);
    if (bme.has_value()) {
      const DegC tCelcius = bme->temperature;
      Screen::lcd.printf(u8"温度 %6.1f ℃\n",
                         static_cast<float>(tCelcius.count()));
      const RelHumidity rh = bme->relative_humidity;
      Screen::lcd.printf(u8"湿度 %6.1f ％\n", static_cast<float>(rh.count()));
      const HectoPa hpa = bme->pressure;
      Screen::lcd.printf(u8"気圧 %6.1f hPa\n", static_cast<float>(hpa.count()));
    } else {
      Screen::lcd.printf(u8"温度 ------ ℃\n");
      Screen::lcd.printf(u8"湿度 ------ ％\n");
      Screen::lcd.printf(u8"気圧 ------ hPa\n");
    }
    if (sgp.has_value()) {
      Screen::lcd.printf(u8"eCO2 %6d ppm\n", sgp->eCo2.value);
      Screen::lcd.printf(u8"TVOC %6d ppb\n", sgp->tvoc.value);
    } else {
      Screen::lcd.printf(u8"eCO2 ------ ppm\n");
      Screen::lcd.printf(u8"TVOC ------ ppb\n");
    }
    if (scd.has_value()) {
      const DegC tCelcius = scd->temperature;
      Screen::lcd.printf(u8"CO2  %6d ppm\n", scd->co2.value);
      Screen::lcd.printf(u8"温度 %6.1f ℃\n",
                         static_cast<float>(tCelcius.count()));
      const RelHumidity rh = scd->relative_humidity;
      Screen::lcd.printf(u8"湿度 %6.1f ％\n", static_cast<float>(rh.count()));
    } else {
      Screen::lcd.printf(u8"CO2  ------ ppm\n");
      Screen::lcd.printf(u8"温度 ------ ℃\n");
      Screen::lcd.printf(u8"湿度 ------ ％\n");
    }
  }
};

//
class ClockView : public Screen::View {
  int32_t message_text_color;
  int32_t background_color;
  //
  Coord second_hand(int16_t r, int16_t tm_sec) {
    const auto half_width = Screen::lcd.width() / 2;
    const auto half_height = Screen::lcd.height() / 2;
    constexpr double r90degrees = M_PI / 2.0;
    constexpr double delta_radian = 2.0 * M_PI / 60.0;
    const double rad =
        delta_radian * static_cast<double>(60 - tm_sec) + r90degrees;
    return {static_cast<int16_t>(half_width + r * std::cos(rad)),
            static_cast<int16_t>(half_height - r * std::sin(rad))};
  };

public:
  explicit ClockView()
      : View(Screen::IdClockView),
        message_text_color{TFT_WHITE},
        background_color{TFT_BLACK} {}
  //
  bool focusIn() override {
    Screen::lcd.setBaseColor(background_color);
    Screen::lcd.setTextColor(message_text_color, background_color);
    Screen::lcd.clear();
    return true;
  }
  //
  void focusOut() override {}
  //
  Screen::RegisteredViewId releaseEvent(Event &e) override { return getId(); }
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
    const int16_t radius = static_cast<int16_t>(
        std::floor(std::min(half_width, half_height) * 0.9));
    //
    uint32_t bgcol = Screen::lcd.color888(77, 77, 77);
    uint32_t fgcol = Screen::lcd.color888(232, 107, 107);
    //
    for (auto sec = 0; sec < 60; ++sec) {
      const auto [x, y] = second_hand(radius, sec);
      auto w = dot_radius + dot_radius + 1;
      Screen::lcd.fillRect(x - dot_radius, y - dot_radius, w, w,
                           background_color);
    }
    //
    Screen::lcd.drawCircle(half_width, half_height, radius, bgcol);
    //
    const auto [sec_x, sec_y] = second_hand(radius, local.tm_sec);
    Screen::lcd.fillCircle(sec_x, sec_y, dot_radius, fgcol);
  }
};

//
class Tile {
public:
  using CaptionT = std::pair<std::string_view, std::string_view>;
  //
  Screen::RegisteredViewId tap_to_move_view_id;
  const CaptionT &caption;
  char before[7];
  char now[7];
  int32_t text_color;
  int32_t background_color;
  Rectangle rect;
  Tile(Screen::RegisteredViewId tapToMoveViewId, const CaptionT &caption_,
       int32_t text_col, int32_t bg_col)
      : tap_to_move_view_id{tapToMoveViewId},
        caption{caption_},
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
    const auto [cx, cy] = rect.center();
    Screen::lcd.fillRect(rect.left_x, rect.top_y, rect.width, rect.height,
                         background_color);
    //
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    Screen::lcd.setTextColor(text_color, background_color);
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    const auto [value, unit] = caption;
    Screen::lcd.drawString(value.data(), cx - 32, cy - 32);
    Screen::lcd.drawString(unit.data(), cx + 32, cy + 32);
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
    const auto [cx, cy] = rect.center();
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_32);
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    int16_t w = Screen::lcd.textWidth("123456");
    int16_t h = Screen::lcd.fontHeight();
    Screen::lcd.fillRect(cx - w / 2, cy - h / 2, w, h, background_color);
    Screen::lcd.setTextColor(text_color);
    Screen::lcd.drawString(now, cx, cy);
    strcpy(before, now);
  }
};

//
class SummaryView : public Screen::View {
  Tile::CaptionT t_temperature;
  Tile::CaptionT t_relative_humidity;
  Tile::CaptionT t_pressure;
  Tile::CaptionT t_tvoc;
  Tile::CaptionT t_eCo2;
  Tile::CaptionT t_co2;
  std::array<Tile, 6> tiles;

public:
  int32_t message_text_color;
  int32_t background_color;
  //
  SummaryView()
      : View(Screen::IdSummaryView),
        message_text_color{TFT_WHITE},
        background_color{TFT_BLACK},
        t_temperature({u8"温度", u8"℃"}),
        t_relative_humidity({u8"湿度", u8"%RH"}),
        t_pressure({u8"気圧", u8"hPa"}),
        t_tvoc({u8"TVOC", u8"ppb"}),
        t_eCo2({u8"eCO2", u8"ppm"}),
        t_co2({u8"CO2", u8"ppm"}),
        tiles{
            Tile(Screen::IdTemperatureGraphView, t_temperature,
                 message_text_color, background_color),
            Tile(Screen::IdRelativeHumidityGraphView, t_relative_humidity,
                 message_text_color, background_color),
            Tile(Screen::IdPressureGraphView, t_pressure, message_text_color,
                 background_color),
            Tile(Screen::IdTotalVocGraphView, t_tvoc, message_text_color,
                 background_color),
            Tile(Screen::IdEquivalentCo2GraphView, t_eCo2, message_text_color,
                 background_color),
            Tile(Screen::IdCo2GraphView, t_co2, message_text_color,
                 background_color),
        } {}
  //
  bool focusIn() override {
    const int16_t w = Screen::lcd.width() / 3;
    const int16_t h = Screen::lcd.height() / 2;
    //
    Screen::lcd.setBaseColor(background_color);
    Screen::lcd.setTextColor(message_text_color, background_color);
    Screen::lcd.clear();
    //
    for (int16_t i = 0; i < tiles.size(); ++i) {
      int16_t col = i % 3;
      int16_t row = i / 3;
      Rectangle r;
      r.left_x = w * col;
      r.top_y = h * row;
      r.width = w;
      r.height = h;
      tiles[i].setRectangle(r).prepare();
    }
    return true;
  }
  //
  void focusOut() override {}
  //
  Screen::RegisteredViewId releaseEvent(Event &e) override {
    Coord c = {e.to.x, e.to.y};
    for (auto t : tiles) {
      if (t.rect.contains(c)) {
        return t.tap_to_move_view_id;
      }
    }
    return getId();
  }
  //
  void render(const struct tm &local) override {
    Peripherals &peri = Peripherals::getInstance();
    std::time_t now = std::time(nullptr);
    std::optional<TempHumiPres> bme = peri.bme280.calculateSMA(now);
    std::optional<TvocEco2> sgp = peri.sgp30.calculateSMA(now);
    std::optional<Co2TempHumi> scd = peri.scd30.calculateSMA(now);
    //
    for (auto t : tiles) {
      if (t.isEqual(t_temperature)) {
        if (bme.has_value()) {
          t.render_value_float(static_cast<DegC>(bme->temperature).count());
        } else {
          t.render_notavailable();
        }
      } else if (t.isEqual(t_relative_humidity)) {
        if (bme.has_value()) {
          t.render_value_float(
              static_cast<RelHumidity>(bme->relative_humidity).count());
        } else {
          t.render_notavailable();
        }
      } else if (t.isEqual(t_pressure)) {
        if (bme.has_value()) {
          t.render_value_float(static_cast<HectoPa>(bme->pressure).count());
        } else {
          t.render_notavailable();
        }
      } else if (t.isEqual(t_tvoc)) {
        if (sgp.has_value()) {
          t.render_value_uint16(sgp->tvoc.value);
        } else {
          t.render_notavailable();
        }
      } else if (t.isEqual(t_eCo2)) {
        if (sgp.has_value()) {
          t.render_value_uint16(sgp->eCo2.value);
        } else {
          t.render_notavailable();
        }
      } else if (t.isEqual(t_co2)) {
        if (scd.has_value()) {
          t.render_value_uint16(scd->co2.value);
        } else {
          t.render_notavailable();
        }
      }
    }
  }
};

//
//
//
class GraphPlotter {
public:
  struct State {
    int32_t message_text_color;
    int32_t background_color;
    uint32_t grid_color;
    uint32_t line_color;
    Coord graph_offset;
    Range range;
    std::vector<double> locators;
  };
  //
  constexpr static int16_t graph_margin = 8;
  constexpr static int16_t graph_width = 240;
  constexpr static int16_t graph_height = 180;
  //
  static Coord getGraphLeft(const State &st) {
    const auto [ox, oy] = st.graph_offset;
    return {ox + graph_margin, oy + graph_margin};
  }
  //
  static Coord getGraphRight(const State &st) {
    const auto [ox, oy] = st.graph_offset;
    return {ox + graph_margin + graph_width, oy + graph_margin + graph_height};
  }
  //
  static Coord getGraphCenter(const State &st) {
    const auto [ox, oy] = st.graph_offset;
    return {ox + graph_margin + graph_width / 2,
            oy + graph_margin + graph_height / 2};
  }
  //
  static Coord calculateGraphOffset(int16_t displayWidth,
                                    int16_t displayHeight) {

    return {static_cast<int16_t>(displayWidth -
                                 (graph_width + graph_margin * 2) - 1),
            static_cast<int16_t>(displayHeight -
                                 (graph_height + graph_margin * 2) - 1)};
  }
  //
  static double normalize_value(const Range &range, double value) {
    return (value - range.min) / range.range();
  }
  //
  static double clipping_value(const Range &range, double value) {
    return max(min(value, range.max), range.min);
  }
  //
  static int16_t coord_y(const State &st, double value) {
    const auto [unused, oy] = st.graph_offset;
    int16_t y = graph_height *
                normalize_value(st.range, clipping_value(st.range, value));
    return (oy + graph_margin + graph_height - y);
  }
  //
  static void grid(const State &st) {
    const auto [leftx, unused] = getGraphLeft(st);
    const auto [ox, unused2] = st.graph_offset;
    //
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    Screen::lcd.setTextColor(st.message_text_color);
    Screen::lcd.setTextDatum(textdatum_t::middle_right);
    //
    for (auto value : st.locators) {
      if (st.range.contains(value)) {
        int16_t y = coord_y(st, value);
        Screen::lcd.drawFastHLine(leftx, y, graph_width, st.grid_color);
        Screen::lcd.drawFloat(value, 1, ox, y);
      }
    }
  }
  //
  static void prepare(const State &st, const char *title) {
    Screen::lcd.setBaseColor(st.background_color);
    Screen::lcd.setTextColor(st.message_text_color, st.background_color);
    Screen::lcd.clear();
    //
    grid(st);
    //
    Screen::lcd.setFont(&fonts::lgfxJapanGothic_20);
    Screen::lcd.setTextDatum(textdatum_t::top_left);
    Screen::lcd.drawString(title, 3, 3);
  }
  //
  static void showUpdateMessage(const State &st) {
    Screen::lcd.setTextColor(st.message_text_color);
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    const auto [x, y] = getGraphCenter(st);
    Screen::lcd.drawString(u8"更新中", x, y);
  }
  //
  static void hideUpdateMessage(const State &st) {
    Screen::lcd.setTextColor(st.background_color);
    Screen::lcd.setTextDatum(textdatum_t::middle_center);
    const auto [x, y] = getGraphCenter(st);
    Screen::lcd.drawString(u8"更新中", x, y);
    Screen::lcd.setTextColor(st.message_text_color, st.background_color);
  }
  //
  template <class T, std::size_t N>
  static void plot(const State &st, const std::array<T, N> &data_reversed) {
    const int16_t y_top = coord_y(st, st.range.max);
    const int16_t y_bottom = coord_y(st, st.range.min);
    const int16_t y_length = abs(y_bottom - y_top);
    const int16_t y0 = coord_y(st, 0.0);
    const int16_t step = graph_width / data_reversed.size();
    const auto [rx, unused] = getGraphRight(st);
    for (std::size_t i = 0; i < data_reversed.size(); ++i) {
      int16_t x = rx - i * step;
      int16_t y = coord_y(st, data_reversed[i]);
      // 消去
      Screen::lcd.fillRect(x, y_top, step, y_length, st.background_color);
      // 書き込む
      Screen::lcd.fillRect(x, y, step / 2, std::abs(y - y0), st.line_color);
    }
  }
  //
  static void progressBar(const State &st, uint16_t current, uint16_t min,
                          uint16_t max) {
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
    Screen::lcd.setTextColor(st.message_text_color);
    Screen::lcd.drawNumber(percentage, cx, cy);
  }
};

//
class TemperatureGraphView : public Screen::View {
public:
  //
  TemperatureGraphView() : View(Screen::IdTemperatureGraphView), state{} {}
  //
  bool focusIn() override {
    state = GraphPlotter::State{
        .message_text_color = TFT_WHITE,
        .background_color = TFT_BLACK,
        .grid_color = Screen::lcd.color888(200, 200, 200),
        .line_color = Screen::lcd.color888(232, 120, 120),
        .graph_offset = GraphPlotter::calculateGraphOffset(
            Screen::lcd.width(), Screen::lcd.height()),
        .range =
            Range{
                .min = -10.0,
                .max = 60.0,
            },
        .locators = {-10.0, 0.0, 20.0, 40.0, 60.0},
    };
    init_rawid();
    GraphPlotter::prepare(state, u8"温度 ℃");
    return true;
  }
  //
  void focusOut() override {}
  //
  Screen::RegisteredViewId releaseEvent(Event &e) override { return getId(); }
  //
  void render(const struct tm &local) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)を邪魔しない時間を選んで作業を開始する。
    //
    if (1 <= local.tm_sec && local.tm_sec <= 55) {
      std::array<float, GraphPlotter::graph_width / 2> data_reversed;
      GraphPlotter::showUpdateMessage(state);
      update_data(data_reversed.max_size(), data_reversed);
      update_rawid();
      GraphPlotter::hideUpdateMessage(state);
      GraphPlotter::plot(state, data_reversed);
      GraphPlotter::grid(state);
    } else {
      GraphPlotter::showUpdateMessage(state);
    }
  }

private:
  GraphPlotter::State state;
  int64_t rawid;
  //
  void init_rawid() { rawid = INT64_MIN; }
  //
  void update_rawid() {
    rawid = Peripherals::getInstance().local_database.rowid_temperature;
  }
  //
  template <class T, std::size_t N>
  std::array<T, N> &update_data(std::size_t count,
                                std::array<T, N> &data_reversed) {
    Peripherals &peri = Peripherals::getInstance();
    // 最新のデータを得る
    auto callback = [&](size_t counter, time_t, T val) {
      data_reversed[counter] = val;
      counter = counter + 1;
      return true;
    };
    peri.local_database.get_temperatures_desc(
        peri.bme280.getSensorDescriptor().id, count, callback);
    return data_reversed;
  }
  //
  bool need_for_update() {
    return rawid != Peripherals::getInstance().local_database.rowid_temperature;
  }
};

//
class RelativeHumidityGraphView : public Screen::View {
public:
  RelativeHumidityGraphView()
      : View(Screen::IdRelativeHumidityGraphView), state{} {}
  //
  bool focusIn() override {
    state = GraphPlotter::State{
        .message_text_color = TFT_WHITE,
        .background_color = TFT_BLACK,
        .grid_color = Screen::lcd.color888(200, 200, 200),
        .line_color = Screen::lcd.color888(232, 120, 120),
        .graph_offset = GraphPlotter::calculateGraphOffset(
            Screen::lcd.width(), Screen::lcd.height()),
        .range =
            Range{
                .min = 0.0,
                .max = 100.0,
            },
        .locators = {0.0, 25.0, 50.0, 75.0, 100.0},
    };
    init_rawid();
    GraphPlotter::prepare(state, u8"相対湿度 %RH");
    return true;
  }
  //
  void focusOut() override {}
  //
  Screen::RegisteredViewId releaseEvent(Event &e) override { return getId(); }
  //
  void render(const struct tm &local) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)を邪魔しない時間を選んで作業を開始する。
    //
    if (1 <= local.tm_sec && local.tm_sec <= 55) {
      std::array<float, GraphPlotter::graph_width / 2> data_reversed;
      GraphPlotter::showUpdateMessage(state);
      update_data(data_reversed.max_size(), data_reversed);
      update_rawid();
      GraphPlotter::hideUpdateMessage(state);
      GraphPlotter::plot(state, data_reversed);
      GraphPlotter::grid(state);
    } else {
      GraphPlotter::showUpdateMessage(state);
    }
  }

private:
  GraphPlotter::State state;
  int64_t rawid;
  //
  void init_rawid() { rawid = INT64_MIN; }
  //
  void update_rawid() {
    rawid = Peripherals::getInstance().local_database.rowid_relative_humidity;
  }
  //
  template <class T, std::size_t N>
  std::array<T, N> &update_data(std::size_t count,
                                std::array<T, N> &data_reversed) {
    Peripherals &peri = Peripherals::getInstance();
    // 最新のデータを得る
    auto callback = [&](size_t counter, time_t, T val) {
      data_reversed[counter] = val;
      counter = counter + 1;
      return true;
    };
    peri.local_database.get_relative_humidities_desc(
        peri.bme280.getSensorDescriptor().id, count, callback);
    return data_reversed;
  }
  //
  bool need_for_update() {
    return rawid !=
           Peripherals::getInstance().local_database.rowid_relative_humidity;
  }
};

//
class PressureGraphView : public Screen::View {
public:
  PressureGraphView() : View(Screen::IdPressureGraphView) {}
  //
  bool focusIn() override {
    state = GraphPlotter::State{
        .message_text_color = TFT_WHITE,
        .background_color = TFT_BLACK,
        .grid_color = Screen::lcd.color888(200, 200, 200),
        .line_color = Screen::lcd.color888(232, 120, 120),
        .graph_offset = GraphPlotter::calculateGraphOffset(
            Screen::lcd.width(), Screen::lcd.height()),
        .range =
            Range{
                .min = 940.0,
                .max = 1060.0,
            },
        .locators = {940.0, 970.0, 1000.0, 1030.0, 1060.0},
    };
    init_rawid();
    GraphPlotter::prepare(state, u8"気圧 hPa");
    return true;
  }
  //
  void focusOut() override {}
  //
  Screen::RegisteredViewId releaseEvent(Event &e) override { return getId(); }
  //
  void render(const struct tm &local) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)を邪魔しない時間を選んで作業を開始する。
    //
    if (1 <= local.tm_sec && local.tm_sec <= 55) {
      std::array<float, GraphPlotter::graph_width / 2> data_reversed;
      GraphPlotter::showUpdateMessage(state);
      update_data(data_reversed.max_size(), data_reversed);
      update_rawid();
      GraphPlotter::hideUpdateMessage(state);
      GraphPlotter::plot(state, data_reversed);
      GraphPlotter::grid(state);
    } else {
      GraphPlotter::showUpdateMessage(state);
    }
  }

private:
  GraphPlotter::State state;
  int64_t rawid;
  //
  void init_rawid() { rawid = INT64_MIN; }
  //
  void update_rawid() {
    rawid = Peripherals::getInstance().local_database.rowid_pressure;
  }
  //
  template <class T, std::size_t N>
  std::array<T, N> &update_data(std::size_t count,
                                std::array<T, N> &data_reversed) {
    Peripherals &peri = Peripherals::getInstance();
    // 最新のデータを得る
    auto callback = [&](size_t counter, time_t, T val) {
      data_reversed[counter] = val;
      counter = counter + 1;
      return true;
    };
    peri.local_database.get_pressures_desc(peri.bme280.getSensorDescriptor().id,
                                           count, callback);
    return data_reversed;
  }
  //
  bool need_for_update() {
    return rawid != Peripherals::getInstance().local_database.rowid_pressure;
  }
};

//
class TotalVocGraphView : public Screen::View {
public:
  TotalVocGraphView() : View(Screen::IdTotalVocGraphView) {}
  //
  bool focusIn() override {
    state = GraphPlotter::State{
        .message_text_color = TFT_WHITE,
        .background_color = TFT_BLACK,
        .grid_color = Screen::lcd.color888(200, 200, 200),
        .line_color = Screen::lcd.color888(232, 120, 120),
        .graph_offset = GraphPlotter::calculateGraphOffset(
            Screen::lcd.width(), Screen::lcd.height()),
        .range =
            Range{
                .min = 0.0,
                .max = 6000.0,
            },
        .locators = {0.0, 1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0},
    };
    init_rawid();
    GraphPlotter::prepare(state, u8"総揮発性有機化合物(TVOC) ppb");
    return true;
  }
  //
  void focusOut() override {}
  //
  Screen::RegisteredViewId releaseEvent(Event &e) override { return getId(); }
  //
  void render(const struct tm &local) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)を邪魔しない時間を選んで作業を開始する。
    //
    if (1 <= local.tm_sec && local.tm_sec <= 55) {
      std::array<uint16_t, GraphPlotter::graph_width / 2> data_reversed;
      GraphPlotter::showUpdateMessage(state);
      update_data(data_reversed.max_size(), data_reversed);
      update_rawid();
      GraphPlotter::hideUpdateMessage(state);
      GraphPlotter::plot(state, data_reversed);
      GraphPlotter::grid(state);
    } else {
      GraphPlotter::showUpdateMessage(state);
    }
  }

private:
  GraphPlotter::State state;
  int64_t rawid;
  //
  void init_rawid() { rawid = INT64_MIN; }
  //
  void update_rawid() {
    rawid = Peripherals::getInstance().local_database.rowid_total_voc;
  }
  //
  template <class T, std::size_t N>
  std::array<T, N> &update_data(std::size_t count,
                                std::array<T, N> &data_reversed) {
    Peripherals &peri = Peripherals::getInstance();
    // 最新のデータを得る
    auto callback = [&](size_t counter, time_t, uint16_t val, uint16_t, bool) {
      data_reversed[counter] = val;
      counter = counter + 1;
      return true;
    };
    peri.local_database.get_total_vocs_desc(peri.sgp30.getSensorDescriptor().id,
                                            count, callback);
    return data_reversed;
  }
  //
  bool need_for_update() {
    return rawid != Peripherals::getInstance().local_database.rowid_total_voc;
  }
};

//
class EquivalentCo2GraphView : public Screen::View {
public:
  EquivalentCo2GraphView() : View(Screen::IdEquivalentCo2GraphView) {}
  //
  bool focusIn() override {
    state = GraphPlotter::State{
        .message_text_color = TFT_WHITE,
        .background_color = TFT_BLACK,
        .grid_color = Screen::lcd.color888(200, 200, 200),
        .line_color = Screen::lcd.color888(232, 120, 120),
        .graph_offset = GraphPlotter::calculateGraphOffset(
            Screen::lcd.width(), Screen::lcd.height()),
        .range =
            Range{
                .min = 0.0,
                .max = 4000.0,
            },
        .locators = {400.0, 1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0},
    };
    init_rawid();
    GraphPlotter::prepare(state, u8"二酸化炭素相当量(eCo2) ppm");
    return true;
  }
  //
  void focusOut() override {}
  //
  Screen::RegisteredViewId releaseEvent(Event &e) override { return getId(); }
  //
  void render(const struct tm &local) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)を邪魔しない時間を選んで作業を開始する。
    //
    if (1 <= local.tm_sec && local.tm_sec <= 55) {
      std::array<uint16_t, GraphPlotter::graph_width / 2> data_reversed;
      GraphPlotter::showUpdateMessage(state);
      update_data(data_reversed.max_size(), data_reversed);
      update_rawid();
      GraphPlotter::hideUpdateMessage(state);
      GraphPlotter::plot(state, data_reversed);
      GraphPlotter::grid(state);
    } else {
      GraphPlotter::showUpdateMessage(state);
    }
  }

private:
  GraphPlotter::State state;
  int64_t rawid;
  //
  void init_rawid() { rawid = INT64_MIN; }
  //
  void update_rawid() {
    rawid = Peripherals::getInstance().local_database.rowid_carbon_dioxide;
  }
  //
  template <class T, std::size_t N>
  std::array<T, N> &update_data(std::size_t count,
                                std::array<T, N> &data_reversed) {
    Peripherals &peri = Peripherals::getInstance();
    // 最新のデータを得る
    auto callback = [&](size_t counter, time_t, uint16_t val, uint16_t, bool) {
      data_reversed[counter] = val;
      counter = counter + 1;
      return true;
    };
    peri.local_database.get_carbon_deoxides_desc(
        peri.sgp30.getSensorDescriptor().id, count, callback);
    return data_reversed;
  }
  //
  bool need_for_update() {
    return rawid !=
           Peripherals::getInstance().local_database.rowid_carbon_dioxide;
  }
};

//
class Co2GraphView : public Screen::View {
public:
  Co2GraphView() : View(Screen::IdCo2GraphView) {}
  //
  bool focusIn() override {
    state = GraphPlotter::State{
        .message_text_color = TFT_WHITE,
        .background_color = TFT_BLACK,
        .grid_color = Screen::lcd.color888(200, 200, 200),
        .line_color = Screen::lcd.color888(232, 120, 120),
        .graph_offset = GraphPlotter::calculateGraphOffset(
            Screen::lcd.width(), Screen::lcd.height()),
        .range =
            Range{
                .min = 0.0,
                .max = 4000.0,
            },
        .locators = {400.0, 1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0},
    };
    init_rawid();
    GraphPlotter::prepare(state, u8"二酸化炭素(CO2) ppm");
    return true;
  }
  //
  void focusOut() override {}
  //
  Screen::RegisteredViewId releaseEvent(Event &e) override { return getId(); }
  //
  void render(const struct tm &local) override {
    if (!need_for_update()) {
      return;
    }
    //
    // 次回の測定(毎分0秒)を邪魔しない時間を選んで作業を開始する。
    //
    if (1 <= local.tm_sec && local.tm_sec <= 55) {
      std::array<uint16_t, GraphPlotter::graph_width / 2> data_reversed;
      GraphPlotter::showUpdateMessage(state);
      update_data(data_reversed.max_size(), data_reversed);
      update_rawid();
      GraphPlotter::hideUpdateMessage(state);
      GraphPlotter::plot(state, data_reversed);
      GraphPlotter::grid(state);
    } else {
      GraphPlotter::showUpdateMessage(state);
    }
  }

private:
  GraphPlotter::State state;
  int64_t rawid;
  //
  void init_rawid() { rawid = INT64_MIN; }
  //
  void update_rawid() {
    rawid = Peripherals::getInstance().local_database.rowid_carbon_dioxide;
  }
  //
  template <class T, std::size_t N>
  std::array<T, N> &update_data(std::size_t count,
                                std::array<T, N> &data_reversed) {
    Peripherals &peri = Peripherals::getInstance();
    // 最新のデータを得る
    auto callback = [&](size_t counter, time_t, uint16_t val, uint16_t, bool) {
      data_reversed[counter] = val;
      counter = counter + 1;
      return true;
    };
    peri.local_database.get_carbon_deoxides_desc(
        peri.scd30.getSensorDescriptor().id, count, callback);
    return data_reversed;
  }
  //
  bool need_for_update() {
    return rawid !=
           Peripherals::getInstance().local_database.rowid_carbon_dioxide;
  }
};

//
LGFX Screen::lcd;

//
Screen::Screen()
    : views{new ClockView(),
            new SummaryView(),
            new TemperatureGraphView(),
            new RelativeHumidityGraphView(),
            new PressureGraphView(),
            new TotalVocGraphView(),
            new EquivalentCo2GraphView(),
            new Co2GraphView(),
            new SystemHealthView(),
            },
      now_view{0} {}

//
Screen::~Screen() {
  for (std::size_t i = 0; i < views.size(); ++i) {
    delete views[i];
  }
}

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
  auto go = (now_view + views.size() - 1) % views.size();
  moveToView(go);
}

//
void Screen::next() {
  auto go = (now_view + 1) % views.size();
  moveToView(go);
}

//
bool Screen::moveByViewId(RegisteredViewId view_id) {
  for (std::size_t i = 0; i < views.size(); ++i) {
    if (views[i]->isThisId(view_id)) {
      return moveToView(i);
    }
  }
  return false;
}

//
void Screen::update(std::time_t time) {
  // time zone offset UTC+9 = asia/tokyo
  std::time_t local_time = time + 9 * 60 * 60;
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
void Screen::releaseEvent(Event &e) {
  RegisteredViewId nextId;
  nextId = views[now_view]->releaseEvent(e);
  if (!views[now_view]->isThisId(nextId)) {
    moveByViewId(nextId);
  }
}
