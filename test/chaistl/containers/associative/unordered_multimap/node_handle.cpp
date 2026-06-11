// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_map.hpp>
#include <chaistl/containers/unordered_multimap.hpp>

#include <cstddef>
#include <functional>
#include <string>

namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

struct OffsetHash {
  std::size_t offset = 0;
  std::size_t operator()(int value) const { return std::hash<int>{}(value) + offset; }
};

struct Equivalent {
  bool operator()(int lhs, int rhs) const { return lhs == rhs; }
};

TEST(UnorderedMultimapNodeHandle, ExtractMutateAndInsertAllowsDuplicates) {
  chaistl::unordered_multimap<int, std::string> map{{1, "a"}, {1, "b"}, {2, "c"}};

  auto nh = map.extract(2);
  ASSERT_FALSE(nh.empty());
  nh.key() = 1;
  nh.mapped() = "moved";
  auto it = map.insert(std::move(nh));

  EXPECT_TRUE(nh.empty());
  EXPECT_EQ(it->first, 1);
  EXPECT_EQ(it->second, "moved");
  EXPECT_EQ(map.count(1), 3u);
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMultimapNodeHandle, MergeAllCrossContainerCombinations) {
  chaistl::unordered_map<int, int> unique_target{{1, 10}};
  chaistl::unordered_multimap<int, int> multi_source{{1, 11}, {2, 20}, {2, 21}};
  unique_target.merge(multi_source);
  EXPECT_THAT(unique_target, UnorderedElementsAre(Pair(1, 10), Pair(2, 20)));
  EXPECT_THAT(multi_source, UnorderedElementsAre(Pair(1, 11), Pair(2, 21)));

  chaistl::unordered_multimap<int, int> multi_target{{1, 10}};
  chaistl::unordered_map<int, int> unique_source{{1, 11}, {3, 30}};
  multi_target.merge(unique_source);
  EXPECT_THAT(multi_target, UnorderedElementsAre(Pair(1, 10), Pair(1, 11), Pair(3, 30)));
  EXPECT_TRUE(unique_source.empty());

  chaistl::unordered_multimap<int, int> multi_target_2{{4, 40}};
  chaistl::unordered_multimap<int, int> multi_source_2{{4, 41}, {4, 42}, {5, 50}};
  multi_target_2.merge(multi_source_2);
  EXPECT_THAT(multi_target_2, UnorderedElementsAre(Pair(4, 40), Pair(4, 41), Pair(4, 42), Pair(5, 50)));
  EXPECT_TRUE(multi_source_2.empty());

  chaistl::unordered_map<int, int> unique_target_2{{6, 60}};
  chaistl::unordered_map<int, int> unique_source_2{{6, 61}, {7, 70}};
  unique_target_2.merge(unique_source_2);
  EXPECT_THAT(unique_target_2, UnorderedElementsAre(Pair(6, 60), Pair(7, 70)));
  EXPECT_THAT(unique_source_2, UnorderedElementsAre(Pair(6, 61)));
}

TEST(UnorderedMultimapNodeHandle, MergeAcceptsDifferentHashAndPredicateTypes) {
  chaistl::unordered_multimap<int, int, OffsetHash, Equivalent> target(0, OffsetHash{100}, Equivalent{});
  chaistl::unordered_map<int, int, std::hash<int>, std::equal_to<int>> source{{1, 10}, {2, 20}};

  target.merge(source);

  EXPECT_THAT(target, UnorderedElementsAre(Pair(1, 10), Pair(2, 20)));
  EXPECT_TRUE(source.empty());
  EXPECT_TRUE(target.validate());
}

}  // namespace
