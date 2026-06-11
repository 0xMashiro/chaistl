// SPDX-License-Identifier: Apache-2.0

// References:
// - merge ([unord.req.general]): splices nodes whose keys are absent in the
//   target; duplicates stay in the source; pointers and references to the
//   transferred elements remain valid. Only the hash/predicate types may
//   differ — the allocator type is fixed.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_set.hpp>

#include <cstddef>
#include <functional>
#include <utility>

namespace {

using ::testing::UnorderedElementsAre;

TEST(UnorderedSetInterop, MergeMovesAbsentKeysAndKeepsDuplicates) {
  chaistl::unordered_set<int> target{1, 2};
  chaistl::unordered_set<int> source{2, 3, 4};

  target.merge(source);

  EXPECT_THAT(target, UnorderedElementsAre(1, 2, 3, 4));
  EXPECT_THAT(source, UnorderedElementsAre(2));  // the duplicate stays
  EXPECT_TRUE(target.validate());
  EXPECT_TRUE(source.validate());
}

TEST(UnorderedSetInterop, MergePreservesElementAddresses) {
  chaistl::unordered_set<int> target{1};
  chaistl::unordered_set<int> source{2, 3};
  const int* address = &*source.find(3);

  target.merge(source);

  EXPECT_EQ(address, &*target.find(3));  // spliced, not copied
}

TEST(UnorderedSetInterop, MergeAcrossDifferentHasherTypes) {
  struct ShiftHash {
    std::size_t operator()(int value) const { return std::hash<int>{}(value) >> 1; }
  };
  chaistl::unordered_set<int> target{1, 2};
  chaistl::unordered_set<int, ShiftHash> source{2, 3};

  target.merge(source);  // keys are re-hashed with target's hasher

  EXPECT_THAT(target, UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(source, UnorderedElementsAre(2));
  EXPECT_TRUE(target.validate());
  EXPECT_TRUE(source.validate());
}

TEST(UnorderedSetInterop, MergeFromRvalue) {
  chaistl::unordered_set<int> target{1};

  target.merge(chaistl::unordered_set<int>{2, 3});

  EXPECT_THAT(target, UnorderedElementsAre(1, 2, 3));
  EXPECT_TRUE(target.validate());
}

TEST(UnorderedSetInterop, MergeIntoEmptyAndFromEmpty) {
  chaistl::unordered_set<int> target;
  chaistl::unordered_set<int> source{1, 2};

  target.merge(source);
  EXPECT_THAT(target, UnorderedElementsAre(1, 2));
  EXPECT_TRUE(source.empty());

  chaistl::unordered_set<int> empty;
  target.merge(empty);
  EXPECT_THAT(target, UnorderedElementsAre(1, 2));
  EXPECT_TRUE(target.validate());
}

}  // namespace
