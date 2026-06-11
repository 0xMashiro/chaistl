// SPDX-License-Identifier: Apache-2.0

// References:
// - Node handles: https://eel.is/c++draft/container.node
// - Re-insertion must re-hash with the destination's hash function: the
//   cached hash code is meaningless across containers (ADR: Hash Table
//   Design; libstdc++ _M_reinsert_node does the same).

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_set.hpp>

#include <cstddef>
#include <functional>
#include <utility>

namespace {

using ::testing::UnorderedElementsAre;

// Stateful hasher: two instances with different offsets bucket the same key
// differently, which makes a stale cached hash observable via validate().
struct OffsetHash {
  std::size_t offset = 0;
  std::size_t operator()(int value) const { return std::hash<int>{}(value) + offset; }
};

TEST(UnorderedSetNodeHandle, ExtractByIteratorAndReinsert) {
  chaistl::unordered_set<int> set{1, 2, 3};
  const int* address = &*set.find(2);

  auto nh = set.extract(set.find(2));

  ASSERT_FALSE(nh.empty());
  EXPECT_TRUE(static_cast<bool>(nh));
  EXPECT_EQ(nh.value(), 2);
  EXPECT_EQ(set.size(), 2u);
  EXPECT_FALSE(set.contains(2));
  EXPECT_TRUE(set.validate());

  auto result = set.insert(std::move(nh));

  EXPECT_TRUE(result.inserted);
  EXPECT_TRUE(result.node.empty());
  EXPECT_EQ(*result.position, 2);
  EXPECT_EQ(address, &*result.position);  // the same node came back
  EXPECT_THAT(set, UnorderedElementsAre(1, 2, 3));
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetNodeHandle, ExtractMissReturnsEmptyHandle) {
  chaistl::unordered_set<int> set{1};

  auto nh = set.extract(99);

  EXPECT_TRUE(nh.empty());
  EXPECT_EQ(set.size(), 1u);

  // Inserting an empty handle is a no-op.
  auto result = set.insert(std::move(nh));
  EXPECT_FALSE(result.inserted);
  EXPECT_EQ(result.position, set.end());
  EXPECT_TRUE(result.node.empty());
}

TEST(UnorderedSetNodeHandle, InsertDuplicateKeepsNodeInHandle) {
  chaistl::unordered_set<int> set{1, 2};
  auto nh = set.extract(1);
  set.insert(1);  // a fresh element with the same key

  auto result = set.insert(std::move(nh));

  EXPECT_FALSE(result.inserted);
  ASSERT_FALSE(result.node.empty());  // ret.node keeps the rejected node
  EXPECT_EQ(result.node.value(), 1);
  EXPECT_EQ(*result.position, 1);
  EXPECT_EQ(set.size(), 2u);
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetNodeHandle, ReinsertionRehashesWithDestinationHasher) {
  using set_t = chaistl::unordered_set<int, OffsetHash>;
  set_t source(0, OffsetHash{0});
  set_t target(0, OffsetHash{12345});
  for (int i = 0; i < 20; ++i) {
    source.insert(i);
    target.insert(100 + i);
  }

  // The node's cached hash was computed with offset 0; the target hashes
  // differently. validate() recomputes hashes with the owning container's
  // hasher, so a stale cache would land in the wrong bucket and fail it.
  auto nh = source.extract(7);
  auto result = target.insert(std::move(nh));

  EXPECT_TRUE(result.inserted);
  EXPECT_TRUE(target.contains(7));
  EXPECT_TRUE(target.validate());
  EXPECT_TRUE(source.validate());
}

TEST(UnorderedSetNodeHandle, DroppedHandleDestroysNode) {
  chaistl::unordered_set<int> set{1, 2};
  {
    auto nh = set.extract(1);
    ASSERT_FALSE(nh.empty());
  }  // ASan verifies the node is freed here

  EXPECT_EQ(set.size(), 1u);
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetNodeHandle, HandleGetAllocatorMatchesContainer) {
  chaistl::unordered_set<int> set{1};
  auto nh = set.extract(1);
  EXPECT_EQ(nh.get_allocator(), set.get_allocator());
}

}  // namespace
