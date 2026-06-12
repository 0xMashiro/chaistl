// SPDX-License-Identifier: Apache-2.0

// References:
// - std::forward_list modifiers:
//   https://en.cppreference.com/w/cpp/container/forward_list
//   https://eel.is/c++draft/forwardlist.modifiers

#include <gtest/gtest.h>

#include <chaistl/containers/forward_list.hpp>

#include <memory>
#include <string>

namespace {

using chaistl::forward_list;

// =======================================================================
// push_front / emplace_front / pop_front
// =======================================================================

TEST(ForwardListModifiers, PushFrontLvalue) {
  forward_list<int> fl;
  int x = 42;
  fl.push_front(x);
  EXPECT_EQ(fl.size(), 1);
  EXPECT_EQ(fl.front(), 42);
}

TEST(ForwardListModifiers, PushFrontRvalue) {
  forward_list<int> fl;
  fl.push_front(42);
  EXPECT_EQ(fl.size(), 1);
  EXPECT_EQ(fl.front(), 42);
}

TEST(ForwardListModifiers, EmplaceFront) {
  forward_list<std::pair<int, int>> fl;
  fl.emplace_front(1, 2);
  EXPECT_EQ(fl.size(), 1);
  EXPECT_EQ(fl.front().first, 1);
  EXPECT_EQ(fl.front().second, 2);
}

TEST(ForwardListModifiers, PopFront) {
  forward_list<int> fl = {1, 2, 3};
  fl.pop_front();
  EXPECT_EQ(fl.size(), 2);
  EXPECT_EQ(fl.front(), 2);

  fl.pop_front();
  EXPECT_EQ(fl.size(), 1);
  EXPECT_EQ(fl.front(), 3);

  fl.pop_front();
  EXPECT_TRUE(fl.empty());
}

TEST(ForwardListModifiers, EmplaceFrontMaintainsLast) {
  forward_list<int> fl;
  fl.emplace_front(1);
  fl.emplace_front(2);
  fl.emplace_front(3);
  EXPECT_EQ(fl.size(), 3);
  // Verify full iteration works (tests last_ is correct).
  auto it = fl.begin();
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(it, fl.end());
}

// =======================================================================
// emplace_after / insert_after
// =======================================================================

TEST(ForwardListModifiers, EmplaceAfterBegin) {
  forward_list<int> fl = {1, 3};
  auto it = fl.emplace_after(fl.before_begin(), 0);
  EXPECT_EQ(*it, 0);
  EXPECT_EQ(fl.size(), 3);
  auto check = fl.begin();
  EXPECT_EQ(*check++, 0);  // inserted before first
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 3);
}

TEST(ForwardListModifiers, EmplaceAfterMiddle) {
  forward_list<int> fl = {1, 3};
  auto it = fl.emplace_after(fl.begin(), 2);
  EXPECT_EQ(*it, 2);
  EXPECT_EQ(fl.size(), 3);
  auto check = fl.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 3);
}

TEST(ForwardListModifiers, EmplaceAfterEnd) {
  forward_list<int> fl = {1, 2};
  auto it = fl.emplace_after(std::next(fl.begin()), 3);
  EXPECT_EQ(*it, 3);
  EXPECT_EQ(fl.size(), 3);
  auto check = fl.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 3);
}

TEST(ForwardListModifiers, InsertAfterLvalue) {
  forward_list<int> fl = {1, 3};
  int value = 2;
  fl.insert_after(fl.begin(), value);
  EXPECT_EQ(fl.size(), 3);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
}

TEST(ForwardListModifiers, InsertAfterRvalue) {
  forward_list<int> fl = {1, 3};
  fl.insert_after(fl.begin(), 2);
  EXPECT_EQ(fl.size(), 3);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
}

TEST(ForwardListModifiers, InsertAfterCount) {
  forward_list<int> fl = {1, 4};
  auto it = fl.insert_after(fl.begin(), 2, 42);
  EXPECT_EQ(*it, 42);
  EXPECT_EQ(fl.size(), 4);
  auto check = fl.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 42);
  EXPECT_EQ(*check++, 42);
  EXPECT_EQ(*check++, 4);
}

TEST(ForwardListModifiers, InsertAfterCountAtTail) {
  forward_list<int> fl = {1, 2};
  auto tail = std::next(fl.begin());

  auto it = fl.insert_after(tail, 3, 7);

  EXPECT_EQ(*it, 7);
  EXPECT_EQ(fl.size(), 5);
  auto check = fl.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 2);
  EXPECT_EQ(*check++, 7);
  EXPECT_EQ(*check++, 7);
  EXPECT_EQ(*check++, 7);
  EXPECT_EQ(check, fl.end());
}

