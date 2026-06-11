// SPDX-License-Identifier: Apache-2.0

// References:
// - std::unordered_map:
//   https://en.cppreference.com/w/cpp/container/unordered_map
//   https://eel.is/c++draft/unord.map

#include <gtest/gtest.h>

#include <chaistl/containers/unordered_map.hpp>
#include <chaistl/functional/hash.hpp>

#include <concepts>
#include <iterator>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>

namespace {

using int_map = chaistl::unordered_map<int, std::string>;

// ============================================================================
// Member types and concepts
// ============================================================================

static_assert(std::is_same_v<int_map::key_type, int>);
static_assert(std::is_same_v<int_map::mapped_type, std::string>);
static_assert(std::is_same_v<int_map::value_type, std::pair<const int, std::string>>);

// Unlike set, map's iterator and const_iterator are distinct: the mapped
// value is mutable through the iterator while pair<const Key, T> keeps the
// key immutable.
static_assert(!std::is_same_v<int_map::iterator, int_map::const_iterator>);
static_assert(std::convertible_to<int_map::iterator, int_map::const_iterator>);
static_assert(std::is_same_v<std::iter_reference_t<int_map::iterator>, std::pair<const int, std::string>&>);
static_assert(std::is_same_v<std::iter_reference_t<int_map::const_iterator>, const std::pair<const int, std::string>&>);

static_assert(std::forward_iterator<int_map::iterator>);
static_assert(std::forward_iterator<int_map::const_iterator>);
static_assert(std::ranges::forward_range<int_map>);
static_assert(std::is_nothrow_default_constructible_v<int_map>);

// ============================================================================
// constexpr smoke test (P3372 direction): operator[], try_emplace,
// insert_or_assign, erase — all in constant evaluation.
// ============================================================================

constexpr bool constexpr_smoke() {
  chaistl::unordered_map<int, int, chaistl::hash<int>> map;
  for (int key = 0; key < 30; ++key) {
    map[key] = key * 10;
  }
  bool ok = map.size() == 30 && map.at(7) == 70;

  map[7] = 700;  // hit: assign through the reference
  ok = ok && map.at(7) == 700;

  auto [it, inserted] = map.try_emplace(7, 9999);
  ok = ok && !inserted && it->second == 700;  // hit constructs nothing

  map.insert_or_assign(7, 7000);
  ok = ok && map.at(7) == 7000;

  map.erase(7);
  ok = ok && !map.contains(7) && map.size() == 29;

  int sum = 0;
  for (const auto& [key, value] : map) {
    sum += value - key * 10;
  }
  ok = ok && sum == 0;

  return ok && map.validate();
}

static_assert(constexpr_smoke());

// ============================================================================
// Runtime sanity
// ============================================================================

TEST(UnorderedMapOverview, BasicUsage) {
  int_map map{{1, "one"}, {2, "two"}};

  EXPECT_EQ(map.size(), 2u);
  EXPECT_EQ(map.at(1), "one");

  map[3] = "three";
  EXPECT_EQ(map.size(), 3u);
  EXPECT_TRUE(map.contains(3));

  // The mapped value is mutable through the iterator.
  map.find(2)->second = "TWO";
  EXPECT_EQ(map.at(2), "TWO");
  EXPECT_TRUE(map.validate());
}

}  // namespace
