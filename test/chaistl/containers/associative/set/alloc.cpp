// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/set.hpp>

#include <functional>

#include "../../sequence/support.hpp"

namespace {

using ::testing::ElementsAre;

TEST(SetAlloc, AssignmentPreservesNonPropagatingAllocator) {
  using Alloc = chaistl::test::TaggedAllocator<int>;
  using Set = chaistl::set<int, std::less<int>, Alloc>;

  Set copy_target({1}, Alloc{1});
  Set copy_source({2, 3}, Alloc{2});

  copy_target = copy_source;

  EXPECT_EQ(copy_target.get_allocator().id, 1);
  EXPECT_THAT(copy_target, ElementsAre(2, 3));
  EXPECT_TRUE(copy_target.validate());

  Set move_target({4}, Alloc{3});
  Set move_source({5, 6}, Alloc{4});

  move_target = std::move(move_source);

  EXPECT_EQ(move_target.get_allocator().id, 3);
  EXPECT_THAT(move_target, ElementsAre(5, 6));
  EXPECT_TRUE(move_target.validate());
}

TEST(SetAlloc, AssignmentPropagatesPropagatingAllocator) {
  using Alloc = chaistl::test::PropagatingAllocator<int>;
  using Set = chaistl::set<int, std::less<int>, Alloc>;

  Set copy_target({1}, Alloc{1});
  Set copy_source({2}, Alloc{2});

  copy_target = copy_source;

  EXPECT_EQ(copy_target.get_allocator().id, 2);
  EXPECT_THAT(copy_target, ElementsAre(2));

  Set move_target({3}, Alloc{3});
  Set move_source({4}, Alloc{4});

  move_target = std::move(move_source);

  EXPECT_EQ(move_target.get_allocator().id, 4);
  EXPECT_THAT(move_target, ElementsAre(4));
  EXPECT_TRUE(move_source.empty());
}

TEST(SetAlloc, SwapHonorsAllocatorPropagationTraits) {
  {
    using Alloc = chaistl::test::CompatibleTaggedAllocator<int>;
    using Set = chaistl::set<int, std::less<int>, Alloc>;
    Set lhs({1}, Alloc{1});
    Set rhs({2}, Alloc{2});

    lhs.swap(rhs);

    EXPECT_EQ(lhs.get_allocator().id, 1);
    EXPECT_EQ(rhs.get_allocator().id, 2);
    EXPECT_THAT(lhs, ElementsAre(2));
    EXPECT_THAT(rhs, ElementsAre(1));
  }

  {
    using Alloc = chaistl::test::PropagatingAllocator<int>;
    using Set = chaistl::set<int, std::less<int>, Alloc>;
    Set lhs({1}, Alloc{1});
    Set rhs({2}, Alloc{2});

    lhs.swap(rhs);

    EXPECT_EQ(lhs.get_allocator().id, 2);
    EXPECT_EQ(rhs.get_allocator().id, 1);
    EXPECT_THAT(lhs, ElementsAre(2));
    EXPECT_THAT(rhs, ElementsAre(1));
  }
}

}  // namespace
