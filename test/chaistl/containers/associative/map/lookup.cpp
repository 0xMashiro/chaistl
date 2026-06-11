// SPDX-License-Identifier: Apache-2.0

// References:
// - std::map lookup:
//   https://en.cppreference.com/w/cpp/container/map
//   https://eel.is/c++draft/map.ops

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/map.hpp>

#include <string>

namespace {

using ::testing::ElementsAre;

TEST(MapLookup, Find) {
  chaistl::map<int, std::string> m{{1, "one"}, {3, "three"}, {5, "five"}};

  auto it = m.find(3);
  EXPECT_NE(it, m.end());
  EXPECT_EQ(it->second, "three");

  EXPECT_EQ(m.find(99), m.end());
}

TEST(MapLookup, Contains) {
  chaistl::map<int, std::string> m{{1, "one"}};

  EXPECT_TRUE(m.contains(1));
  EXPECT_FALSE(m.contains(2));
}

TEST(MapLookup, Count) {
  chaistl::map<int, std::string> m{{1, "one"}};

  EXPECT_EQ(m.count(1), 1uz);
  EXPECT_EQ(m.count(2), 0uz);
}

TEST(MapLookup, LowerBound) {
  chaistl::map<int, std::string> m{{1, "one"}, {3, "three"}, {5, "five"}};

  EXPECT_EQ(m.lower_bound(0)->first, 1);
  EXPECT_EQ(m.lower_bound(1)->first, 1);
  EXPECT_EQ(m.lower_bound(2)->first, 3);
  EXPECT_EQ(m.lower_bound(3)->first, 3);
  EXPECT_EQ(m.lower_bound(6), m.end());
}

TEST(MapLookup, UpperBound) {
  chaistl::map<int, std::string> m{{1, "one"}, {3, "three"}, {5, "five"}};

  EXPECT_EQ(m.upper_bound(0)->first, 1);
  EXPECT_EQ(m.upper_bound(1)->first, 3);
  EXPECT_EQ(m.upper_bound(3)->first, 5);
  EXPECT_EQ(m.upper_bound(5), m.end());
}

TEST(MapLookup, EqualRange) {
  chaistl::map<int, std::string> m{{1, "one"}, {3, "three"}, {5, "five"}};

  auto [first, last] = m.equal_range(3);
  EXPECT_EQ(first->first, 3);
  EXPECT_EQ(last->first, 5);
}

TEST(MapLookup, IterationOrder) {
  chaistl::map<int, std::string> m{{5, "five"}, {2, "two"}, {8, "eight"}, {1, "one"}};

  std::vector<int> keys;
  for (auto& [k, v] : m) {
    keys.push_back(k);
  }
  EXPECT_THAT(keys, ElementsAre(1, 2, 5, 8));
}

}  // namespace
