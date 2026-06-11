// SPDX-License-Identifier: Apache-2.0

// References:
// - std::list assignment operations:
//   https://en.cppreference.com/w/cpp/container/list/operator%3D
//   https://en.cppreference.com/w/cpp/container/list/assign

#include <gtest/gtest.h>

#include <chaistl/containers/list.hpp>

#include <vector>

namespace {

using chaistl::list;

// ---------------------------------------------------------------------------
// operator=(initializer_list)
// ---------------------------------------------------------------------------

TEST(ListAssign, InitializerList) {
  list<int> l = {1, 2, 3};
  l = {4, 5, 6, 7};
  EXPECT_EQ(l.size(), 4);
  EXPECT_EQ(l.front(), 4);
  EXPECT_EQ(l.back(), 7);
}

// ---------------------------------------------------------------------------
// assign(InputIt, InputIt)
// ---------------------------------------------------------------------------

TEST(ListAssign, IteratorRange) {
  list<int> l = {1, 2, 3};
  int arr[] = {7, 8, 9, 10};
  l.assign(arr, arr + 4);
  EXPECT_EQ(l.size(), 4);
  EXPECT_EQ(l.front(), 7);
  EXPECT_EQ(l.back(), 10);
}

// ---------------------------------------------------------------------------
// assign(count, value)
// ---------------------------------------------------------------------------

TEST(ListAssign, CountValue) {
  list<int> l = {1, 2, 3};
  l.assign(5, 42);
  EXPECT_EQ(l.size(), 5);
  for (int value : l) {
    EXPECT_EQ(value, 42);
  }
}

// ---------------------------------------------------------------------------
// assign(initializer_list)
// ---------------------------------------------------------------------------

TEST(ListAssign, AssignInitializerList) {
  list<int> l = {1, 2, 3};
  l.assign({4, 5, 6, 7});
  EXPECT_EQ(l.size(), 4);
  EXPECT_EQ(l.front(), 4);
  EXPECT_EQ(l.back(), 7);
}

// ---------------------------------------------------------------------------
// assign_range
// ---------------------------------------------------------------------------

TEST(ListAssign, AssignRange) {
  list<int> l = {1, 2, 3};
  std::vector<int> v = {7, 8, 9, 10};
  l.assign_range(v);
  EXPECT_EQ(l.size(), 4);
  EXPECT_EQ(l.front(), 7);
  EXPECT_EQ(l.back(), 10);
}

// ---------------------------------------------------------------------------
// operator=(list&&) with is_always_equal=true (std::allocator path)
// ---------------------------------------------------------------------------

TEST(ListAssign, MoveAssignAlwaysEqual) {
  list<int> l1 = {1, 2, 3};
  list<int> l2 = {4, 5};
  l2 = std::move(l1);
  EXPECT_EQ(l2.size(), 3);
  EXPECT_TRUE(l1.empty());
}

}  // namespace
