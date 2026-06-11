// SPDX-License-Identifier: Apache-2.0

// References:
// - std::list capacity operations:
//   https://en.cppreference.com/w/cpp/container/list
//   https://eel.is/c++draft/list.capacity

#include <gtest/gtest.h>

#include <chaistl/containers/list.hpp>

#include <limits>

#include "../support.hpp"

namespace {

using chaistl::list;

TEST(ListCapacity, Empty) {
  list<int> l;
  EXPECT_TRUE(l.empty());
  EXPECT_EQ(l.size(), 0);
}

TEST(ListCapacity, NonEmpty) {
  list<int> l = {1, 2, 3};
  EXPECT_FALSE(l.empty());
  EXPECT_EQ(l.size(), 3);
}

TEST(ListCapacity, MaxSize) {
  list<int> l;
  EXPECT_GT(l.max_size(), 0);
  // max_size should be at least as large as what we can reasonably allocate.
  EXPECT_GE(l.max_size(), l.size());
}

TEST(ListCapacity, MaxSizeWithSmallAllocator) {
  list<int, chaistl::test::SmallMaxAllocator<int>> l;
  EXPECT_EQ(l.max_size(), 3uz);
}

TEST(ListCapacity, ResizeGrow) {
  list<int> l = {1, 2};
  l.resize(5);
  EXPECT_EQ(l.size(), 5);
  auto it = l.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 0);
  EXPECT_EQ(*it++, 0);
  EXPECT_EQ(*it++, 0);
}

TEST(ListCapacity, ResizeGrowWithValue) {
  list<int> l = {1, 2};
  l.resize(5, 42);
  EXPECT_EQ(l.size(), 5);
  auto it = l.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 42);
  EXPECT_EQ(*it++, 42);
  EXPECT_EQ(*it++, 42);
}

TEST(ListCapacity, ResizeShrink) {
  list<int> l = {1, 2, 3, 4, 5};
  l.resize(2);
  EXPECT_EQ(l.size(), 2);
  EXPECT_EQ(l.front(), 1);
  EXPECT_EQ(l.back(), 2);
}

TEST(ListCapacity, ResizeToZero) {
  list<int> l = {1, 2, 3};
  l.resize(0);
  EXPECT_TRUE(l.empty());
}

TEST(ListCapacity, ResizeNoOp) {
  list<int> l = {1, 2, 3};
  l.resize(3);
  EXPECT_EQ(l.size(), 3);
  EXPECT_EQ(l.front(), 1);
  EXPECT_EQ(l.back(), 3);
}

}  // namespace
