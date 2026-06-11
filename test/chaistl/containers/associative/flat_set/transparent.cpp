// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_set.hpp>

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

TEST(FlatSetTransparent, HeterogeneousLookup) {
  chaistl::flat_set<std::string, string_less> s{"alpha", "beta", "gamma"};

  EXPECT_EQ(*s.find("beta"), "beta");
  EXPECT_EQ(s.find("missing"), s.end());
  EXPECT_TRUE(s.contains("alpha"));
  EXPECT_EQ(s.count("gamma"), 1uz);
  EXPECT_EQ(*s.lower_bound("b"), "beta");
  EXPECT_EQ(*s.upper_bound("beta"), "gamma");

  auto [first, last] = s.equal_range("beta");
  EXPECT_EQ(*first, "beta");
  EXPECT_EQ(*last, "gamma");
}

TEST(FlatSetTransparent, HeterogeneousInsertAndErase) {
  chaistl::flat_set<std::string, string_less> s{"beta"};

  auto [it, inserted] = s.insert("alpha");

  EXPECT_TRUE(inserted);
  EXPECT_EQ(*it, "alpha");
  EXPECT_FALSE(s.insert("alpha").second);
  EXPECT_EQ(s.erase("beta"), 1uz);
  EXPECT_THAT(s, ElementsAre("alpha"));
}

}  // namespace
