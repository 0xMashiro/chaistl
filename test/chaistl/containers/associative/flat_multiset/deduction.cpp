// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/flat_multiset.hpp>

#include <functional>
#include <iterator>
#include <type_traits>
#include <vector>

namespace {

TEST(FlatMultisetDeduction, FromContainer) {
  chaistl::vector<int> keys{3, 1, 2, 2};

  chaistl::flat_multiset s(std::move(keys));

  static_assert(std::is_same_v<decltype(s), chaistl::flat_multiset<int>>);
  EXPECT_EQ(s.size(), 4uz);
  EXPECT_EQ(s.count(2), 2uz);
  auto [first, last] = s.equal_range(2);
  EXPECT_EQ(std::distance(first, last), 2);
}

TEST(FlatMultisetDeduction, FromSortedEquivalentContainerWithComparator) {
  chaistl::vector<int> keys{3, 2, 2, 1};

  chaistl::flat_multiset s(chaistl::sorted_equivalent, std::move(keys), std::greater<int>{});

  static_assert(std::is_same_v<decltype(s), chaistl::flat_multiset<int, std::greater<int>, chaistl::vector<int>>>);
  EXPECT_EQ(*s.begin(), 3);
  EXPECT_EQ(s.size(), 4uz);
  EXPECT_EQ(s.count(2), 2uz);
}

TEST(FlatMultisetDeduction, FromIteratorPair) {
  std::vector<int> source{2, 1, 2};

  chaistl::flat_multiset s(source.begin(), source.end());

  static_assert(std::is_same_v<decltype(s), chaistl::flat_multiset<int>>);
  EXPECT_EQ(s.size(), 3uz);
  EXPECT_EQ(s.count(2), 2uz);
}

TEST(FlatMultisetDeduction, FromRange) {
  std::vector<int> source{1, 2, 2, 3};

  chaistl::flat_multiset s(std::from_range, source);

  static_assert(std::is_same_v<decltype(s), chaistl::flat_multiset<int>>);
  EXPECT_TRUE(s.contains(2));
  EXPECT_EQ(s.count(2), 2uz);
}

TEST(FlatMultisetDeduction, FromInitializerList) {
  chaistl::flat_multiset s{1, 2, 3, 3};

  static_assert(std::is_same_v<decltype(s), chaistl::flat_multiset<int>>);
  EXPECT_TRUE(s.contains(3));
  EXPECT_EQ(s.size(), 4uz);
  EXPECT_EQ(s.count(3), 2uz);
}

}  // namespace
