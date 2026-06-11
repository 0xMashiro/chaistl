// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/flat_set.hpp>

#include <functional>
#include <vector>

namespace {

TEST(FlatSetDeduction, FromContainer) {
  chaistl::vector<int> keys{3, 1, 2};

  chaistl::flat_set s(std::move(keys));

  static_assert(std::is_same_v<decltype(s), chaistl::flat_set<int>>);
  EXPECT_EQ(s.size(), 3uz);
}

TEST(FlatSetDeduction, FromSortedUniqueContainerWithComparator) {
  chaistl::vector<int> keys{3, 2, 1};

  chaistl::flat_set s(chaistl::sorted_unique, std::move(keys), std::greater<int>{});

  static_assert(std::is_same_v<decltype(s), chaistl::flat_set<int, std::greater<int>, chaistl::vector<int>>>);
  EXPECT_EQ(*s.begin(), 3);
}

TEST(FlatSetDeduction, FromIteratorPair) {
  std::vector<int> source{2, 1, 2};

  chaistl::flat_set s(source.begin(), source.end());

  static_assert(std::is_same_v<decltype(s), chaistl::flat_set<int>>);
  EXPECT_EQ(s.size(), 2uz);
}

TEST(FlatSetDeduction, FromRange) {
  std::vector<int> source{1, 2, 3};

  chaistl::flat_set s(std::from_range, source);

  static_assert(std::is_same_v<decltype(s), chaistl::flat_set<int>>);
  EXPECT_TRUE(s.contains(2));
}

TEST(FlatSetDeduction, FromInitializerList) {
  chaistl::flat_set s{1, 2, 3};

  static_assert(std::is_same_v<decltype(s), chaistl::flat_set<int>>);
  EXPECT_TRUE(s.contains(3));
}

}  // namespace
