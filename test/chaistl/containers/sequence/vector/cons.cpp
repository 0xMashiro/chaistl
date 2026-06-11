// SPDX-License-Identifier: Apache-2.0

// References:
// - [vector.cons], construct/copy/destroy/assign/get_allocator:
//   https://eel.is/c++draft/vector.cons
//   https://en.cppreference.com/w/cpp/container/vector/vector
//   https://en.cppreference.com/w/cpp/container/vector/operator%3D
//   https://en.cppreference.com/w/cpp/container/vector/assign
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/containers/vector.hpp>

#include <algorithm>
#include <array>
#include <ranges>
#include <utility>

#include "../support.hpp"

TEST(VectorTest, AssignRangeUsesCxx23RangeInterface) {
  chaistl::vector<int> values{1, 2, 3};

  values.assign_range(std::views::iota(1, 4));

  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
}

TEST(VectorTest, SelfMoveAssignmentIsStable) {
  chaistl::vector<int> values{1, 2, 3};

  // Route through a reference so the deliberate self-move does not trigger
  // -Wself-move.
  chaistl::vector<int>& self = values;
  values = std::move(self);

  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
}

TEST(VectorTest, AssignmentPreservesNonPropagatingAllocator) {
  using Vector = chaistl::vector<int, chaistl::test::TaggedAllocator<int>>;

  Vector copy_target({1}, chaistl::test::TaggedAllocator<int>{1});
  Vector copy_source({2, 3}, chaistl::test::TaggedAllocator<int>{2});

  copy_target = copy_source;

  EXPECT_EQ(copy_target.get_allocator().id, 1);
  EXPECT_TRUE(std::ranges::equal(copy_target, std::array{2, 3}));
  EXPECT_TRUE(std::ranges::equal(copy_source, std::array{2, 3}));

  Vector move_target({4}, chaistl::test::TaggedAllocator<int>{3});
  Vector move_source({5, 6, 7}, chaistl::test::TaggedAllocator<int>{4});

  move_target = std::move(move_source);

  EXPECT_EQ(move_target.get_allocator().id, 3);
  EXPECT_TRUE(std::ranges::equal(move_target, std::array{5, 6, 7}));
}

TEST(VectorTest, EqualNonPropagatingAllocatorMoveStealsStorage) {
  using Allocator = chaistl::test::TaggedAllocator<int>;
  using Vector = chaistl::vector<int, Allocator>;

  Vector move_target({1}, Allocator{7});
  Vector move_source({2, 3, 4}, Allocator{7});
  const auto* source_data = move_source.data();

  Vector constructed(std::move(move_source), Allocator{7});

  EXPECT_EQ(constructed.get_allocator().id, 7);
  EXPECT_EQ(constructed.data(), source_data);
  EXPECT_TRUE(std::ranges::equal(constructed, std::array{2, 3, 4}));
  EXPECT_TRUE(move_source.empty());

  Vector assigned_source({5, 6}, Allocator{7});
  const auto* assigned_source_data = assigned_source.data();

  move_target = std::move(assigned_source);

  EXPECT_EQ(move_target.get_allocator().id, 7);
  EXPECT_EQ(move_target.data(), assigned_source_data);
  EXPECT_TRUE(std::ranges::equal(move_target, std::array{5, 6}));
  EXPECT_TRUE(assigned_source.empty());
}

TEST(VectorTest, PropagatingAllocatorAssignmentPropagatesAllocator) {
  using Allocator = chaistl::test::PropagatingAllocator<int>;
  using Vector = chaistl::vector<int, Allocator>;

  Vector copy_target({1}, Allocator{1});
  Vector copy_source({2, 3}, Allocator{2});

  copy_target = copy_source;

  EXPECT_EQ(copy_target.get_allocator().id, 2);
  EXPECT_TRUE(std::ranges::equal(copy_target, std::array{2, 3}));

  Vector move_target({4}, Allocator{3});
  Vector move_source({5, 6}, Allocator{4});

  move_target = std::move(move_source);

  EXPECT_EQ(move_target.get_allocator().id, 4);
  EXPECT_TRUE(std::ranges::equal(move_target, std::array{5, 6}));
  EXPECT_TRUE(move_source.empty());
}

TEST(VectorTest, DefaultConstructionReleasesStorageWhenElementConstructionThrows) {
  chaistl::test::ThrowOnDefault::reset();
  {
    chaistl::test::ThrowOnDefault::defaults_before_throw = 1;

    EXPECT_THROW((chaistl::vector<chaistl::test::ThrowOnDefault>(3)), std::runtime_error);

    EXPECT_EQ(chaistl::test::ThrowOnDefault::alive, 0);
  }
  EXPECT_EQ(chaistl::test::ThrowOnDefault::alive, 0);
}

TEST(VectorTest, IteratorConstructionReleasesStorageWhenCopyThrows) {
  chaistl::test::ThrowOnCopy::reset();
  {
    std::array source{chaistl::test::ThrowOnCopy(1), chaistl::test::ThrowOnCopy(2), chaistl::test::ThrowOnCopy(3)};

    chaistl::test::ThrowOnCopy::copies_before_throw = 1;
    EXPECT_THROW((chaistl::vector<chaistl::test::ThrowOnCopy>(source.begin(), source.end())), std::runtime_error);

    EXPECT_EQ(chaistl::test::ThrowOnCopy::alive, 3);
  }
  EXPECT_EQ(chaistl::test::ThrowOnCopy::alive, 0);
}
