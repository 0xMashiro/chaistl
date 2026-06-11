// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_set.hpp>

#include <array>
#include <ranges>

namespace {

using ::testing::ElementsAre;

TEST(FlatSetCons, InitializerListSortsAndDeduplicates) {
  chaistl::flat_set<int> s{5, 1, 3, 1, 5, 2};

  EXPECT_THAT(s, ElementsAre(1, 2, 3, 5));
  EXPECT_TRUE(s.validate());
}

TEST(FlatSetCons, ContainerConstructorSortsAndDeduplicates) {
  chaistl::vector<int> values{4, 2, 4, 1};

  chaistl::flat_set<int> s(std::move(values));

  EXPECT_THAT(s, ElementsAre(1, 2, 4));
}

TEST(FlatSetCons, SortedUniqueConstructorAdoptsContainer) {
  chaistl::vector<int> values{1, 2, 4};

  chaistl::flat_set<int> s(chaistl::sorted_unique, std::move(values));

  EXPECT_THAT(s, ElementsAre(1, 2, 4));
  EXPECT_TRUE(s.validate());
}

TEST(FlatSetCons, RangeConstructor) {
  std::array values{3, 1, 2, 3};

  chaistl::flat_set<int> s(values.begin(), values.end());

  EXPECT_THAT(s, ElementsAre(1, 2, 3));
}

TEST(FlatSetCons, FromRangeConstructor) {
  std::array values{4, 2, 2, 1};

  chaistl::flat_set<int> s(std::from_range, values);

  EXPECT_THAT(s, ElementsAre(1, 2, 4));
}

TEST(FlatSetCons, AssignmentFromInitializerList) {
  chaistl::flat_set<int> s{1, 2};

  s = {5, 3, 5, 4};

  EXPECT_THAT(s, ElementsAre(3, 4, 5));
}

}  // namespace
