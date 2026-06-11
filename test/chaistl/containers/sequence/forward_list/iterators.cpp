// SPDX-License-Identifier: Apache-2.0

// References:
// - std::forward_list iterators:
//   https://en.cppreference.com/w/cpp/container/forward_list/begin
//   https://eel.is/c++draft/forwardlist.iterators

#include <gtest/gtest.h>

#include <chaistl/containers/forward_list.hpp>

#include <iterator>
#include <ranges>
#include <type_traits>

namespace {

using chaistl::forward_list;

TEST(ForwardListIterators, BeginEnd) {
  forward_list<int> fl = {1, 2, 3};
  auto it = fl.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(it, fl.end());
}

TEST(ForwardListIterators, BeginEndConst) {
  const forward_list<int> fl = {1, 2, 3};
  auto it = fl.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);
}

TEST(ForwardListIterators, CbeginCend) {
  forward_list<int> fl = {1, 2, 3};
  auto it = fl.cbegin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(it, fl.cend());
}

TEST(ForwardListIterators, EmptyBeginEnd) {
  forward_list<int> fl;
  EXPECT_EQ(fl.begin(), fl.end());
}

TEST(ForwardListIterators, ConstIteratorConversion) {
  forward_list<int> fl = {1, 2, 3};
  forward_list<int>::const_iterator cit = fl.begin();
  EXPECT_EQ(*cit, 1);
}

TEST(ForwardListIterators, PostIncrement) {
  forward_list<int> fl = {1, 2, 3};
  auto it = fl.begin();
  auto prev = it++;
  EXPECT_EQ(*prev, 1);
  EXPECT_EQ(*it, 2);
}

TEST(ForwardListIterators, IteratorDefaultConstruct) {
  forward_list<int>::iterator it;
  forward_list<int>::iterator it2;
  EXPECT_EQ(it, it2);
}

TEST(ForwardListIterators, ArrowOperator) {
  forward_list<std::pair<int, int>> fl = {{1, 2}, {3, 4}};
  auto it = fl.begin();
  EXPECT_EQ(it->first, 1);
  EXPECT_EQ(it->second, 2);
}

TEST(ForwardListIterators, ForwardIteratorConcept) {
  static_assert(std::forward_iterator<forward_list<int>::iterator>);
  static_assert(std::forward_iterator<forward_list<int>::const_iterator>);
}

}  // namespace
