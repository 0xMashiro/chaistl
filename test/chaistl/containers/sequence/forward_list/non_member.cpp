// SPDX-License-Identifier: Apache-2.0

// References:
// - std::forward_list non-member comparisons:
//   https://en.cppreference.com/w/cpp/container/forward_list/operator_cmp
//   https://eel.is/c++draft/forwardlist.nonmember

#include <gtest/gtest.h>

#include <chaistl/containers/forward_list.hpp>

namespace {

using chaistl::forward_list;

TEST(ForwardListNonMember, Equal) {
  forward_list<int> fl1 = {1, 2, 3};
  forward_list<int> fl2 = {1, 2, 3};
  EXPECT_TRUE(fl1 == fl2);
}

TEST(ForwardListNonMember, NotEqual) {
  forward_list<int> fl1 = {1, 2, 3};
  forward_list<int> fl2 = {1, 2, 4};
  EXPECT_FALSE(fl1 == fl2);
}

TEST(ForwardListNonMember, EqualDifferentSize) {
  forward_list<int> fl1 = {1, 2, 3};
  forward_list<int> fl2 = {1, 2};
  EXPECT_FALSE(fl1 == fl2);
}

TEST(ForwardListNonMember, ThreeWayCompare) {
  forward_list<int> fl1 = {1, 2, 3};
  forward_list<int> fl2 = {1, 2, 4};
  EXPECT_LT(fl1, fl2);
}

TEST(ForwardListNonMember, ThreeWayEqual) {
  forward_list<int> fl1 = {1, 2, 3};
  forward_list<int> fl2 = {1, 2, 3};
  EXPECT_EQ(fl1 <=> fl2, std::strong_ordering::equal);
}

}  // namespace
