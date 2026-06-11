// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/unordered_multiset.hpp>
#include <chaistl/functional/hash.hpp>

#include <concepts>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <vector>

namespace {

using int_multiset = chaistl::unordered_multiset<int>;

static_assert(std::is_same_v<int_multiset::key_type, int>);
static_assert(std::is_same_v<int_multiset::value_type, int>);
static_assert(std::is_same_v<int_multiset::iterator, int_multiset::const_iterator>);
static_assert(std::forward_iterator<int_multiset::iterator>);
static_assert(std::ranges::forward_range<int_multiset>);
static_assert(std::equality_comparable<int_multiset>);

constexpr bool constexpr_smoke() {
  chaistl::unordered_multiset<int, chaistl::hash<int>> set;
  set.insert(1);
  set.insert(2);
  set.insert(1);
  set.insert(1);
  bool ok = set.size() == 4 && set.count(1) == 3 && set.count(2) == 1;
  auto [first, last] = set.equal_range(1);
  int seen = 0;
  for (; first != last; ++first) {
    ok = ok && *first == 1;
    ++seen;
  }
  ok = ok && seen == 3;
  ok = ok && set.erase(1) == 3 && !set.contains(1) && set.size() == 1;
  return ok && set.validate();
}

static_assert(constexpr_smoke());

TEST(UnorderedMultisetOverview, BasicUsage) {
  int_multiset set{1, 2, 1};

  EXPECT_EQ(set.size(), 3u);
  EXPECT_EQ(set.count(1), 2u);
  EXPECT_TRUE(set.contains(2));
  EXPECT_TRUE(set.validate());
}

}  // namespace
