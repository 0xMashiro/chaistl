// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_set.hpp>
#include <chaistl/functional/hash.hpp>

#include <functional>

#include "../../sequence/support.hpp"

namespace {

using ::testing::UnorderedElementsAre;

TEST(UnorderedSetAlloc, AssignmentPreservesNonPropagatingAllocator) {
  using Alloc = chaistl::test::TaggedAllocator<int>;
  using Set = chaistl::unordered_set<int, chaistl::hash<int>, std::equal_to<int>, Alloc>;

  Set copy_target(Alloc{1});
  copy_target.insert(1);
  Set copy_source({2, 3}, Alloc{2});

  copy_target = copy_source;

  EXPECT_EQ(copy_target.get_allocator().id, 1);
  EXPECT_THAT(copy_target, UnorderedElementsAre(2, 3));
  EXPECT_TRUE(copy_target.validate());

  Set move_target(Alloc{3});
  move_target.insert(4);
  Set move_source({5, 6}, Alloc{4});

  move_target = std::move(move_source);

  EXPECT_EQ(move_target.get_allocator().id, 3);
  EXPECT_THAT(move_target, UnorderedElementsAre(5, 6));
  EXPECT_TRUE(move_target.validate());
}

TEST(UnorderedSetAlloc, AssignmentPropagatesPropagatingAllocator) {
  using Alloc = chaistl::test::PropagatingAllocator<int>;
  using Set = chaistl::unordered_set<int, chaistl::hash<int>, std::equal_to<int>, Alloc>;

  Set copy_target(Alloc{1});
  copy_target.insert(1);
  Set copy_source(Alloc{2});
  copy_source.insert(2);

  copy_target = copy_source;

  EXPECT_EQ(copy_target.get_allocator().id, 2);
  EXPECT_THAT(copy_target, UnorderedElementsAre(2));

  Set move_target(Alloc{3});
  move_target.insert(3);
  Set move_source(Alloc{4});
  move_source.insert(4);

  move_target = std::move(move_source);

  EXPECT_EQ(move_target.get_allocator().id, 4);
  EXPECT_THAT(move_target, UnorderedElementsAre(4));
  EXPECT_TRUE(move_source.empty());
}

TEST(UnorderedSetAlloc, SwapHonorsAllocatorPropagationTraits) {
  {
    using Alloc = chaistl::test::CompatibleTaggedAllocator<int>;
    using Set = chaistl::unordered_set<int, chaistl::hash<int>, std::equal_to<int>, Alloc>;
    Set lhs(Alloc{1});
    lhs.insert(1);
    Set rhs(Alloc{2});
    rhs.insert(2);

    lhs.swap(rhs);

    EXPECT_EQ(lhs.get_allocator().id, 1);
    EXPECT_EQ(rhs.get_allocator().id, 2);
    EXPECT_THAT(lhs, UnorderedElementsAre(2));
    EXPECT_THAT(rhs, UnorderedElementsAre(1));
  }

  {
    using Alloc = chaistl::test::PropagatingAllocator<int>;
    using Set = chaistl::unordered_set<int, chaistl::hash<int>, std::equal_to<int>, Alloc>;
    Set lhs(Alloc{1});
    lhs.insert(1);
    Set rhs(Alloc{2});
    rhs.insert(2);

    lhs.swap(rhs);

    EXPECT_EQ(lhs.get_allocator().id, 2);
    EXPECT_EQ(rhs.get_allocator().id, 1);
    EXPECT_THAT(lhs, UnorderedElementsAre(2));
    EXPECT_THAT(rhs, UnorderedElementsAre(1));
  }
}

}  // namespace
