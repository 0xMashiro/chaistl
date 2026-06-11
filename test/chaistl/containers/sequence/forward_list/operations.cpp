// SPDX-License-Identifier: Apache-2.0

// References:
// - std::forward_list operations:
//   https://en.cppreference.com/w/cpp/container/forward_list
//   https://eel.is/c++draft/forwardlist.ops

#include <gtest/gtest.h>

#include <chaistl/containers/forward_list.hpp>

namespace {

using chaistl::forward_list;

// =======================================================================
// splice_after
// =======================================================================

TEST(ForwardListOperations, SpliceAfterAll) {
  forward_list<int> fl1 = {1, 2, 3};
  forward_list<int> fl2 = {4, 5, 6};
  fl1.splice_after(fl1.cbefore_begin(), fl2);
  EXPECT_EQ(fl1.size(), 6);
  EXPECT_TRUE(fl2.empty());
  auto it = fl1.begin();
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 5);
  EXPECT_EQ(*it++, 6);
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
}

TEST(ForwardListOperations, SpliceAfterAllRvalue) {
  forward_list<int> fl1 = {1, 2};
  forward_list<int> fl2 = {3, 4};
  fl1.splice_after(fl1.cbefore_begin(), std::move(fl2));
  EXPECT_EQ(fl1.size(), 4);
  EXPECT_TRUE(fl2.empty());
}

TEST(ForwardListOperations, SpliceAfterSingleElement) {
  forward_list<int> fl1 = {1, 2, 3};
  forward_list<int> fl2 = {4, 5, 6};
  // Move element after fl2.begin() (i.e. 5) to after fl1.begin().
  fl1.splice_after(fl1.begin(), fl2, fl2.begin());
  EXPECT_EQ(fl1.size(), 4);
  EXPECT_EQ(fl2.size(), 2);
  auto it = fl1.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 5);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
}

TEST(ForwardListOperations, SpliceAfterRange) {
  forward_list<int> fl1 = {1, 6};
  forward_list<int> fl2 = {2, 3, 4, 5, 7};
  // Move [fl2.begin()+1, fl2.begin()+4) i.e. 3,4,5 to after fl1.begin()
  auto first = fl2.begin();               // points to 2
  auto last = std::next(fl2.begin(), 4);  // points to 5
  fl1.splice_after(fl1.begin(), fl2, first, last);
  EXPECT_EQ(fl1.size(), 5);
  EXPECT_EQ(fl2.size(), 2);
  auto it1 = fl1.begin();
  EXPECT_EQ(*it1++, 1);
  EXPECT_EQ(*it1++, 3);
  EXPECT_EQ(*it1++, 4);
  EXPECT_EQ(*it1++, 5);
  EXPECT_EQ(*it1++, 6);
  // fl2 should have {2, 7}
  auto it2 = fl2.begin();
  EXPECT_EQ(*it2++, 2);
  EXPECT_EQ(*it2++, 7);
}

TEST(ForwardListOperations, SpliceAfterEmpty) {
  forward_list<int> fl1 = {1, 2, 3};
  forward_list<int> fl2;
  fl1.splice_after(fl1.cbefore_begin(), fl2);
  EXPECT_EQ(fl1.size(), 3);
}

// =======================================================================
// remove / remove_if
// =======================================================================

TEST(ForwardListOperations, Remove) {
  forward_list<int> fl = {1, 2, 3, 2, 4, 2};
  auto removed = fl.remove(2);
  EXPECT_EQ(removed, 3);
  EXPECT_EQ(fl.size(), 3);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 4);
}

TEST(ForwardListOperations, RemoveAll) {
  forward_list<int> fl = {5, 5, 5};
  auto removed = fl.remove(5);
  EXPECT_EQ(removed, 3);
  EXPECT_TRUE(fl.empty());
}

