// SPDX-License-Identifier: Apache-2.0

// References:
// - Heterogeneous lookup in associative containers (C++14):
//   https://en.cppreference.com/w/cpp/container/map/find

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/map.hpp>

#include <string>

namespace {

using ::testing::ElementsAre;
using ::testing::Pair;

struct string_less {
  using is_transparent = void;

  template <class L, class R>
  [[nodiscard]] constexpr bool operator()(const L& lhs, const R& rhs) const {
    return lhs < rhs;
  }
};

TEST(MapTransparent, FindWithDifferentType) {
  chaistl::map<std::string, int, string_less> m{{"alpha", 1}, {"beta", 2}};

  auto it = m.find("beta");
  EXPECT_NE(it, m.end());
  EXPECT_EQ(it->second, 2);

  EXPECT_EQ(m.find("missing"), m.end());
}

TEST(MapTransparent, ContainsWithDifferentType) {
  chaistl::map<std::string, int, string_less> m{{"alpha", 1}};

  EXPECT_TRUE(m.contains("alpha"));
  EXPECT_FALSE(m.contains("beta"));
}

TEST(MapTransparent, CountWithDifferentType) {
  chaistl::map<std::string, int, string_less> m{{"alpha", 1}};

  EXPECT_EQ(m.count("alpha"), 1uz);
  EXPECT_EQ(m.count("beta"), 0uz);
}

TEST(MapTransparent, LowerBoundWithDifferentType) {
  chaistl::map<std::string, int, string_less> m{{"a", 1}, {"c", 3}};

  EXPECT_EQ(m.lower_bound("b")->first, "c");
  EXPECT_EQ(m.lower_bound("d"), m.end());
}

TEST(MapTransparent, UpperBoundWithDifferentType) {
  chaistl::map<std::string, int, string_less> m{{"a", 1}, {"c", 3}};

  EXPECT_EQ(m.upper_bound("a")->first, "c");
  EXPECT_EQ(m.upper_bound("c"), m.end());
}

TEST(MapTransparent, EqualRangeWithDifferentType) {
  chaistl::map<std::string, int, string_less> m{{"a", 1}, {"c", 3}};

  auto [first, last] = m.equal_range("c");
  EXPECT_EQ(first->first, "c");
  EXPECT_EQ(last, m.end());
}

TEST(MapTransparent, EraseWithDifferentType) {
  chaistl::map<std::string, int, string_less> m{{"alpha", 1}, {"beta", 2}};

  EXPECT_EQ(m.erase("alpha"), 1uz);
  EXPECT_THAT(m, ElementsAre(Pair("beta", 2)));
}

// P2077R3 (C++23): Heterogeneous erasure — extract() with transparent comparator.
TEST(MapTransparent, ExtractWithDifferentType) {
  chaistl::map<std::string, int, string_less> m{{"alpha", 1}, {"beta", 2}, {"gamma", 3}};

  auto node = m.extract("beta");
  EXPECT_TRUE(node);
  EXPECT_EQ(node.key(), "beta");
  EXPECT_EQ(node.mapped(), 2);
  EXPECT_THAT(m, ElementsAre(Pair("alpha", 1), Pair("gamma", 3)));

  // Extract missing key returns empty node
  auto missing = m.extract("missing");
  EXPECT_FALSE(missing);
}

}  // namespace
