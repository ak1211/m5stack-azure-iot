// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#ifndef MOVING_AVERAGE_HPP
#define MOVING_AVERAGE_HPP

#include <cstdint>

template <uint8_t N, class value_t, class sum_t> class SimpleMovingAverage {
public:
  static_assert(N > 0, "N must be a natural number.");
  //
  SimpleMovingAverage() : ready_to_go{false}, position{}, ring{} {};
  //
  inline bool ready() { return ready_to_go; }
  void push_back(value_t v) {
    ring[position] = v;
    if ((position + 1) < N) {
      position = position + 1;
    } else {
      ready_to_go = true;
      position = 0;
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
  bool ready_to_go;
  uint_fast16_t position;
  value_t ring[N];
};

#endif // MOVING_AVERAGE_HPP
