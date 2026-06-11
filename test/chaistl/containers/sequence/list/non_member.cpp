// SPDX-License-Identifier: Apache-2.0

// References:
// - std::list non-member functions:
//   https://en.cppreference.com/w/cpp/container/list/operator_cmp
//   https://en.cppreference.com/w/cpp/container/list/erase2
//   https://en.cppreference.com/w/cpp/container/list/swap2

#include <gtest/gtest.h>

#include <chaistl/containers/list.hpp>

namespace {

using chaistl::list;

// ---------------------------------------------------------------------------
// operator==
// ---------------------------------------------------------------------------

TEST(ListNonMember, EqualSameContent) {
  list<int> l1 = {1, 2, 3};
  list<int> l2 = {1, 2, 3};
  EXPECT_TRUE(l1 == l2);
}

TEST(ListNonMember, EqualDifferentSize) {
  list<int> l1 = {1, 2, 3};
  list<int> l2 = {1, 2};
  EXPECT_FALSE(l1 == l2);
}

TEST(ListNonMember, EqualDifferentContent) {
  list<int> l1 = {1, 2, 3};
  list<int> l2 = {1, 2, 4};
  EXPECT_FALSE(l1 == l2);
}

// ---------------------------------------------------------------------------
// operator<=>
// ---------------------------------------------------------------------------

TEST(ListNonMember, ThreeWayEqual) {
  list<int> l1 = {1, 2, 3};
  list<int> l2 = {1, 2, 3};
  EXPECT_EQ(l1 <=> l2, std::strong_ordering::equal);
}

TEST(ListNonMember, ThreeWayLess) {
  list<int> l1 = {1, 2, 3};
  list<int> l2 = {1, 2, 4};
  EXPECT_EQ(l1 <=> l2, std::strong_ordering::less);
}

TEST(ListNonMember, ThreeWayGreater) {
  list<int> l1 = {1, 3};
  list<int> l2 = {1, 2, 4};
  EXPECT_EQ(l1 <=> l2, std::strong_ordering::greater);
}

TEST(ListNonMember, ThreeWayShorter) {
  list<int> l1 = {1, 2};
  list<int> l2 = {1, 2, 3};
  EXPECT_EQ(l1 <=> l2, std::strong_ordering::less);
}

// ---------------------------------------------------------------------------
// erase
// ---------------------------------------------------------------------------

TEST(ListNonMember, EraseValue) {
  list<int> l = {1, 2, 3, 2, 4};
  auto count = chaistl::erase(l, 2);
  EXPECT_EQ(count, 2);
  EXPECT_EQ(l.size(), 3);
  auto it = l.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 4);
}

TEST(ListNonMember, EraseNoMatch) {
  list<int> l = {1, 2, 3};
  auto count = chaistl::erase(l, 99);
  EXPECT_EQ(count, 0);
  EXPECT_EQ(l.size(), 3);
}

// ---------------------------------------------------------------------------
// erase_if
// ---------------------------------------------------------------------------

TEST(ListNonMember, EraseIf) {
  list<int> l = {1, 2, 3, 4, 5, 6};
  auto count = chaistl::erase_if(l, [](int x) {
    return x % 2 == 0;
  });
  EXPECT_EQ(count, 3);
  EXPECT_EQ(l.size(), 3);
  auto it = l.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 5);
}

// ---------------------------------------------------------------------------
// swap (non-member)
// ---------------------------------------------------------------------------

TEST(ListNonMember, Swap) {
  list<int> l1 = {1, 2, 3};
  list<int> l2 = {4, 5};
  chaistl::swap(l1, l2);
  EXPECT_EQ(l1.size(), 2);
  EXPECT_EQ(l2.size(), 3);
  EXPECT_EQ(l1.front(), 4);
  EXPECT_EQ(l2.front(), 1);
}

}  // namespace
