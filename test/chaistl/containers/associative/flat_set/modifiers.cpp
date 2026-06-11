// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_set.hpp>

namespace {

using ::testing::ElementsAre;

TEST(FlatSetModifiers, InsertReturnsPositionAndInsertedFlag) {
  chaistl::flat_set<int> s{1, 3};

  auto [inserted_it, inserted] = s.insert(2);
  auto [duplicate_it, duplicate] = s.insert(2);

  EXPECT_TRUE(inserted);
  EXPECT_EQ(*inserted_it, 2);
  EXPECT_FALSE(duplicate);
  EXPECT_EQ(*duplicate_it, 2);
  EXPECT_THAT(s, ElementsAre(1, 2, 3));
}

TEST(FlatSetModifiers, HintInsertUsesValidHint) {
  chaistl::flat_set<int> s{1, 3};

  auto it = s.insert(s.lower_bound(3), 2);

  EXPECT_EQ(*it, 2);
  EXPECT_THAT(s, ElementsAre(1, 2, 3));
}

TEST(FlatSetModifiers, RangeInsertSortsMergesAndDeduplicates) {
  chaistl::flat_set<int> s{2, 6};
  int values[] = {5, 2, 4, 1, 4};

  s.insert(std::begin(values), std::end(values));

  EXPECT_THAT(s, ElementsAre(1, 2, 4, 5, 6));
}

TEST(FlatSetModifiers, SortedUniqueRangeInsertMergesAndDeduplicatesAgainstExisting) {
  chaistl::flat_set<int> s{2, 6};
  int values[] = {1, 2, 4};

  s.insert(chaistl::sorted_unique, std::begin(values), std::end(values));

  EXPECT_THAT(s, ElementsAre(1, 2, 4, 6));
}

TEST(FlatSetModifiers, EraseByPositionRangeAndKey) {
  chaistl::flat_set<int> s{1, 2, 3, 4, 5};

  auto it = s.erase(s.find(2));
  EXPECT_EQ(*it, 3);

  EXPECT_EQ(s.erase(4), 1uz);
  EXPECT_EQ(s.erase(99), 0uz);

  s.erase(s.begin(), s.lower_bound(3));
  EXPECT_THAT(s, ElementsAre(3, 5));
}

TEST(FlatSetModifiers, ExtractLeavesSetEmpty) {
  chaistl::flat_set<int> s{1, 2, 3};

  auto storage = std::move(s).extract();

  EXPECT_THAT(storage, ElementsAre(1, 2, 3));
  EXPECT_TRUE(s.empty());
}

TEST(FlatSetModifiers, ReplaceAdoptsSortedUniqueStorage) {
  chaistl::flat_set<int> s{1};
  chaistl::vector<int> storage{2, 4, 6};

  s.replace(std::move(storage));

  EXPECT_THAT(s, ElementsAre(2, 4, 6));
}

TEST(FlatSetModifiers, EraseIfRemovesMatchingElements) {
  chaistl::flat_set<int> s{1, 2, 3, 4, 5};

  auto removed = erase_if(s, [](int value) {
    return value % 2 == 0;
  });

  EXPECT_EQ(removed, 2uz);
  EXPECT_THAT(s, ElementsAre(1, 3, 5));
}

}  // namespace
