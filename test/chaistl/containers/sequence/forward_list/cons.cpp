// SPDX-License-Identifier: Apache-2.0

// References:
// - std::forward_list constructors:
//   https://en.cppreference.com/w/cpp/container/forward_list/forward_list
//   https://eel.is/c++draft/forwardlist.cons

#include <gtest/gtest.h>

#include <chaistl/containers/forward_list.hpp>

#include <concepts>
#include <memory>
#include <string>
#include <vector>

namespace {

using chaistl::forward_list;

TEST(ForwardListCons, DefaultConstruct) {
  forward_list<int> fl;
  EXPECT_TRUE(fl.empty());
  EXPECT_EQ(fl.size(), 0);
}

TEST(ForwardListCons, CountDefaultValue) {
  forward_list<int> fl(3);
  EXPECT_EQ(fl.size(), 3);
  for (int value : fl) {
    EXPECT_EQ(value, 0);
  }
}

TEST(ForwardListCons, CountValue) {
  forward_list<int> fl(3, 42);
  EXPECT_EQ(fl.size(), 3);
  for (int value : fl) {
    EXPECT_EQ(value, 42);
  }
}

TEST(ForwardListCons, IteratorRange) {
  int arr[] = {1, 2, 3, 4, 5};
  forward_list<int> fl(arr, arr + 5);
  EXPECT_EQ(fl.size(), 5);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 5);
  EXPECT_EQ(it, fl.end());
}

TEST(ForwardListCons, InitializerList) {
  forward_list<int> fl = {1, 2, 3};
  EXPECT_EQ(fl.size(), 3);
  EXPECT_EQ(fl.front(), 1);
}

TEST(ForwardListCons, CopyConstruct) {
  forward_list<int> fl1 = {1, 2, 3};
  forward_list<int> fl2(fl1);
  EXPECT_EQ(fl2.size(), 3);
  EXPECT_EQ(fl2.front(), 1);
  // Verify independence.
  fl1.push_front(0);
  EXPECT_EQ(fl1.size(), 4);
  EXPECT_EQ(fl2.size(), 3);
}

TEST(ForwardListCons, MoveConstruct) {
  forward_list<int> fl1 = {1, 2, 3};
  forward_list<int> fl2(std::move(fl1));
  EXPECT_EQ(fl2.size(), 3);
  EXPECT_TRUE(fl1.empty());
}

TEST(ForwardListCons, AllocatorConstruct) {
  chaistl::allocator<int> alloc;
  forward_list<int> fl(alloc);
  EXPECT_TRUE(fl.empty());
}

TEST(ForwardListCons, FromRange) {
  std::vector<int> v = {1, 2, 3, 4, 5};
  forward_list<int> fl(std::from_range, v);
  EXPECT_EQ(fl.size(), 5);
  EXPECT_EQ(fl.front(), 1);
}

TEST(ForwardListCons, CopyWithAllocator) {
  forward_list<int> fl1 = {1, 2, 3};
  chaistl::allocator<int> alloc;
  forward_list<int> fl2(fl1, alloc);
  EXPECT_EQ(fl2.size(), 3);
  EXPECT_EQ(fl2.front(), 1);
}

TEST(ForwardListCons, MoveWithCompatibleAllocator) {
  forward_list<int> fl1 = {1, 2, 3};
  forward_list<int> fl2(std::move(fl1), chaistl::allocator<int>{});
  EXPECT_EQ(fl2.size(), 3);
  EXPECT_TRUE(fl1.empty());
}

TEST(ForwardListCons, MoveOnlyElements) {
  forward_list<std::unique_ptr<int>> fl;
  fl.push_front(std::make_unique<int>(1));
  fl.push_front(std::make_unique<int>(2));
  EXPECT_EQ(fl.size(), 2);

  forward_list<std::unique_ptr<int>> fl2(std::move(fl));
  EXPECT_EQ(fl2.size(), 2);
  EXPECT_TRUE(fl.empty());
  EXPECT_EQ(*fl2.front(), 2);
}

TEST(ForwardListCons, CountZero) {
  forward_list<int> fl(0);
  EXPECT_TRUE(fl.empty());
}

TEST(ForwardListCons, CountZeroWithValue) {
  forward_list<int> fl(0, 42);
  EXPECT_TRUE(fl.empty());
}

TEST(ForwardListCons, EmptyRange) {
  std::vector<int> v;
  forward_list<int> fl(v.begin(), v.end());
  EXPECT_TRUE(fl.empty());
}

TEST(ForwardListCons, DeductionGuides) {
  std::vector<int> v = {1, 2, 3};

  chaistl::forward_list from_iterators(v.begin(), v.end());
  static_assert(std::same_as<decltype(from_iterators), chaistl::forward_list<int>>);
  EXPECT_EQ(from_iterators.size(), 3uz);

  chaistl::forward_list from_range(std::from_range, v);
  static_assert(std::same_as<decltype(from_range), chaistl::forward_list<int>>);
  EXPECT_EQ(from_range.size(), 3uz);
}

}  // namespace
