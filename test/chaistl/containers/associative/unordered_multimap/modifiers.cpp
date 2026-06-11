// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_multimap.hpp>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

std::vector<std::pair<int, std::string>> values(const chaistl::unordered_multimap<int, std::string>& map) {
  return {map.begin(), map.end()};
}

TEST(UnorderedMultimapModifiers, InsertDuplicatesKeepEquivalentKeysContiguous) {
  chaistl::unordered_multimap<int, std::string> map;

  map.insert({1, "a"});
  map.insert({2, "b"});
  auto inserted = map.emplace(1, "c");
  map.insert({3, "d"});
  map.insert({1, "e"});

  EXPECT_EQ(inserted->first, 1);
  EXPECT_EQ(map.count(1), 3u);
  EXPECT_THAT(values(map), ElementsAre(Pair(1, "a"), Pair(1, "c"), Pair(1, "e"), Pair(2, "b"), Pair(3, "d")));
  auto [first, last] = map.equal_range(1);
  EXPECT_EQ(std::distance(first, last), 3);
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMultimapModifiers, RehashPreservesEquivalentKeyAdjacency) {
  chaistl::unordered_multimap<int, std::string> map;
  map.insert({1, "a"});
  map.insert({2, "b"});
  map.insert({1, "c"});
  map.insert({3, "d"});
  map.insert({1, "e"});
  map.insert({2, "f"});

  map.rehash(97);

  EXPECT_THAT(values(map),
              ElementsAre(Pair(1, "a"), Pair(1, "c"), Pair(1, "e"), Pair(2, "b"), Pair(2, "f"), Pair(3, "d")));
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMultimapModifiers, EraseByKeyRemovesWholeSegment) {
  chaistl::unordered_multimap<int, std::string> map{{1, "a"}, {2, "b"}, {1, "c"}, {3, "d"}};

  EXPECT_EQ(map.erase(1), 2u);
  EXPECT_EQ(map.erase(1), 0u);
  EXPECT_THAT(map, UnorderedElementsAre(Pair(2, "b"), Pair(3, "d")));
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMultimapModifiers, InsertRangeMatchesStdMultiplicity) {
  const std::vector<std::pair<int, int>> input{{1, 10}, {2, 20}, {1, 11}, {1, 12}};
  chaistl::unordered_multimap<int, int> actual;
  std::unordered_multimap<int, int> expected;

  actual.insert_range(input);
  expected.insert(input.begin(), input.end());

  for (int key : {1, 2, 3}) {
    EXPECT_EQ(actual.count(key), expected.count(key));
  }
  EXPECT_TRUE(actual.validate());
}

}  // namespace
