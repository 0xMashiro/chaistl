// SPDX-License-Identifier: Apache-2.0

// References:
// - std::forward_list element access:
//   https://en.cppreference.com/w/cpp/container/forward_list/front
//   https://eel.is/c++draft/forwardlist.access

#include <gtest/gtest.h>

#include <chaistl/containers/forward_list.hpp>

namespace {

using chaistl::forward_list;

TEST(ForwardListAccess, FrontNonConst) {
  forward_list<int> fl = {10, 20, 30};
  EXPECT_EQ(fl.front(), 10);
  fl.front() = 100;
  EXPECT_EQ(fl.front(), 100);
}

TEST(ForwardListAccess, FrontConst) {
  const forward_list<int> fl = {10, 20, 30};
  EXPECT_EQ(fl.front(), 10);
}

TEST(ForwardListAccess, BeforeBeginNonConst) {
  forward_list<int> fl = {1, 2, 3};
  auto it = fl.before_begin();
  ++it;
  EXPECT_EQ(*it, 1);
}

TEST(ForwardListAccess, BeforeBeginConst) {
  const forward_list<int> fl = {1, 2, 3};
  auto it = fl.before_begin();
  ++it;
  EXPECT_EQ(*it, 1);
}

TEST(ForwardListAccess, CbeforeBegin) {
  forward_list<int> fl = {1, 2, 3};
  auto it = fl.cbefore_begin();
  ++it;
  EXPECT_EQ(*it, 1);
  static_assert(std::is_same_v<decltype(it), chaistl::forward_list<int>::const_iterator>);
}

TEST(ForwardListAccess, BeforeBeginOfEmpty) {
  forward_list<int> fl;
  auto it = fl.before_begin();
  ++it;
  EXPECT_EQ(it, fl.end());
}

}  // namespace
