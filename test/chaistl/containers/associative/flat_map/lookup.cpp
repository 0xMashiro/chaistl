// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_map.hpp>

#include <string>
#include <string_view>

namespace {

using ::testing::ElementsAre;

struct string_less {
  using is_transparent = void;

  constexpr bool operator()(std::string_view lhs, std::string_view rhs) const noexcept { return lhs < rhs; }
};

TEST(FlatMapLookup, FindContainsCount) {
  chaistl::flat_map<int, std::string> m{{1, "one"}, {3, "three"}, {5, "five"}};

  auto it = m.find(3);
  ASSERT_NE(it, m.end());
  EXPECT_EQ(it->first, 3);
  EXPECT_EQ(it->second, "three");
  EXPECT_TRUE(m.contains(5));
  EXPECT_FALSE(m.contains(4));
  EXPECT_EQ(m.count(1), 1uz);
  EXPECT_EQ(m.count(2), 0uz);
}

TEST(FlatMapLookup, BoundsAndEqualRange) {
  chaistl::flat_map<int, std::string> m{{1, "one"}, {3, "three"}, {5, "five"}};

  EXPECT_EQ(m.lower_bound(2)->first, 3);
  EXPECT_EQ(m.upper_bound(3)->first, 5);

  auto [first, last] = m.equal_range(3);
  ASSERT_NE(first, last);
  EXPECT_EQ(first->second, "three");
  ++first;
  EXPECT_EQ(first, last);
}

TEST(FlatMapLookup, ReverseIteration) {
  chaistl::flat_map<int, std::string> m{{1, "one"}, {3, "three"}, {5, "five"}};

  EXPECT_EQ(m.rbegin()->first, 5);
}

TEST(FlatMapLookup, HeterogeneousLookup) {
  chaistl::flat_map<std::string, int, string_less> m{{"alpha", 1}, {"beta", 2}, {"gamma", 3}};

  EXPECT_TRUE(m.contains(std::string_view("beta")));
  EXPECT_EQ(m.find(std::string_view("gamma"))->second, 3);
  EXPECT_EQ(m.erase(std::string_view("alpha")), 1uz);
  EXPECT_THAT(m.keys(), ElementsAre("beta", "gamma"));
}

}  // namespace
