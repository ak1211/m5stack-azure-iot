// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef SCREEN_HPP
#define SCREEN_HPP

#include "local_database.hpp"
#include <M5Core2.h>
#include <array>
#include <ctime>

#include <LovyanGFX.hpp>

class Screen {
public:
  class View;
  static LGFX lcd;
  //
  Screen(LocalDatabase &local_database);
  ~Screen();
  //
  bool begin(int32_t text_color = TFT_WHITE, int32_t bg_color = TFT_BLACK);
  //
  void update(std::time_t time);
  void repaint(std::time_t time);
  //
  void releaseEvent(Event &e);
  //
  void home();
  void prev();
  void next();
  //
  bool moveByViewId(uint32_t view_id);

private:
  std::array<View *, 9> views;
  int8_t now_view;
  //
  bool moveToView(int go_view);
};

#endif // SCREEN_HPP
