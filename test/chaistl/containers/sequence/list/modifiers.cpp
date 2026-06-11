// SPDX-License-Identifier: Apache-2.0

// References:
// - std::list modifiers:
//   https://en.cppreference.com/w/cpp/container/list
//   https://eel.is/c++draft/list.modifiers

#include <gtest/gtest.h>

#include <chaistl/containers/list.hpp>

#include <string>

namespace {

using chaistl::list;

TEST(ListModifiers, PushBack) {
  list<int> l;
  l.push_back(1);
  l.push_back(2);
  l.push_back(3);
  EXPECT_EQ(l.size(), 3);
  EXPECT_EQ(l.back(), 3);
  EXPECT_EQ(l.front(), 1);
}

TEST(ListModifiers, PushFront) {
  list<int> l;
  l.push_front(1);
  l.push_front(2);
  l.push_front(3);
  EXPECT_EQ(l.size(), 3);
  EXPECT_EQ(l.front(), 3);
  EXPECT_EQ(l.back(), 1);
}

TEST(ListModifiers, PopBack) {
  list<int> l = {1, 2, 3};
  l.pop_back();
  EXPECT_EQ(l.size(), 2);
  EXPECT_EQ(l.back(), 2);
}

TEST(ListModifiers, PopFront) {
  list<int> l = {1, 2, 3};
  l.pop_front();
  EXPECT_EQ(l.size(), 2);
  EXPECT_EQ(l.front(), 2);
}

TEST(ListModifiers, EmplaceBack) {
  list<std::string> l;
  l.emplace_back(5, 'a');
  EXPECT_EQ(l.back(), "aaaaa");
}

TEST(ListModifiers, EmplaceFront) {
  list<std::string> l;
  l.emplace_front(5, 'b');
  EXPECT_EQ(l.front(), "bbbbb");
}

TEST(ListModifiers, InsertSingle) {
  list<int> l = {1, 3};
  auto it = l.begin();
  ++it;
  auto result = l.insert(it, 2);
  EXPECT_EQ(*result, 2);
  EXPECT_EQ(l.size(), 3);
  auto check = l.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 3);
}

TEST(ListModifiers, InsertCount) {
  list<int> l = {1, 4};
  auto it = l.begin();
  ++it;
  l.insert(it, 2, 2);
  EXPECT_EQ(l.size(), 4);
  auto check = l.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 4);
}

TEST(ListModifiers, InsertRange) {
  list<int> l = {1, 5};
  int arr[] = {2, 3, 4};
  auto it = l.begin();
  ++it;
  l.insert(it, arr, arr + 3);
  EXPECT_EQ(l.size(), 5);
  auto check = l.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 3);
  EXPECT_EQ(*check++, 4);
  EXPECT_EQ(*check++, 5);
}

TEST(ListModifiers, EraseSingle) {
  list<int> l = {1, 2, 3};
  auto it = l.begin();
  ++it;
  auto result = l.erase(it);
  EXPECT_EQ(*result, 3);
  EXPECT_EQ(l.size(), 2);
  EXPECT_EQ(l.front(), 1);
  EXPECT_EQ(l.back(), 3);
}

TEST(ListModifiers, EraseRange) {
  list<int> l = {1, 2, 3, 4, 5};
  auto first = l.begin();
  ++first;
  auto last = first;
  ++last;
  ++last;
  auto result = l.erase(first, last);
  EXPECT_EQ(*result, 4);
  EXPECT_EQ(l.size(), 3);
  auto check = l.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 4);
  EXPECT_EQ(*check++, 5);
}

TEST(ListModifiers, Clear) {
  list<int> l = {1, 2, 3};
  l.clear();
  EXPECT_TRUE(l.empty());
  EXPECT_EQ(l.size(), 0);
}

TEST(ListModifiers, Swap) {
  list<int> l1 = {1, 2, 3};
  list<int> l2 = {4, 5};
  l1.swap(l2);
  EXPECT_EQ(l1.size(), 2);
  EXPECT_EQ(l2.size(), 3);
  EXPECT_EQ(l1.front(), 4);
  EXPECT_EQ(l2.front(), 1);
}

TEST(ListModifiers, ResizeGrow) {
  list<int> l = {1, 2};
  l.resize(5);
  EXPECT_EQ(l.size(), 5);
  auto it = l.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 0);
  EXPECT_EQ(*it++, 0);
  EXPECT_EQ(*it++, 0);
}

TEST(ListModifiers, ResizeGrowWithValue) {
  list<int> l = {1, 2};
  l.resize(5, 42);
  EXPECT_EQ(l.size(), 5);
  auto it = l.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 42);
  EXPECT_EQ(*it++, 42);
  EXPECT_EQ(*it++, 42);
}

TEST(ListModifiers, ResizeShrink) {
  list<int> l = {1, 2, 3, 4, 5};
  l.resize(2);
  EXPECT_EQ(l.size(), 2);
  EXPECT_EQ(l.front(), 1);
  EXPECT_EQ(l.back(), 2);
}

// ---------------------------------------------------------------------------
// Edge cases for insert/erase
// ---------------------------------------------------------------------------

TEST(ListModifiers, InsertCountZero) {
  list<int> l = {1, 2, 3};
  auto it = l.begin();
  ++it;
  auto result = l.insert(it, 0, 99);
  EXPECT_EQ(result, it);
  EXPECT_EQ(l.size(), 3);
}

TEST(ListModifiers, InsertEmptyRange) {
  list<int> l = {1, 2, 3};
  const int* empty = nullptr;  // an empty [first, last) range
  auto it = l.begin();
  ++it;
  auto result = l.insert(it, empty, empty);
  EXPECT_EQ(result, it);
  EXPECT_EQ(l.size(), 3);
}

TEST(ListModifiers, EraseEmptyRange) {
  list<int> l = {1, 2, 3};
  auto it = l.begin();
  ++it;
  auto result = l.erase(it, it);
  EXPECT_EQ(result, it);
  EXPECT_EQ(l.size(), 3);
}

}  // namespace