TEST(ForwardListOperations, RemoveNone) {
  forward_list<int> fl = {1, 2, 3};
  auto removed = fl.remove(99);
  EXPECT_EQ(removed, 0);
  EXPECT_EQ(fl.size(), 3);
}

TEST(ForwardListOperations, RemoveIf) {
  forward_list<int> fl = {1, 2, 3, 4, 5, 6};
  auto removed = fl.remove_if([](int x) {
    return x % 2 == 0;
  });
  EXPECT_EQ(removed, 3);
  EXPECT_EQ(fl.size(), 3);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 5);
}

TEST(ForwardListOperations, RemoveFromSingleElement) {
  forward_list<int> fl = {42};
  auto removed = fl.remove(42);
  EXPECT_EQ(removed, 1);
  EXPECT_TRUE(fl.empty());
}

// =======================================================================
// unique
// =======================================================================

TEST(ForwardListOperations, Unique) {
  forward_list<int> fl = {1, 1, 2, 2, 2, 3, 3, 1, 1};
  auto removed = fl.unique();
  EXPECT_EQ(removed, 5);
  EXPECT_EQ(fl.size(), 4);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 1);
}

TEST(ForwardListOperations, UniqueEmpty) {
  forward_list<int> fl;
  auto removed = fl.unique();
  EXPECT_EQ(removed, 0);
}

TEST(ForwardListOperations, UniqueSingle) {
  forward_list<int> fl = {42};
  auto removed = fl.unique();
  EXPECT_EQ(removed, 0);
  EXPECT_EQ(fl.size(), 1);
}

TEST(ForwardListOperations, UniqueBinaryPred) {
  forward_list<int> fl = {1, 2, 4, 5, 7, 8};
  // predicate b - a <= 1 identifies consecutive integers.
  // Removes 2 (consecutive with 1), 5 (consecutive with 4), 8 (consecutive with 7).
  auto removed = fl.unique([](int a, int b) {
    return b - a <= 1;
  });
  EXPECT_EQ(removed, 3);
  EXPECT_EQ(fl.size(), 3);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 7);
  EXPECT_EQ(it, fl.end());
}

// =======================================================================
// merge
// =======================================================================

TEST(ForwardListOperations, Merge) {
  forward_list<int> fl1 = {1, 3, 5};
  forward_list<int> fl2 = {2, 4, 6};
  fl1.merge(fl2);
  EXPECT_EQ(fl1.size(), 6);
  EXPECT_TRUE(fl2.empty());
  auto it = fl1.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 5);
  EXPECT_EQ(*it++, 6);
}

TEST(ForwardListOperations, MergeRvalue) {
  forward_list<int> fl1 = {1, 3, 5};
  fl1.merge(forward_list<int>{2, 4, 6});
  EXPECT_EQ(fl1.size(), 6);
}

TEST(ForwardListOperations, MergeIntoEmpty) {
  forward_list<int> fl1;
  forward_list<int> fl2 = {1, 2, 3};
  fl1.merge(fl2);
  EXPECT_EQ(fl1.size(), 3);
  EXPECT_TRUE(fl2.empty());
  auto it = fl1.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
}

TEST(ForwardListOperations, MergeEmpty) {
  forward_list<int> fl1 = {1, 2, 3};
  forward_list<int> fl2;
  fl1.merge(fl2);
  EXPECT_EQ(fl1.size(), 3);
}

TEST(ForwardListOperations, MergeSelf) {
  forward_list<int> fl = {1, 2, 3};
  fl.merge(fl);
  EXPECT_EQ(fl.size(), 3);
}

TEST(ForwardListOperations, MergeWithCompare) {
  forward_list<int> fl1 = {5, 3, 1};
  forward_list<int> fl2 = {6, 4, 2};
  fl1.merge(fl2, std::greater<>{});
  EXPECT_EQ(fl1.size(), 6);
  auto it = fl1.begin();
  EXPECT_EQ(*it++, 6);
  EXPECT_EQ(*it++, 5);
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 1);
}

// =======================================================================
// sort
// =======================================================================

