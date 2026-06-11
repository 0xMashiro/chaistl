// SPDX-License-Identifier: Apache-2.0

// References:
// - Heterogeneous lookup in associative containers (C++14):
//   https://en.cppreference.com/w/cpp/container/set/find

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/set.hpp>

#include <string>

namespace {

using ::testing::ElementsAre;

struct string_less {
  using is_transparent = void;

  template <class L, class R>
  [[nodiscard]] constexpr bool operator()(const L& lhs, const R& rhs) const {
    return lhs < rhs;
  }
};

TEST(SetTransparent, FindWithDifferentType) {
  chaistl::set<std::string, string_less> s{"alpha", "beta", "gamma"};

  auto it = s.find("beta");
  EXPECT_NE(it, s.end());
  EXPECT_EQ(*it, "beta");

  EXPECT_EQ(s.find("missing"), s.end());
}

TEST(SetTransparent, ContainsWithDifferentType) {
  chaistl::set<std::string, string_less> s{"alpha"};

  EXPECT_TRUE(s.contains("alpha"));
  EXPECT_FALSE(s.contains("beta"));
}

TEST(SetTransparent, CountWithDifferentType) {
  chaistl::set<std::string, string_less> s{"alpha"};

  EXPECT_EQ(s.count("alpha"), 1uz);
  EXPECT_EQ(s.count("beta"), 0uz);
}

TEST(SetTransparent, LowerBoundWithDifferentType) {
  chaistl::set<std::string, string_less> s{"a", "c", "e"};

  EXPECT_EQ(*s.lower_bound("b"), "c");
  EXPECT_EQ(s.lower_bound("f"), s.end());
}

TEST(SetTransparent, UpperBoundWithDifferentType) {
  chaistl::set<std::string, string_less> s{"a", "c", "e"};

  EXPECT_EQ(*s.upper_bound("a"), "c");
  EXPECT_EQ(s.upper_bound("e"), s.end());
}

TEST(SetTransparent, EqualRangeWithDifferentType) {
  chaistl::set<std::string, string_less> s{"a", "c", "e"};

  auto [first, last] = s.equal_range("c");
  EXPECT_EQ(*first, "c");
  EXPECT_EQ(*last, "e");
}

TEST(SetTransparent, EraseWithDifferentType) {
  chaistl::set<std::string, string_less> s{"alpha", "beta"};

  EXPECT_EQ(s.erase("alpha"), 1uz);
  EXPECT_THAT(s, ElementsAre("beta"));
}

// P2077R3 (C++23): Heterogeneous erasure — extract() with transparent comparator.
TEST(SetTransparent, ExtractWithDifferentType) {
  chaistl::set<std::string, string_less> s{"alpha", "beta", "gamma"};

  auto node = s.extract("beta");
  EXPECT_TRUE(node);
  EXPECT_EQ(node.value(), "beta");
  EXPECT_THAT(s, ElementsAre("alpha", "gamma"));

  // Extract missing key returns empty node
  auto missing = s.extract("missing");
  EXPECT_FALSE(missing);
}

}  // namespace
