// SPDX-License-Identifier: Apache-2.0

// References:
// - Node handles for maps: nh.key() is a MUTABLE reference even though the
//   stored value is pair<const Key, T> — updating a key without reallocation
//   is the point of extract() ([container.node.overview]). Re-insertion
//   hashes the (possibly new) key with the destination's hash function.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_map.hpp>

#include <string>
#include <utility>

namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(UnorderedMapNodeHandle, KeyMutationThroughHandle) {
  chaistl::unordered_map<int, std::string> map{{1, "payload"}, {5, "other"}};
  const std::string* mapped_address = &map.at(1);

  auto nh = map.extract(1);
  ASSERT_FALSE(nh.empty());
  nh.key() = 2;               // rename the key while outside the container
  nh.mapped() += "-updated";  // the mapped value is reachable too

  auto result = map.insert(std::move(nh));

  EXPECT_TRUE(result.inserted);
  EXPECT_FALSE(map.contains(1));
  EXPECT_EQ(map.at(2), "payload-updated");
  // Same node, same mapped object: no reallocation happened.
  EXPECT_EQ(mapped_address, &map.at(2));
  EXPECT_TRUE(map.validate());  // the new key hashes into the right bucket
}

TEST(UnorderedMapNodeHandle, InsertDuplicateKeepsNode) {
  chaistl::unordered_map<int, int> map{{1, 10}, {2, 20}};
  auto nh = map.extract(1);
  map.try_emplace(1, 99);

  auto result = map.insert(std::move(nh));

  EXPECT_FALSE(result.inserted);
  ASSERT_FALSE(result.node.empty());
  EXPECT_EQ(result.node.key(), 1);
  EXPECT_EQ(result.node.mapped(), 10);
  EXPECT_EQ(map.at(1), 99);  // the fresh element won
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMapNodeHandle, MergeKeepsDuplicateMappedValuesInSource) {
  chaistl::unordered_map<int, int> target{{1, 10}, {2, 20}};
  chaistl::unordered_map<int, int> source{{2, 222}, {3, 30}};

  target.merge(source);

  EXPECT_THAT(target, UnorderedElementsAre(Pair(1, 10), Pair(2, 20), Pair(3, 30)));
  EXPECT_THAT(source, UnorderedElementsAre(Pair(2, 222)));  // loser stays put
  EXPECT_TRUE(target.validate());
  EXPECT_TRUE(source.validate());
}

}  // namespace
