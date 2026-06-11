// SPDX-License-Identifier: Apache-2.0

// References:
// - std::forward_list assign:
//   https://en.cppreference.com/w/cpp/container/forward_list/assign
//   https://eel.is/c++draft/forwardlist.modifiers

#include <gtest/gtest.h>

#include <chaistl/containers/forward_list.hpp>

#include <vector>

namespace {

using chaistl::forward_list;

TEST(ForwardListAssign, IteratorRange) {
  forward_list<int> fl;
  std::vector<int> v = {1, 2, 3, 4, 5};
  fl.assign(v.begin(), v.end());
  EXPECT_EQ(fl.size(), 5);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 5);
}

TEST(ForwardListAssign, IteratorRangeReplace) {
  forward_list<int> fl = {9, 9, 9};
  std::vector<int> v = {1, 2};
  fl.assign(v.begin(), v.end());
  EXPECT_EQ(fl.size(), 2);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
}

TEST(ForwardListAssign, IteratorRangeEmpty) {
  forward_list<int> fl = {1, 2, 3};
  std::vector<int> v;
  fl.assign(v.begin(), v.end());
  EXPECT_TRUE(fl.empty());
}

TEST(ForwardListAssign, IteratorRangeReuseNodes) {
  forward_list<int> fl = {1, 2, 3};
  std::vector<int> v = {4, 5, 6};
  fl.assign(v.begin(), v.end());
  EXPECT_EQ(fl.size(), 3);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 5);
  EXPECT_EQ(*it++, 6);
}

TEST(ForwardListAssign, IteratorRangeLonger) {
  forward_list<int> fl = {1, 2};
  std::vector<int> v = {3, 4, 5, 6};
  fl.assign(v.begin(), v.end());
  EXPECT_EQ(fl.size(), 4);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 5);
  EXPECT_EQ(*it++, 6);
}

TEST(ForwardListAssign, CountValue) {
  forward_list<int> fl = {1, 2, 3};
  fl.assign(2, 99);
  EXPECT_EQ(fl.size(), 2);
  EXPECT_EQ(fl.front(), 99);
  EXPECT_EQ(*std::next(fl.begin()), 99);
}

TEST(ForwardListAssign, CountValueLonger) {
  forward_list<int> fl = {1, 2};
  fl.assign(4, 42);
  EXPECT_EQ(fl.size(), 4);
  for (int v : fl) {
    EXPECT_EQ(v, 42);
  }
}

TEST(ForwardListAssign, InitializerList) {
  forward_list<int> fl = {9, 8, 7};
  fl.assign({1, 2, 3});
  EXPECT_EQ(fl.size(), 3);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
}

TEST(ForwardListAssign, AssignRange) {
  forward_list<int> fl = {9, 9};
  std::vector<int> v = {1, 2, 3};
  fl.assign_range(v);
  EXPECT_EQ(fl.size(), 3);
}

}  // namespace
