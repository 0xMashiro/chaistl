// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_multiset.hpp>

#include <algorithm>
#include <unordered_set>
#include <vector>

namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

std::vector<int> values(const chaistl::unordered_multiset<int>& set) {
  return {set.begin(), set.end()};
}

TEST(UnorderedMultisetModifiers, InsertDuplicatesKeepEquivalentKeysContiguous) {
  chaistl::unordered_multiset<int> set;

  auto first = set.insert(1);
  set.insert(2);
  auto second = set.insert(1);
  set.insert(3);
  auto third = set.emplace(1);

  EXPECT_EQ(*first, 1);
  EXPECT_EQ(*second, 1);
  EXPECT_EQ(*third, 1);
  EXPECT_EQ(set.count(1), 3u);
  EXPECT_THAT(values(set), ElementsAre(1, 1, 1, 2, 3));
  auto [range_first, range_last] = set.equal_range(1);
  EXPECT_EQ(std::distance(range_first, range_last), 3);
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedMultisetModifiers, RehashPreservesEquivalentKeyAdjacency) {
  chaistl::unordered_multiset<int> set;
  for (int value : {1, 2, 1, 3, 1, 4, 2, 2}) {
    set.insert(value);
  }

  set.rehash(97);

  EXPECT_THAT(values(set), ElementsAre(1, 1, 1, 2, 2, 2, 3, 4));
  EXPECT_EQ(set.count(1), 3u);
  EXPECT_EQ(set.count(2), 3u);
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedMultisetModifiers, EraseByKeyRemovesWholeSegment) {
  chaistl::unordered_multiset<int> set{1, 2, 1, 3, 1};

  EXPECT_EQ(set.erase(1), 3u);
  EXPECT_EQ(set.erase(1), 0u);
  EXPECT_THAT(set, UnorderedElementsAre(2, 3));
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedMultisetModifiers, InsertRangeMatchesStdMultiplicity) {
  const std::vector<int> input{1, 2, 1, 3, 2, 1};
  chaistl::unordered_multiset<int> actual;
  std::unordered_multiset<int> expected;

  actual.insert_range(input);
  expected.insert(input.begin(), input.end());

  for (int key : {1, 2, 3, 4}) {
    EXPECT_EQ(actual.count(key), expected.count(key));
  }
  EXPECT_TRUE(actual.validate());
}

}  // namespace
