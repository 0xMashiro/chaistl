// SPDX-License-Identifier: Apache-2.0

// References:
// - std::deque constructors, assignment, and allocator access:
//   https://en.cppreference.com/w/cpp/container/deque/deque
//   https://en.cppreference.com/w/cpp/container/deque/operator%3D
//   https://en.cppreference.com/w/cpp/container/deque/assign
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/containers/deque.hpp>

#include <array>
#include <ranges>
#include <stdexcept>
#include <utility>

#include "../support.hpp"

TEST(DequeTest, AssignRangeUsesCxx23RangeInterface) {
  chaistl::deque<int> values{9, 8, 7};

  values.assign_range(std::views::iota(1, 4));

  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
}

TEST(DequeTest, SelfMoveAssignmentIsStable) {
  chaistl::deque<int> values{1, 2, 3};

  // Route through a reference so the deliberate self-move does not trigger
  // -Wself-move.
  chaistl::deque<int>& self = values;
  values = std::move(self);

  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
}

TEST(DequeTest, AssignmentPreservesNonPropagatingAllocator) {
  using Deque = chaistl::deque<int, chaistl::test::TaggedAllocator<int>>;

  Deque copy_target({1}, chaistl::test::TaggedAllocator<int>{1});
  Deque copy_source({2, 3}, chaistl::test::TaggedAllocator<int>{2});

  copy_target = copy_source;

  EXPECT_EQ(copy_target.get_allocator().id, 1);
  EXPECT_TRUE(std::ranges::equal(copy_target, std::array{2, 3}));
  EXPECT_TRUE(std::ranges::equal(copy_source, std::array{2, 3}));

  Deque move_target({4}, chaistl::test::TaggedAllocator<int>{3});
  Deque move_source({5, 6, 7}, chaistl::test::TaggedAllocator<int>{4});

  move_target = std::move(move_source);

  EXPECT_EQ(move_target.get_allocator().id, 3);
  EXPECT_TRUE(std::ranges::equal(move_target, std::array{5, 6, 7}));
}

TEST(DequeTest, EqualNonPropagatingAllocatorMoveStealsStorageSemantics) {
  using Allocator = chaistl::test::TaggedAllocator<int>;
  using Deque = chaistl::deque<int, Allocator>;

  Deque move_source({2, 3, 4}, Allocator{7});
  Deque constructed(std::move(move_source), Allocator{7});

  EXPECT_EQ(constructed.get_allocator().id, 7);
  EXPECT_TRUE(std::ranges::equal(constructed, std::array{2, 3, 4}));
  EXPECT_TRUE(move_source.empty());

  Deque move_target({1}, Allocator{7});
  Deque assigned_source({5, 6}, Allocator{7});

  move_target = std::move(assigned_source);

  EXPECT_EQ(move_target.get_allocator().id, 7);
  EXPECT_TRUE(std::ranges::equal(move_target, std::array{5, 6}));
  EXPECT_TRUE(assigned_source.empty());
}

TEST(DequeTest, PropagatingAllocatorAssignmentPropagatesAllocator) {
  using Allocator = chaistl::test::PropagatingAllocator<int>;
  using Deque = chaistl::deque<int, Allocator>;

  Deque copy_target({1}, Allocator{1});
  Deque copy_source({2, 3}, Allocator{2});

  copy_target = copy_source;

  EXPECT_EQ(copy_target.get_allocator().id, 2);
  EXPECT_TRUE(std::ranges::equal(copy_target, std::array{2, 3}));

  Deque move_target({4}, Allocator{3});
  Deque move_source({5, 6}, Allocator{4});

  move_target = std::move(move_source);

  EXPECT_EQ(move_target.get_allocator().id, 4);
  EXPECT_TRUE(std::ranges::equal(move_target, std::array{5, 6}));
  EXPECT_TRUE(move_source.empty());
}

TEST(DequeTest, DefaultConstructionDestroysAndDeallocatesStorageWhenElementConstructionThrows) {
  chaistl::test::ThrowOnDefault::reset();
  {
    chaistl::test::ThrowOnDefault::defaults_before_throw = 1;

    EXPECT_THROW((chaistl::deque<chaistl::test::ThrowOnDefault>(3)), std::runtime_error);

    EXPECT_EQ(chaistl::test::ThrowOnDefault::alive, 0);
  }
  EXPECT_EQ(chaistl::test::ThrowOnDefault::alive, 0);
}

TEST(DequeTest, IteratorConstructionDestroysAndDeallocatesStorageWhenCopyThrows) {
  chaistl::test::ThrowOnCopy::reset();
  {
    std::array source{chaistl::test::ThrowOnCopy(1), chaistl::test::ThrowOnCopy(2), chaistl::test::ThrowOnCopy(3)};

    chaistl::test::ThrowOnCopy::copies_before_throw = 1;
    EXPECT_THROW((chaistl::deque<chaistl::test::ThrowOnCopy>(source.begin(), source.end())), std::runtime_error);

    EXPECT_EQ(chaistl::test::ThrowOnCopy::alive, 3);
  }
  EXPECT_EQ(chaistl::test::ThrowOnCopy::alive, 0);
}