TEST(ForwardListModifiers, InsertAfterRange) {
  forward_list<int> fl = {1, 4};
  int arr[] = {2, 3};
  fl.insert_after(fl.begin(), arr, arr + 2);
  EXPECT_EQ(fl.size(), 4);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 4);
}

TEST(ForwardListModifiers, InsertAfterInitializerList) {
  forward_list<int> fl = {1, 4};
  fl.insert_after(fl.begin(), {2, 3});
  EXPECT_EQ(fl.size(), 4);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 4);
}

TEST(ForwardListModifiers, InsertAfterCountZero) {
  forward_list<int> fl = {1, 2};
  auto it = fl.insert_after(fl.begin(), 0, 99);
  EXPECT_EQ(*it, 1);  // returns pos unchanged when count == 0
  EXPECT_EQ(fl.size(), 2);
}

TEST(ForwardListModifiers, InsertAfterEmptyRange) {
  forward_list<int> fl = {1, 2};
  std::vector<int> empty;
  auto it = fl.insert_after(fl.begin(), empty.begin(), empty.end());
  EXPECT_EQ(*it, 1);  // returns pos when range is empty
  EXPECT_EQ(fl.size(), 2);
}

// =======================================================================
// emplace_back / push_back (chaistl extension)
// =======================================================================

TEST(ForwardListModifiers, EmplaceBack) {
  forward_list<int> fl = {1, 2};
  fl.emplace_back(3);
  EXPECT_EQ(fl.size(), 3);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
}

TEST(ForwardListModifiers, PushBackLvalue) {
  forward_list<int> fl;
  int x = 42;
  fl.push_back(x);
  EXPECT_EQ(fl.size(), 1);
  EXPECT_EQ(fl.front(), 42);
}

TEST(ForwardListModifiers, PushBackRvalue) {
  forward_list<int> fl;
  fl.push_back(42);
  EXPECT_EQ(fl.size(), 1);
  EXPECT_EQ(fl.front(), 42);
}

// =======================================================================
// erase_after
// =======================================================================

TEST(ForwardListModifiers, EraseAfterSingle) {
  forward_list<int> fl = {1, 2, 3};
  auto it = fl.erase_after(fl.begin());
  EXPECT_EQ(*it, 3);
  EXPECT_EQ(fl.size(), 2);
  auto check = fl.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 3);
}

TEST(ForwardListModifiers, EraseAfterBeforeBegin) {
  forward_list<int> fl = {1, 2, 3};
  auto it = fl.erase_after(fl.before_begin());
  EXPECT_EQ(*it, 2);
  EXPECT_EQ(fl.size(), 2);
  EXPECT_EQ(fl.front(), 2);
}

TEST(ForwardListModifiers, EraseAfterRange) {
  forward_list<int> fl = {1, 2, 3, 4, 5};
  // erase (1, 3] i.e. elements after first element through third element
  auto first = fl.begin();
  auto last = std::next(fl.begin(), 3);  // points to 3
  auto it = fl.erase_after(first, last);
  EXPECT_EQ(*it, 4);
  EXPECT_EQ(fl.size(), 3);
  auto check = fl.begin();
  EXPECT_EQ(*check++, 1);
  EXPECT_EQ(*check++, 4);
  EXPECT_EQ(*check++, 5);
}

TEST(ForwardListModifiers, EraseAfterRangeTail) {
  forward_list<int> fl = {1, 2, 3, 4, 5};
  // erase everything after 1
  auto it = fl.erase_after(fl.begin(), fl.end());
  EXPECT_EQ(it, fl.end());
  EXPECT_EQ(fl.size(), 1);
  EXPECT_EQ(fl.front(), 1);
}

TEST(ForwardListModifiers, EraseAfterEmptyRange) {
  forward_list<int> fl = {1, 2, 3};
  auto it = fl.erase_after(fl.begin(), std::next(fl.begin()));
  EXPECT_EQ(*it, 2);
  EXPECT_EQ(fl.size(), 3);
}

// =======================================================================
// assign
// =======================================================================

TEST(ForwardListModifiers, AssignRange) {
  forward_list<int> fl = {1, 2, 3};
  int arr[] = {4, 5, 6, 7};
  fl.assign(arr, arr + 4);
  EXPECT_EQ(fl.size(), 4);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 5);
  EXPECT_EQ(*it++, 6);
  EXPECT_EQ(*it++, 7);
}

TEST(ForwardListModifiers, AssignRangeShorter) {
  forward_list<int> fl = {1, 2, 3, 4, 5};
  int arr[] = {7, 8};
  fl.assign(arr, arr + 2);
  EXPECT_EQ(fl.size(), 2);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 7);
  EXPECT_EQ(*it++, 8);
}

TEST(ForwardListModifiers, AssignCount) {
  forward_list<int> fl = {1, 2, 3};
  fl.assign(2, 99);
  EXPECT_EQ(fl.size(), 2);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 99);
  EXPECT_EQ(*it++, 99);
}

