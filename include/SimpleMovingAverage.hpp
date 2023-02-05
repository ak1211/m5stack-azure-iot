// Copyright (c) 2021 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include <array>
#include <cstdint>
#include <numeric>

template <uint8_t N, typename value_t, typename sum_t>
class SimpleMovingAverage final {
  bool ready_to_go;
  std::array<value_t, N> ring;
  typename decltype(ring)::iterator itr;

public:
  SimpleMovingAverage() : ready_to_go{false}, ring{}, itr{ring.begin()} {};
  bool ready() const { return ready_to_go; }
  void push_back(value_t input) {
    if (itr == ring.end()) {
      itr = ring.begin();
      ready_to_go = true;
    }
    *itr++ = input;
  }
  value_t calculate() const {
    static_assert(N > 0, "N must be a natural number.");
    sum_t summary = std::accumulate(ring.begin(), ring.end(), sum_t(0));
    return summary / N;
  }
};
