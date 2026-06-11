// SPDX-License-Identifier: Apache-2.0

// References:
// - std::list allocator-aware operations:
//   https://en.cppreference.com/w/cpp/container/list
//   https://eel.is/c++draft/list.cons

#include <gtest/gtest.h>

#include <chaistl/containers/list.hpp>

#include "../support.hpp"

namespace {

using chaistl::list;
using chaistl::test::CompatibleTaggedAllocator;
using chaistl::test::PropagatingAllocator;
using chaistl::test::TaggedAllocator;

// ---------------------------------------------------------------------------
// Explicit allocator constructors
// ---------------------------------------------------------------------------

TEST(ListAllocators, ExplicitAllocatorConstruct) {
  TaggedAllocator<int> alloc(42);
  list<int, TaggedAllocator<int>> l(alloc);
  EXPECT_TRUE(l.empty());
}

TEST(ListAllocators, CountWithAllocator) {
  TaggedAllocator<int> alloc(42);
  list<int, TaggedAllocator<int>> l(3, alloc);
  EXPECT_EQ(l.size(), 3);
  for (int value : l) {
    EXPECT_EQ(value, 0);
  }
}

TEST(ListAllocators, CountValueWithAllocator) {
  TaggedAllocator<int> alloc(42);
  list<int, TaggedAllocator<int>> l(3, 7, alloc);
  EXPECT_EQ(l.size(), 3);
  for (int value : l) {
    EXPECT_EQ(value, 7);
  }
}

TEST(ListAllocators, IteratorRangeWithAllocator) {
  TaggedAllocator<int> alloc(42);
  int arr[] = {1, 2, 3};
  list<int, TaggedAllocator<int>> l(arr, arr + 3, alloc);
  EXPECT_EQ(l.size(), 3);
  EXPECT_EQ(l.front(), 1);
  EXPECT_EQ(l.back(), 3);
}

TEST(ListAllocators, InitializerListWithAllocator) {
  TaggedAllocator<int> alloc(42);
  list<int, TaggedAllocator<int>> l({1, 2, 3}, alloc);
  EXPECT_EQ(l.size(), 3);
  EXPECT_EQ(l.front(), 1);
  EXPECT_EQ(l.back(), 3);
}

TEST(ListAllocators, FromRangeWithAllocator) {
  TaggedAllocator<int> alloc(42);
  std::vector<int> v = {1, 2, 3};
  list<int, TaggedAllocator<int>> l(std::from_range, v, alloc);
  EXPECT_EQ(l.size(), 3);
  EXPECT_EQ(l.front(), 1);
  EXPECT_EQ(l.back(), 3);
}

// ---------------------------------------------------------------------------
// Copy construct with allocator
// ---------------------------------------------------------------------------

TEST(ListAllocators, CopyConstructWithSameAllocator) {
  TaggedAllocator<int> alloc(42);
  list<int, TaggedAllocator<int>> l1({1, 2, 3}, alloc);
  list<int, TaggedAllocator<int>> l2(l1, alloc);
  EXPECT_EQ(l2.size(), 3);
  EXPECT_EQ(l2.front(), 1);
}

TEST(ListAllocators, CopyConstructWithDifferentAllocator) {
  TaggedAllocator<int> alloc1(1);
  TaggedAllocator<int> alloc2(2);
  list<int, TaggedAllocator<int>> l1({1, 2, 3}, alloc1);
  list<int, TaggedAllocator<int>> l2(l1, alloc2);
  EXPECT_EQ(l2.size(), 3);
  EXPECT_EQ(l2.front(), 1);
}

// ---------------------------------------------------------------------------
// Move construct with allocator (storage_compatible path)
// ---------------------------------------------------------------------------

TEST(ListAllocators, MoveConstructWithSameAllocator) {
  TaggedAllocator<int> alloc(42);
  list<int, TaggedAllocator<int>> l1({1, 2, 3}, alloc);
  list<int, TaggedAllocator<int>> l2(std::move(l1), alloc);
  EXPECT_EQ(l2.size(), 3);
  EXPECT_TRUE(l1.empty());
}

TEST(ListAllocators, MoveConstructWithCompatibleAllocator) {
  // CompatibleTaggedAllocator::operator== always returns true,
  // so storage_compatible_with returns true and take_storage_from is called.
  CompatibleTaggedAllocator<int> alloc1(1);
  CompatibleTaggedAllocator<int> alloc2(2);
  list<int, CompatibleTaggedAllocator<int>> l1({1, 2, 3}, alloc1);
  list<int, CompatibleTaggedAllocator<int>> l2(std::move(l1), alloc2);
  EXPECT_EQ(l2.size(), 3);
  EXPECT_TRUE(l1.empty());
}

TEST(ListAllocators, MoveConstructWithIncompatibleAllocator) {
  // TaggedAllocator::operator== compares id, so alloc1 != alloc2.
  // storage_compatible_with returns false, so elements are moved individually.
  TaggedAllocator<int> alloc1(1);
  TaggedAllocator<int> alloc2(2);
  list<int, TaggedAllocator<int>> l1({1, 2, 3}, alloc1);
  list<int, TaggedAllocator<int>> l2(std::move(l1), alloc2);
  EXPECT_EQ(l2.size(), 3);
  EXPECT_EQ(l2.front(), 1);
}

// ---------------------------------------------------------------------------
// Copy assignment with POCCA
// ---------------------------------------------------------------------------

TEST(ListAllocators, CopyAssignPropagating) {
  PropagatingAllocator<int> alloc1(1);
  PropagatingAllocator<int> alloc2(2);
  list<int, PropagatingAllocator<int>> l1({1, 2, 3}, alloc1);
  list<int, PropagatingAllocator<int>> l2({4, 5}, alloc2);

  l2 = l1;

  EXPECT_EQ(l2.size(), 3);
  EXPECT_EQ(l2.front(), 1);
  EXPECT_EQ(l2.back(), 3);
}

// ---------------------------------------------------------------------------
// Move assignment with POCMA
// ---------------------------------------------------------------------------

TEST(ListAllocators, MoveAssignPropagating) {
  PropagatingAllocator<int> alloc1(1);
  PropagatingAllocator<int> alloc2(2);
  list<int, PropagatingAllocator<int>> l1({1, 2, 3}, alloc1);
  list<int, PropagatingAllocator<int>> l2({4, 5}, alloc2);

  l2 = std::move(l1);

  EXPECT_EQ(l2.size(), 3);
  EXPECT_TRUE(l1.empty());
}

// ---------------------------------------------------------------------------
// Move assignment with always_equal allocator
// ---------------------------------------------------------------------------

TEST(ListAllocators, MoveAssignAlwaysEqual) {
  // std::allocator is always_equal, so replace_storage_from path is taken.
  list<int> l1 = {1, 2, 3};
  list<int> l2 = {4, 5};

  l2 = std::move(l1);

  EXPECT_EQ(l2.size(), 3);
  EXPECT_TRUE(l1.empty());
}

// ---------------------------------------------------------------------------
// Move assignment with incompatible allocator (element-wise move)
// ---------------------------------------------------------------------------

TEST(ListAllocators, MoveAssignIncompatible) {
  TaggedAllocator<int> alloc1(1);
  TaggedAllocator<int> alloc2(2);
  list<int, TaggedAllocator<int>> l1({1, 2, 3}, alloc1);
  list<int, TaggedAllocator<int>> l2({4, 5}, alloc2);

  l2 = std::move(l1);

  EXPECT_EQ(l2.size(), 3);
  EXPECT_EQ(l2.front(), 1);
}

// ---------------------------------------------------------------------------
// Self-assignment
// ---------------------------------------------------------------------------

TEST(ListAllocators, CopyAssignSelf) {
  list<int> l = {1, 2, 3};
  // Route through a reference so the deliberate self-assignment does not
  // trigger -Wself-assign-overloaded.
  list<int>& self = l;
  l = self;
  EXPECT_EQ(l.size(), 3);
  EXPECT_EQ(l.front(), 1);
}

TEST(ListAllocators, MoveAssignSelf) {
  list<int> l = {1, 2, 3};
  list<int>& self = l;
  l = std::move(self);
  EXPECT_EQ(l.size(), 3);
  EXPECT_EQ(l.front(), 1);
}

// ---------------------------------------------------------------------------
// Swap with allocator propagation
// ---------------------------------------------------------------------------

TEST(ListAllocators, SwapPropagating) {
  PropagatingAllocator<int> alloc1(1);
  PropagatingAllocator<int> alloc2(2);
  list<int, PropagatingAllocator<int>> l1({1, 2, 3}, alloc1);
  list<int, PropagatingAllocator<int>> l2({4, 5}, alloc2);

  l1.swap(l2);

  EXPECT_EQ(l1.size(), 2);
  EXPECT_EQ(l2.size(), 3);
}

}  // namespace
