// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_multiset.hpp>

#include <array>
#include <ranges>
#include <type_traits>

namespace {

using ::testing::ElementsAre;

TEST(FlatMultisetCons, InitializerListSortsAndKeepsDuplicates) {
  chaistl::flat_multiset<int> s{5, 1, 3, 1, 5, 2};

  EXPECT_THAT(s, ElementsAre(1, 1, 2, 3, 5, 5));
  EXPECT_TRUE(s.validate());
}

TEST(FlatMultisetCons, ContainerConstructorSortsAndKeepsDuplicates) {
  chaistl::vector<int> values{4, 2, 4, 1};

  chaistl::flat_multiset<int> s(std::move(values));

  EXPECT_THAT(s, ElementsAre(1, 2, 4, 4));
}

TEST(FlatMultisetCons, SortedEquivalentConstructorAdoptsContainer) {
  chaistl::vector<int> values{1, 2, 2, 4};

  chaistl::flat_multiset<int> s(chaistl::sorted_equivalent, std::move(values));

  EXPECT_THAT(s, ElementsAre(1, 2, 2, 4));
  EXPECT_TRUE(s.validate());
}

TEST(FlatMultisetCons, RangeAndFromRangeConstructors) {
  std::array values{3, 1, 2, 3};

  chaistl::flat_multiset<int> by_iter(values.begin(), values.end());
  chaistl::flat_multiset<int> by_range(std::from_range, values);

  EXPECT_THAT(by_iter, ElementsAre(1, 2, 3, 3));
  EXPECT_THAT(by_range, ElementsAre(1, 2, 3, 3));
}

TEST(FlatMultisetCons, AssignmentFromInitializerList) {
  chaistl::flat_multiset<int> s{1, 2};

  s = {5, 3, 5, 4};

  EXPECT_THAT(s, ElementsAre(3, 4, 5, 5));
}

TEST(FlatMultisetCons, DeductionGuides) {
  chaistl::vector<int> storage{1, 1, 2};
  chaistl::flat_multiset from_container(storage);
  static_assert(std::is_same_v<decltype(from_container), chaistl::flat_multiset<int>>);

  std::array values{3, 3, 4};
  chaistl::flat_multiset from_iters(values.begin(), values.end());
  static_assert(std::is_same_v<decltype(from_iters), chaistl::flat_multiset<int>>);

  chaistl::flat_multiset from_init{1, 1, 2};
  static_assert(std::is_same_v<decltype(from_init), chaistl::flat_multiset<int>>);
}

}  // namespace
