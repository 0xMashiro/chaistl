// SPDX-License-Identifier: Apache-2.0

// References:
// - std::list operations:
//   https://en.cppreference.com/w/cpp/container/list
//   https://eel.is/c++draft/list.ops

#include <gtest/gtest.h>

#include <chaistl/containers/list.hpp>

#include "../support.hpp"

namespace {

using chaistl::list;

TEST(ListOperations, SpliceAll) {
  list<int> l1 = {1, 2, 3};
  list<int> l2 = {4, 5, 6};
  l1.splice(l1.end(), l2);
  EXPECT_EQ(l1.size(), 6);
  EXPECT_TRUE(l2.empty());
  auto it = l1.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 5);
  EXPECT_EQ(*it++, 6);
}

TEST(ListOperations, SpliceSingle) {
  list<int> l1 = {1, 3};
  list<int> l2 = {2};
  auto it = l1.begin();
  ++it;
  l1.splice(it, l2, l2.begin());
  EXPECT_EQ(l1.size(), 3);
  EXPECT_TRUE(l2.empty());
  auto check = l1.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 3);
}

TEST(ListOperations, SpliceRange) {
  list<int> l1 = {1, 5};
  list<int> l2 = {2, 3, 4};
  l1.splice(std::next(l1.begin()), l2, l2.begin(), l2.end());
  EXPECT_EQ(l1.size(), 5);
  EXPECT_TRUE(l2.empty());
  auto check = l1.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 3);
  EXPECT_EQ(*check++, 4);
  EXPECT_EQ(*check++, 5);
}

TEST(ListOperations, Remove) {
  list<int> l = {1, 2, 3, 2, 4, 2, 5};
  auto count = l.remove(2);
  EXPECT_EQ(count, 3);
  EXPECT_EQ(l.size(), 4);
  auto check = l.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 3);
  EXPECT_EQ(*check++, 4);
  EXPECT_EQ(*check++, 5);
}

TEST(ListOperations, RemoveIf) {
  list<int> l = {1, 2, 3, 4, 5, 6};
  auto count = l.remove_if([](int x) {
    return x % 2 == 0;
  });
  EXPECT_EQ(count, 3);
  EXPECT_EQ(l.size(), 3);
  auto check = l.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 3);
  EXPECT_EQ(*check++, 5);
}

TEST(ListOperations, Unique) {
  list<int> l = {1, 1, 2, 2, 2, 3, 3, 1};
  auto count = l.unique();
  EXPECT_EQ(count, 4);
  EXPECT_EQ(l.size(), 4);
  auto check = l.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 3);
  EXPECT_EQ(*check++, 1);
}

TEST(ListOperations, UniqueWithPredicate) {
  // unique only compares consecutive elements.
  list<int> l = {1, 3, 2, 4, 5, 7};
  // Remove consecutive elements with same parity.
  auto count = l.unique([](int a, int b) {
    return (a % 2) == (b % 2);
  });
  EXPECT_EQ(count, 3);
  EXPECT_EQ(l.size(), 3);
  auto check = l.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 5);
}

TEST(ListOperations, Merge) {
  list<int> l1 = {1, 3, 5};
  list<int> l2 = {2, 4, 6};
  l1.merge(l2);
  EXPECT_EQ(l1.size(), 6);
  EXPECT_TRUE(l2.empty());
  auto check = l1.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 3);
  EXPECT_EQ(*check++, 4);
  EXPECT_EQ(*check++, 5);
  EXPECT_EQ(*check++, 6);
}

TEST(ListOperations, MergeWithComparator) {
  list<int> l1 = {5, 3, 1};
  list<int> l2 = {6, 4, 2};
  l1.merge(l2, std::greater<int>{});
  EXPECT_EQ(l1.size(), 6);
  EXPECT_TRUE(l2.empty());
  auto check = l1.begin();
  EXPECT_EQ(*check++, 6);
  EXPECT_EQ(*check++, 5);
  EXPECT_EQ(*check++, 4);
  EXPECT_EQ(*check++, 3);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 1);
}

TEST(ListOperations, Sort) {
  list<int> l = {3, 1, 4, 1, 5, 9, 2, 6};
  l.sort();
  EXPECT_EQ(l.size(), 8);
  auto check = l.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 3);
  EXPECT_EQ(*check++, 4);
  EXPECT_EQ(*check++, 5);
  EXPECT_EQ(*check++, 6);
  EXPECT_EQ(*check++, 9);
}

TEST(ListOperations, SortWithComparator) {
  list<int> l = {3, 1, 4, 1, 5, 9, 2, 6};
  l.sort(std::greater<int>{});
  EXPECT_EQ(l.size(), 8);
  auto check = l.begin();
  EXPECT_EQ(*check++, 9);
  EXPECT_EQ(*check++, 6);
  EXPECT_EQ(*check++, 5);
  EXPECT_EQ(*check++, 4);
  EXPECT_EQ(*check++, 3);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 1);
}

TEST(ListOperations, SortUsesContainerAllocatorForTemporaryLists) {
  using Alloc = chaistl::test::TaggedAllocator<int>;

  list<int, Alloc> l({3, 1, 4, 1, 5, 9, 2, 6}, Alloc{7});

  l.sort();

  EXPECT_EQ(l.size(), 8);
  auto check = l.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 3);
  EXPECT_EQ(*check++, 4);
  EXPECT_EQ(*check++, 5);
  EXPECT_EQ(*check++, 6);
  EXPECT_EQ(*check++, 9);
  EXPECT_EQ(l.get_allocator().id, 7);
}

TEST(ListOperations, Reverse) {
  list<int> l = {1, 2, 3, 4, 5};
  l.reverse();
  EXPECT_EQ(l.size(), 5);
  auto check = l.begin();
  EXPECT_EQ(*check++, 5);
  EXPECT_EQ(*check++, 4);
  EXPECT_EQ(*check++, 3);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 1);
}

TEST(ListOperations, ReverseEmpty) {
  list<int> l;
  l.reverse();
  EXPECT_TRUE(l.empty());
}

TEST(ListOperations, ReverseSingle) {
  list<int> l = {42};
  l.reverse();
  EXPECT_EQ(l.size(), 1);
  EXPECT_EQ(l.front(), 42);
}

}  // namespace
