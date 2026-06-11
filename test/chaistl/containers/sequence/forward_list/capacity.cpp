// SPDX-License-Identifier: Apache-2.0

// References:
// - std::forward_list capacity:
//   https://en.cppreference.com/w/cpp/container/forward_list/empty
//   https://eel.is/c++draft/forwardlist.capacity

#include <gtest/gtest.h>

#include <chaistl/containers/forward_list.hpp>

#include "../support.hpp"

namespace {

using chaistl::forward_list;

TEST(ForwardListCapacity, Empty) {
  forward_list<int> fl;
  EXPECT_TRUE(fl.empty());

  fl.push_front(1);
  EXPECT_FALSE(fl.empty());

  fl.pop_front();
  EXPECT_TRUE(fl.empty());
}

TEST(ForwardListCapacity, Size) {
  forward_list<int> fl;
  EXPECT_EQ(fl.size(), 0);

  fl.push_front(1);
  EXPECT_EQ(fl.size(), 1);

  fl.push_front(2);
  EXPECT_EQ(fl.size(), 2);
}

TEST(ForwardListCapacity, MaxSize) {
  forward_list<int> fl;
  EXPECT_GT(fl.max_size(), 0);
}

TEST(ForwardListCapacity, MaxSizeWithSmallAllocator) {
  forward_list<int, chaistl::test::SmallMaxAllocator<int>> fl;
  EXPECT_EQ(fl.max_size(), 3);
}

TEST(ForwardListCapacity, ResizeGrow) {
  forward_list<int> fl = {1, 2, 3};
  fl.resize(5);
  EXPECT_EQ(fl.size(), 5);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 0);
  EXPECT_EQ(*it++, 0);
  EXPECT_EQ(it, fl.end());
}

TEST(ForwardListCapacity, ResizeShrink) {
  forward_list<int> fl = {1, 2, 3, 4, 5};
  fl.resize(3);
  EXPECT_EQ(fl.size(), 3);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(it, fl.end());
}

TEST(ForwardListCapacity, ResizeGrowWithValue) {
  forward_list<int> fl = {1, 2};
  fl.resize(5, 99);
  EXPECT_EQ(fl.size(), 5);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 99);
  EXPECT_EQ(*it++, 99);
  EXPECT_EQ(*it++, 99);
  EXPECT_EQ(it, fl.end());
}

TEST(ForwardListCapacity, ResizeShrinkWithValue) {
  forward_list<int> fl = {1, 2, 3, 4, 5};
  fl.resize(2, 99);
  EXPECT_EQ(fl.size(), 2);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(it, fl.end());
}

TEST(ForwardListCapacity, ResizeToZero) {
  forward_list<int> fl = {1, 2, 3};
  fl.resize(0);
  EXPECT_TRUE(fl.empty());
}

TEST(ForwardListCapacity, ResizeZeroToCount) {
  forward_list<int> fl;
  fl.resize(3);
  EXPECT_EQ(fl.size(), 3);
}

}  // namespace
