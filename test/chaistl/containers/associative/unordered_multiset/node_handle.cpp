// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_multiset.hpp>
#include <chaistl/containers/unordered_set.hpp>

#include <cstddef>
#include <functional>

namespace {

using ::testing::UnorderedElementsAre;

struct OffsetHash {
  std::size_t offset = 0;
  std::size_t operator()(int value) const { return std::hash<int>{}(value) + offset; }
};

struct Equivalent {
  bool operator()(int lhs, int rhs) const { return lhs == rhs; }
};

TEST(UnorderedMultisetNodeHandle, ExtractAndInsertAllowsDuplicates) {
  chaistl::unordered_multiset<int> set{1, 1, 2};
  const int* address = &*set.find(1);

  auto nh = set.extract(1);
  ASSERT_FALSE(nh.empty());
  set.insert(1);
  auto it = set.insert(std::move(nh));

  EXPECT_TRUE(nh.empty());
  EXPECT_EQ(*it, 1);
  EXPECT_EQ(address, &*it);
  EXPECT_EQ(set.count(1), 3u);
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedMultisetNodeHandle, MergeAllCrossContainerCombinations) {
  chaistl::unordered_set<int> unique_target{1};
  chaistl::unordered_multiset<int> multi_source{1, 2, 2};
  unique_target.merge(multi_source);
  EXPECT_THAT(unique_target, UnorderedElementsAre(1, 2));
  EXPECT_THAT(multi_source, UnorderedElementsAre(1, 2));

  chaistl::unordered_multiset<int> multi_target{1};
  chaistl::unordered_set<int> unique_source{1, 3};
  multi_target.merge(unique_source);
  EXPECT_THAT(multi_target, UnorderedElementsAre(1, 1, 3));
  EXPECT_TRUE(unique_source.empty());

  chaistl::unordered_multiset<int> multi_target_2{4};
  chaistl::unordered_multiset<int> multi_source_2{4, 4, 5};
  multi_target_2.merge(multi_source_2);
  EXPECT_THAT(multi_target_2, UnorderedElementsAre(4, 4, 4, 5));
  EXPECT_TRUE(multi_source_2.empty());

  chaistl::unordered_set<int> unique_target_2{6};
  chaistl::unordered_set<int> unique_source_2{6, 7};
  unique_target_2.merge(unique_source_2);
  EXPECT_THAT(unique_target_2, UnorderedElementsAre(6, 7));
  EXPECT_THAT(unique_source_2, UnorderedElementsAre(6));
}

TEST(UnorderedMultisetNodeHandle, MergeAcceptsDifferentHashAndPredicateTypes) {
  chaistl::unordered_multiset<int, OffsetHash, Equivalent> target(0, OffsetHash{100}, Equivalent{});
  chaistl::unordered_set<int, std::hash<int>, std::equal_to<int>> source{1, 2};

  target.merge(source);

  EXPECT_THAT(target, UnorderedElementsAre(1, 2));
  EXPECT_TRUE(source.empty());
  EXPECT_TRUE(target.validate());
}

}  // namespace
