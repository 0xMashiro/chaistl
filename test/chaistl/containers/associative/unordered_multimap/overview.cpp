// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/unordered_multimap.hpp>
#include <chaistl/functional/hash.hpp>

#include <concepts>
#include <iterator>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>

namespace {

using int_map = chaistl::unordered_multimap<int, std::string>;

static_assert(std::is_same_v<int_map::key_type, int>);
static_assert(std::is_same_v<int_map::mapped_type, std::string>);
static_assert(std::is_same_v<int_map::value_type, std::pair<const int, std::string>>);
static_assert(!std::is_same_v<int_map::iterator, int_map::const_iterator>);
static_assert(std::forward_iterator<int_map::iterator>);
static_assert(std::ranges::forward_range<int_map>);
static_assert(std::equality_comparable<int_map>);

constexpr bool constexpr_smoke() {
  chaistl::unordered_multimap<int, int, chaistl::hash<int>> map;
  map.insert({1, 10});
  map.insert({2, 20});
  map.insert({1, 11});
  bool ok = map.size() == 3 && map.count(1) == 2;
  auto [first, last] = map.equal_range(1);
  int seen = 0;
  for (; first != last; ++first) {
    ok = ok && first->first == 1;
    ++seen;
  }
  ok = ok && seen == 2;
  ok = ok && map.erase(1) == 2 && !map.contains(1);
  return ok && map.validate();
}

static_assert(constexpr_smoke());

TEST(UnorderedMultimapOverview, BasicUsage) {
  int_map map{{1, "one"}, {1, "uno"}, {2, "two"}};

  EXPECT_EQ(map.size(), 3u);
  EXPECT_EQ(map.count(1), 2u);
  EXPECT_TRUE(map.contains(2));
  EXPECT_TRUE(map.validate());
}

}  // namespace
