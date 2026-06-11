// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_set.hpp>

#include <vector>

namespace {

using ::testing::ElementsAre;
using ::testing::Pair;

TEST(FlatSetLookup, FindContainsCount) {
  chaistl::flat_set<int> s{1, 3, 5};

  EXPECT_EQ(*s.find(3), 3);
  EXPECT_EQ(s.find(2), s.end());
  EXPECT_TRUE(s.contains(5));
  EXPECT_FALSE(s.contains(4));
  EXPECT_EQ(s.count(1), 1uz);
  EXPECT_EQ(s.count(9), 0uz);
}

TEST(FlatSetLookup, Bounds) {
  chaistl::flat_set<int> s{1, 3, 5, 7};

  EXPECT_EQ(*s.lower_bound(2), 3);
  EXPECT_EQ(*s.lower_bound(3), 3);
  EXPECT_EQ(s.lower_bound(8), s.end());

  EXPECT_EQ(*s.upper_bound(3), 5);
  EXPECT_EQ(s.upper_bound(7), s.end());
}

TEST(FlatSetLookup, EqualRangeUsesSingleElementRange) {
  chaistl::flat_set<int> s{1, 3, 5, 7};

  auto [first, last] = s.equal_range(3);

  ASSERT_NE(first, s.end());
  EXPECT_EQ(*first, 3);
  ASSERT_NE(last, s.end());
  EXPECT_EQ(*last, 5);
  EXPECT_EQ(std::distance(first, last), 1);

  auto [missing_first, missing_last] = s.equal_range(4);
  EXPECT_EQ(*missing_first, 5);
  EXPECT_EQ(missing_first, missing_last);
}

TEST(FlatSetLookup, ReverseIteration) {
  chaistl::flat_set<int> s{2, 1, 3};

  std::vector<int> values;
  for (auto it = s.rbegin(); it != s.rend(); ++it) {
    values.push_back(*it);
  }

  EXPECT_THAT(values, ElementsAre(3, 2, 1));
}

TEST(FlatSetLookup, ConstLookup) {
  const chaistl::flat_set<int> s{1, 3, 5};

  EXPECT_EQ(*s.find(3), 3);
  EXPECT_EQ(s.find(2), s.end());
  EXPECT_TRUE(s.contains(5));
  EXPECT_EQ(s.count(1), 1uz);
  EXPECT_EQ(*s.lower_bound(2), 3);
  EXPECT_EQ(*s.upper_bound(3), 5);
}

}  // namespace
