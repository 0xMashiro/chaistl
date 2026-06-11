// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_multimap.hpp>

#include <string>
#include <utility>
#include <vector>

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(FlatMultimapModifiers, InsertReturnsIteratorAndAppendsEquivalentKeys) {
  chaistl::flat_multimap<int, std::string> m{{1, "one"}, {2, "a"}, {2, "b"}, {4, "four"}};

  auto it = m.insert({2, "c"});

  EXPECT_EQ(it->first, 2);
  EXPECT_EQ(it->second, "c");
  EXPECT_EQ(it - m.begin(), 3);
  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 2, 2, 4));
  EXPECT_THAT(m.values(), ElementsAre("one", "a", "b", "c", "four"));
  EXPECT_TRUE(m.validate());
}

TEST(FlatMultimapModifiers, EmplaceAndHintInsert) {
  chaistl::flat_multimap<int, std::string> m{{1, "one"}, {3, "three"}};

  auto emplaced = m.emplace(2, "a");
  auto hinted = m.insert(m.lower_bound(2), {2, "b"});

  EXPECT_EQ(emplaced->second, "a");
  EXPECT_EQ(hinted->second, "b");
  EXPECT_EQ(hinted - m.begin(), 2);
  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 2, 3));
  EXPECT_THAT(m.values(), ElementsAre("one", "a", "b", "three"));
  EXPECT_TRUE(m.validate());
}

TEST(FlatMultimapModifiers, InsertRangeMergesExistingEquivalentKeysFirst) {
  chaistl::flat_multimap<int, std::string> m{{1, "one"}, {2, "old-a"}, {2, "old-b"}, {5, "five"}};
  const std::vector<std::pair<int, std::string>> incoming{{4, "four"}, {2, "new-a"}, {3, "three"}, {2, "new-b"}};

  m.insert(incoming.begin(), incoming.end());

  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 2, 2, 2, 3, 4, 5));
  EXPECT_THAT(m.values(), ElementsAre("one", "old-a", "old-b", "new-a", "new-b", "three", "four", "five"));
  EXPECT_TRUE(m.validate());
}

TEST(FlatMultimapModifiers, SortedEquivalentInsertRange) {
  chaistl::flat_multimap<int, std::string> m{{1, "one"}, {3, "three"}};
  const std::vector<std::pair<int, std::string>> incoming{{2, "a"}, {2, "b"}, {4, "four"}};

  m.insert(chaistl::sorted_equivalent, incoming.begin(), incoming.end());

  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 2, 3, 4));
  EXPECT_THAT(m.values(), ElementsAre("one", "a", "b", "three", "four"));
  EXPECT_TRUE(m.validate());
}

TEST(FlatMultimapModifiers, EraseByKeyRemovesAllEquivalentKeys) {
  chaistl::flat_multimap<int, std::string> m{{1, "one"}, {2, "a"}, {2, "b"}, {2, "c"}, {3, "three"}};

  EXPECT_EQ(m.erase(2), 3uz);
  EXPECT_EQ(m.erase(99), 0uz);

  EXPECT_THAT(m.keys(), ElementsAre(1, 3));
  EXPECT_THAT(m.values(), ElementsAre("one", "three"));
  EXPECT_TRUE(m.validate());
}

TEST(FlatMultimapModifiers, ExtractReplaceEraseIfAndCompare) {
  chaistl::flat_multimap<int, std::string> m{{1, "one"}, {2, "a"}, {2, "b"}};

  auto storage = std::move(m).extract();
  EXPECT_THAT(m, IsEmpty());
  EXPECT_THAT(storage.keys, ElementsAre(1, 2, 2));
  EXPECT_THAT(storage.values, ElementsAre("one", "a", "b"));

  storage.keys = {3, 3, 4};
  storage.values = {"a", "b", "four"};
  m.replace(std::move(storage));

  EXPECT_THAT(m.keys(), ElementsAre(3, 3, 4));
  EXPECT_EQ(erase_if(m,
                     [](const auto& value) {
                       return value.first == 3;
                     }),
            2uz);
  EXPECT_THAT(m.keys(), ElementsAre(4));
  EXPECT_LT((chaistl::flat_multimap<int, std::string>{{1, "a"}}), (chaistl::flat_multimap<int, std::string>{{2, "b"}}));
}

}  // namespace
