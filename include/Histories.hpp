// Copyright (c) 2022 Akihiro Yamamoto.
// Licensed under the MIT License <https://spdx.org/licenses/MIT.html>
// See LICENSE file in the project root for full license information.
//
#pragma once
#include <array>
#include <functional>
#include <iterator>
#include <optional>
#include <tuple>

//
//
//
template <typename T, std::size_t N> class Histories final {
  bool ring_filled;
  std::array<T, N> ring;
  typename decltype(ring)::iterator head;
  typename decltype(ring)::const_iterator chead() const noexcept {
    return head;
  }

public:
  constexpr Histories() : head{ring.begin()}, ring_filled{false} {}
  //
  bool filled() const noexcept { return ring_filled; }
  //
  bool empty() const noexcept { return !ring_filled && head == ring.begin(); }
  //
  std::size_t size() const noexcept {
    if (filled()) {
      return ring.size();
    } else {
      return std::distance(ring.cbegin(), chead());
    }
  }
  //
  void insert(T value) noexcept {
    if (head == ring.end()) {
      head = ring.begin();
      ring_filled = true;
    }
    *head++ = value;
  }
  //
  std::optional<T> getLatestValue() const noexcept {
    if (empty()) {
      return std::nullopt;
    }
    auto it = (head == ring.begin()) ? std::prev(ring.end()) : std::prev(head);
    return std::make_optional(*it);
  }
  //
  std::vector<T> getHistories() const noexcept {
    std::vector<T> result;
    if (filled()) {
      result.reserve(N);
      std::copy(chead(), ring.cend(), std::back_inserter(result));
      std::copy(ring.cbegin(), chead(), std::back_inserter(result));
    } else {
      result.reserve(std::distance(ring.cbegin(), chead()));
      std::copy(ring.cbegin(), chead(), std::back_inserter(result));
    }
    return result;
  }
};
