// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_multiset.hpp>

#include <vector>

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(FlatMultisetModifiers, InsertReturnsIteratorAndAppendsEquivalentKeys) {
  chaistl::flat_multiset<int> s{1, 2, 2, 4};

  auto it = s.insert(2);

  EXPECT_EQ(*it, 2);
  EXPECT_EQ(it - s.begin(), 3);
  EXPECT_THAT(s, ElementsAre(1, 2, 2, 2, 4));
  EXPECT_TRUE(s.validate());
}

TEST(FlatMultisetModifiers, EmplaceAndHintInsert) {
  chaistl::flat_multiset<int> s{1, 3};

  auto emplaced = s.emplace(2);
  auto hinted = s.insert(s.lower_bound(2), 2);

  EXPECT_EQ(*emplaced, 2);
  EXPECT_EQ(*hinted, 2);
  EXPECT_EQ(hinted - s.begin(), 2);
  EXPECT_THAT(s, ElementsAre(1, 2, 2, 3));
  EXPECT_TRUE(s.validate());
}

TEST(FlatMultisetModifiers, InsertRangeMergesExistingEquivalentKeysFirst) {
  chaistl::flat_multiset<int> s{1, 2, 2, 5};
  const std::vector<int> incoming{4, 2, 3, 2};

  s.insert(incoming.begin(), incoming.end());

  EXPECT_THAT(s, ElementsAre(1, 2, 2, 2, 2, 3, 4, 5));
  EXPECT_TRUE(s.validate());
}

TEST(FlatMultisetModifiers, SortedEquivalentInsertRange) {
  chaistl::flat_multiset<int> s{1, 3};
  const std::vector<int> incoming{2, 2, 4};

  s.insert(chaistl::sorted_equivalent, incoming.begin(), incoming.end());

  EXPECT_THAT(s, ElementsAre(1, 2, 2, 3, 4));
  EXPECT_TRUE(s.validate());
}

TEST(FlatMultisetModifiers, EraseByKeyRemovesAllEquivalentKeys) {
  chaistl::flat_multiset<int> s{1, 2, 2, 2, 3};

  EXPECT_EQ(s.erase(2), 3uz);
  EXPECT_EQ(s.erase(99), 0uz);

  EXPECT_THAT(s, ElementsAre(1, 3));
  EXPECT_TRUE(s.validate());
}

TEST(FlatMultisetModifiers, ExtractReplaceEraseIfAndCompare) {
  chaistl::flat_multiset<int> s{1, 2, 2};

  auto storage = std::move(s).extract();
  EXPECT_THAT(s, IsEmpty());
  EXPECT_THAT(storage, ElementsAre(1, 2, 2));

  storage = {3, 3, 4};
  s.replace(std::move(storage));

  EXPECT_THAT(s, ElementsAre(3, 3, 4));
  EXPECT_EQ(erase_if(s,
                     [](int value) {
                       return value == 3;
                     }),
            2uz);
  EXPECT_THAT(s, ElementsAre(4));
  EXPECT_LT(chaistl::flat_multiset<int>{1}, chaistl::flat_multiset<int>{2});
}

}  // namespace