TEST(ForwardListModifiers, AssignInitializerList) {
  forward_list<int> fl = {1, 2};
  fl.assign({3, 4, 5});
  EXPECT_EQ(fl.size(), 3);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 5);
}

TEST(ForwardListModifiers, AssignToEmpty) {
  forward_list<int> fl;
  fl.assign({1, 2, 3});
  EXPECT_EQ(fl.size(), 3);
}

TEST(ForwardListModifiers, AssignRangeToEmpty) {
  forward_list<int> fl;
  std::vector<int> v = {1, 2, 3};
  fl.assign(v.begin(), v.end());
  EXPECT_EQ(fl.size(), 3);
}

TEST(ForwardListModifiers, AssignRangeEmptyToNonEmpty) {
  forward_list<int> fl = {1, 2, 3, 4, 5};
  std::vector<int> v;
  fl.assign(v.begin(), v.end());
  EXPECT_TRUE(fl.empty());  // shrink-to-zero path
  EXPECT_EQ(fl.size(), 0);
}

TEST(ForwardListModifiers, AssignCountZeroToNonEmpty) {
  forward_list<int> fl = {1, 2, 3, 4, 5};
  fl.assign(0, 99);
  EXPECT_TRUE(fl.empty());  // shrink-to-zero path
  EXPECT_EQ(fl.size(), 0);
}

TEST(ForwardListModifiers, AssignRangeShorterToNonEmpty) {
  forward_list<int> fl = {1, 2, 3, 4, 5};
  std::vector<int> v = {7};
  fl.assign(v.begin(), v.end());
  EXPECT_EQ(fl.size(), 1);
  EXPECT_EQ(fl.front(), 7);
}

// =======================================================================
// swap
// =======================================================================

TEST(ForwardListModifiers, Swap) {
  forward_list<int> fl1 = {1, 2, 3};
  forward_list<int> fl2 = {4, 5};
  fl1.swap(fl2);
  EXPECT_EQ(fl1.size(), 2);
  EXPECT_EQ(fl1.front(), 4);
  EXPECT_EQ(fl2.size(), 3);
  EXPECT_EQ(fl2.front(), 1);
}

TEST(ForwardListModifiers, SwapEmpty) {
  forward_list<int> fl1 = {1, 2, 3};
  forward_list<int> fl2;
  fl1.swap(fl2);
  EXPECT_TRUE(fl1.empty());
  EXPECT_EQ(fl2.size(), 3);
}

TEST(ForwardListModifiers, NonMemberSwap) {
  forward_list<int> fl1 = {1, 2};
  forward_list<int> fl2 = {3, 4};
  swap(fl1, fl2);
  EXPECT_EQ(fl1.front(), 3);
  EXPECT_EQ(fl2.front(), 1);
}

// =======================================================================
// clear
// =======================================================================

TEST(ForwardListModifiers, Clear) {
  forward_list<int> fl = {1, 2, 3};
  fl.clear();
  EXPECT_TRUE(fl.empty());
  EXPECT_EQ(fl.size(), 0);
  EXPECT_EQ(fl.begin(), fl.end());
}

// =======================================================================
// append_range / prepend_range
// =======================================================================

TEST(ForwardListModifiers, AppendRange) {
  forward_list<int> fl = {1, 2};
  std::vector<int> v = {3, 4, 5};
  fl.append_range(v);
  EXPECT_EQ(fl.size(), 5);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 5);
}

TEST(ForwardListModifiers, PrependRange) {
  forward_list<int> fl = {3, 4, 5};
  std::vector<int> v = {1, 2};
  fl.prepend_range(v);
  EXPECT_EQ(fl.size(), 5);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 5);
}

// =======================================================================
// Move-only types
// =======================================================================

TEST(ForwardListModifiers, MoveOnlyPushFront) {
  forward_list<std::unique_ptr<int>> fl;
  fl.push_front(std::make_unique<int>(42));
  EXPECT_EQ(*fl.front(), 42);
}

TEST(ForwardListModifiers, MoveOnlyEmplaceFront) {
  forward_list<std::unique_ptr<int>> fl;
  fl.emplace_front(new int(42));
  EXPECT_EQ(*fl.front(), 42);
}

TEST(ForwardListModifiers, MoveOnlyInsertAfter) {
  forward_list<std::unique_ptr<int>> fl;
  fl.emplace_front(new int(1));
  fl.insert_after(fl.before_begin(), std::make_unique<int>(0));
  EXPECT_EQ(fl.size(), 2);
  auto it = fl.begin();
  EXPECT_EQ(**it++, 0);
  EXPECT_EQ(**it++, 1);
}

}  // namespace
