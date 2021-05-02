// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef MOVING_AVERAGE_HPP
#define MOVING_AVERAGE_HPP

#include <array>
#include <cmath>
#include <cstdint>

template <uint8_t N, class value_t = uint16_t, class sum_t = uint32_t>
class SimpleMovingAverage {
public:
  SimpleMovingAverage() : n_of_pushed(0), position(0), ring{} {};
  //
  bool ready() { return (n_of_pushed >= N); }
  void push_back(value_t v) {
    if (n_of_pushed == 0) {
      for (int_fast8_t i = 0; i < N; ++i) {
        ring[i] = v;
      }
    } else {
      ring[position] = v;
    }
    position = (position + 1) % N;
    if (n_of_pushed < UINT16_MAX) {
      n_of_pushed = n_of_pushed + 1;
    }
  }
  value_t calculate() {
    sum_t sum = ring[0];
    for (uint_fast8_t i = 1; i < N; ++i) {
      sum = sum + ring[i];
    }
    value_t average = sum / N;
    return average;
  }

private:
  uint_fast16_t n_of_pushed;
  uint_fast16_t position;
  value_t ring[N];
};

#endif // MOVING_AVERAGE_HPP
