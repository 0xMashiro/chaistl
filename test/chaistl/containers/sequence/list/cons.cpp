// SPDX-License-Identifier: Apache-2.0

// References:
// - std::list constructors:
//   https://en.cppreference.com/w/cpp/container/list/list
//   https://eel.is/c++draft/list.cons

#include <gtest/gtest.h>

#include <chaistl/containers/list.hpp>

#include <concepts>
#include <memory>
#include <string>
#include <vector>

namespace {

using chaistl::list;

TEST(ListCons, DefaultConstruct) {
  list<int> l;
  EXPECT_TRUE(l.empty());
  EXPECT_EQ(l.size(), 0);
}

TEST(ListCons, CountDefaultValue) {
  list<int> l(3);
  EXPECT_EQ(l.size(), 3);
  for (int value : l) {
    EXPECT_EQ(value, 0);
  }
}

TEST(ListCons, CountValue) {
  list<int> l(3, 42);
  EXPECT_EQ(l.size(), 3);
  for (int value : l) {
    EXPECT_EQ(value, 42);
  }
}

TEST(ListCons, IteratorRange) {
  int arr[] = {1, 2, 3, 4, 5};
  list<int> l(arr, arr + 5);
  EXPECT_EQ(l.size(), 5);
  auto it = l.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 5);
  EXPECT_EQ(it, l.end());
}

TEST(ListCons, InitializerList) {
  list<int> l = {1, 2, 3};
  EXPECT_EQ(l.size(), 3);
  EXPECT_EQ(l.front(), 1);
  EXPECT_EQ(l.back(), 3);
}

TEST(ListCons, CopyConstruct) {
  list<int> l1 = {1, 2, 3};
  list<int> l2(l1);
  EXPECT_EQ(l2.size(), 3);
  EXPECT_EQ(l2.front(), 1);
  EXPECT_EQ(l2.back(), 3);
  // Verify independence.
  l1.push_back(4);
  EXPECT_EQ(l1.size(), 4);
  EXPECT_EQ(l2.size(), 3);
}

TEST(ListCons, MoveConstruct) {
  list<int> l1 = {1, 2, 3};
  list<int> l2(std::move(l1));
  EXPECT_EQ(l2.size(), 3);
  EXPECT_TRUE(l1.empty());
}

TEST(ListCons, AllocatorConstruct) {
  chaistl::allocator<int> alloc;
  list<int> l(alloc);
  EXPECT_TRUE(l.empty());
}

TEST(ListCons, FromRange) {
  std::vector<int> v = {1, 2, 3, 4, 5};
  list<int> l(std::from_range, v);
  EXPECT_EQ(l.size(), 5);
  EXPECT_EQ(l.front(), 1);
  EXPECT_EQ(l.back(), 5);
}

TEST(ListCons, DeductionGuides) {
  std::vector<int> v = {1, 2, 3};

  chaistl::list from_iterators(v.begin(), v.end());
  static_assert(std::same_as<decltype(from_iterators), chaistl::list<int>>);
  EXPECT_EQ(from_iterators.size(), 3uz);

  chaistl::list from_range(std::from_range, v);
  static_assert(std::same_as<decltype(from_range), chaistl::list<int>>);
  EXPECT_EQ(from_range.size(), 3uz);
}

}  // namespace
