// SPDX-License-Identifier: Apache-2.0

// References:
// - std::list element access:
//   https://en.cppreference.com/w/cpp/container/list/front
//   https://en.cppreference.com/w/cpp/container/list/back
//   https://en.cppreference.com/w/cpp/container/list/begin

#include <gtest/gtest.h>

#include <chaistl/containers/list.hpp>

namespace {

using chaistl::list;

// ---------------------------------------------------------------------------
// const front/back
// ---------------------------------------------------------------------------

TEST(ListAccessors, ConstFront) {
  const list<int> l = {1, 2, 3};
  EXPECT_EQ(l.front(), 1);
}

TEST(ListAccessors, ConstBack) {
  const list<int> l = {1, 2, 3};
  EXPECT_EQ(l.back(), 3);
}

// ---------------------------------------------------------------------------
// cbegin / cend
// ---------------------------------------------------------------------------

TEST(ListAccessors, CBegin) {
  list<int> l = {1, 2, 3};
  auto it = l.cbegin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);
}

TEST(ListAccessors, CEnd) {
  list<int> l = {1, 2, 3};
  auto it = l.cbegin();
  std::advance(it, 3);
  EXPECT_EQ(it, l.cend());
}

// ---------------------------------------------------------------------------
// const_iterator construction from iterator
// ---------------------------------------------------------------------------

TEST(ListAccessors, ConstIteratorFromIterator) {
  list<int> l = {1, 2, 3};
  list<int>::iterator it = l.begin();
  list<int>::const_iterator cit = it;
  EXPECT_EQ(*cit, 1);
}

// ---------------------------------------------------------------------------
// iterator operator--(int) (post-decrement)
// ---------------------------------------------------------------------------

TEST(ListAccessors, IteratorPostDecrement) {
  list<int> l = {1, 2, 3};
  auto it = l.end();
  --it;  // now at 3
  auto prev = it--;
  EXPECT_EQ(*prev, 3);
  EXPECT_EQ(*it, 2);
}

// ---------------------------------------------------------------------------
// const_iterator operations
// ---------------------------------------------------------------------------

TEST(ListAccessors, ConstIteratorIncrement) {
  list<int> l = {1, 2, 3};
  auto it = l.cbegin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it, 3);
}

TEST(ListAccessors, ConstIteratorDecrement) {
  list<int> l = {1, 2, 3};
  auto it = l.cend();
  --it;
  EXPECT_EQ(*it, 3);
  --it;
  EXPECT_EQ(*it, 2);
}

// ---------------------------------------------------------------------------
// prepend_range / append_range
// ---------------------------------------------------------------------------

TEST(ListAccessors, PrependRange) {
  list<int> l = {3, 4, 5};
  std::vector<int> v = {1, 2};
  l.prepend_range(v);
  EXPECT_EQ(l.size(), 5);
  auto it = l.begin();
  // prepend_range uses emplace_front for each element, so order is reversed.
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 5);
}

TEST(ListAccessors, AppendRange) {
  list<int> l = {1, 2};
  std::vector<int> v = {3, 4, 5};
  l.append_range(v);
  EXPECT_EQ(l.size(), 5);
  auto it = l.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 5);
}

// ---------------------------------------------------------------------------
// insert_range
// ---------------------------------------------------------------------------

TEST(ListAccessors, InsertRange) {
  list<int> l = {1, 5};
  std::vector<int> v = {2, 3, 4};
  auto it = l.begin();
  ++it;
  auto result = l.insert_range(it, v);
  EXPECT_EQ(l.size(), 5);
  EXPECT_EQ(*result, 2);
  auto check = l.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 3);
  EXPECT_EQ(*check++, 4);
  EXPECT_EQ(*check++, 5);
}

// ---------------------------------------------------------------------------
// reverse_iterator
// ---------------------------------------------------------------------------

TEST(ListAccessors, ReverseIterator) {
  list<int> l = {1, 2, 3};
  auto rit = l.rbegin();
  EXPECT_EQ(*rit, 3);
  ++rit;
  EXPECT_EQ(*rit, 2);
  ++rit;
  EXPECT_EQ(*rit, 1);
  ++rit;
  EXPECT_EQ(rit, l.rend());
}

TEST(ListAccessors, ConstReverseIterator) {
  const list<int> l = {1, 2, 3};
  auto rit = l.rbegin();
  EXPECT_EQ(*rit, 3);
  ++rit;
  EXPECT_EQ(*rit, 2);
  ++rit;
  EXPECT_EQ(*rit, 1);
  ++rit;
  EXPECT_EQ(rit, l.rend());
}

TEST(ListAccessors, CRBegin) {
  list<int> l = {1, 2, 3};
  auto rit = l.crbegin();
  EXPECT_EQ(*rit, 3);
}

TEST(ListAccessors, CREnd) {
  list<int> l = {1, 2, 3};
  auto rit = l.crbegin();
  std::advance(rit, 3);
  EXPECT_EQ(rit, l.crend());
}

}  // namespace
