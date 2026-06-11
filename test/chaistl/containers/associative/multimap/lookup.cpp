// SPDX-License-Identifier: Apache-2.0

// References:
// - std::multimap lookup:
//   https://en.cppreference.com/w/cpp/container/multimap
//   https://eel.is/c++draft/associative.reqmts

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/multimap.hpp>

#include <functional>
#include <iterator>
#include <string>
#include <string_view>

namespace {

TEST(MultimapLookup, CountAcrossRuns) {
  chaistl::multimap<int, int> mm{{1, 1}, {2, 1}, {2, 2}, {3, 1}, {3, 2}, {3, 3}};

  EXPECT_EQ(mm.count(0), 0uz);
  EXPECT_EQ(mm.count(1), 1uz);
  EXPECT_EQ(mm.count(2), 2uz);
  EXPECT_EQ(mm.count(3), 3uz);
}

TEST(MultimapLookup, FindReturnsFirstOfRun) {
  chaistl::multimap<int, int> mm{{1, 1}, {2, 1}, {2, 2}};

  auto it = mm.find(2);
  ASSERT_NE(it, mm.end());
  EXPECT_EQ(it->second, 1);  // first inserted of the run
  EXPECT_EQ(mm.find(42), mm.end());
}

TEST(MultimapLookup, EqualRangeAndBounds) {
  chaistl::multimap<int, int> mm{{1, 1}, {2, 1}, {2, 2}, {4, 1}};

  auto [first, last] = mm.equal_range(2);
  EXPECT_EQ(std::distance(first, last), 2);

  EXPECT_EQ(mm.lower_bound(2)->second, 1);
  EXPECT_EQ(mm.upper_bound(2)->first, 4);
  EXPECT_TRUE(mm.contains(2));
  EXPECT_FALSE(mm.contains(3));
}

TEST(MultimapLookup, HeterogeneousLookupAndErase) {
  chaistl::multimap<std::string, int, std::less<>> mm{{"a", 1}, {"b", 1}, {"b", 2}, {"c", 1}};
  constexpr std::string_view key = "b";

  EXPECT_EQ(mm.count(key), 2uz);
  EXPECT_TRUE(mm.contains(key));
  auto [first, last] = mm.equal_range(key);
  EXPECT_EQ(std::distance(first, last), 2);

  auto nh = mm.extract(key);
  ASSERT_FALSE(nh.empty());
  EXPECT_EQ(nh.key(), "b");

  EXPECT_EQ(mm.erase(key), 1uz);  // the remaining element of the run
  EXPECT_EQ(mm.count(key), 0uz);
  EXPECT_TRUE(mm.validate());
}

}  // namespace