TEST(ForwardListOperations, Sort) {
  forward_list<int> fl = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5};
  fl.sort();
  EXPECT_TRUE(std::is_sorted(fl.begin(), fl.end()));
  EXPECT_EQ(fl.size(), 11);
}

TEST(ForwardListOperations, SortAlreadySorted) {
  forward_list<int> fl = {1, 2, 3, 4, 5};
  fl.sort();
  EXPECT_TRUE(std::is_sorted(fl.begin(), fl.end()));
}

TEST(ForwardListOperations, SortReverse) {
  forward_list<int> fl = {5, 4, 3, 2, 1};
  fl.sort();
  EXPECT_TRUE(std::is_sorted(fl.begin(), fl.end()));
}

TEST(ForwardListOperations, SortSingleElement) {
  forward_list<int> fl = {42};
  fl.sort();
  EXPECT_EQ(fl.front(), 42);
}

TEST(ForwardListOperations, SortEmpty) {
  forward_list<int> fl;
  fl.sort();
  EXPECT_TRUE(fl.empty());
}

TEST(ForwardListOperations, SortWithCompare) {
  forward_list<int> fl = {3, 1, 4, 1, 5};
  fl.sort(std::greater<>{});
  EXPECT_TRUE(std::is_sorted(fl.begin(), fl.end(), std::greater<>{}));
}

TEST(ForwardListOperations, SortStability) {
  // Use pairs where first element is value, second is original index.
  forward_list<std::pair<int, int>> fl;
  fl.emplace_back(3, 0);
  fl.emplace_back(2, 1);
  fl.emplace_back(3, 2);
  fl.emplace_back(1, 3);
  fl.sort([](const auto& a, const auto& b) {
    return a.first < b.first;
  });
  // Check that equal keys preserve insertion order.
  auto it = fl.begin();
  EXPECT_EQ((*it++), (std::pair(1, 3)));
  EXPECT_EQ((*it++), (std::pair(2, 1)));
  EXPECT_EQ((*it++), (std::pair(3, 0)));  // 3,0 before 3,2
  EXPECT_EQ((*it++), (std::pair(3, 2)));
}

// =======================================================================
// reverse
// =======================================================================

TEST(ForwardListOperations, Reverse) {
  forward_list<int> fl = {1, 2, 3, 4, 5};
  fl.reverse();
  EXPECT_EQ(fl.size(), 5);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 5);
  EXPECT_EQ(*it++, 4);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 1);
}

TEST(ForwardListOperations, ReverseEmpty) {
  forward_list<int> fl;
  fl.reverse();
  EXPECT_TRUE(fl.empty());
}

TEST(ForwardListOperations, ReverseSingle) {
  forward_list<int> fl = {42};
  fl.reverse();
  EXPECT_EQ(fl.front(), 42);
}

TEST(ForwardListOperations, ReverseTwoElements) {
  forward_list<int> fl = {1, 2};
  fl.reverse();
  auto it = fl.begin();
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 1);
}

// =======================================================================
// Erase / erase_if (non-member)
// =======================================================================

TEST(ForwardListOperations, NonMemberErase) {
  forward_list<int> fl = {1, 2, 3, 2, 4};
  auto count = chaistl::erase(fl, 2);
  EXPECT_EQ(count, 2);
  EXPECT_EQ(fl.size(), 3);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 3);
  EXPECT_EQ(*it++, 4);
}

TEST(ForwardListOperations, NonMemberEraseIf) {
  forward_list<int> fl = {1, 2, 3, 4, 5, 6};
  auto count = chaistl::erase_if(fl, [](int x) {
    return x > 3;
  });
  EXPECT_EQ(count, 3);
  EXPECT_EQ(fl.size(), 3);
  auto it = fl.begin();
  EXPECT_EQ(*it++, 1);
  EXPECT_EQ(*it++, 2);
  EXPECT_EQ(*it++, 3);
}

}  // namespace
