// SPDX-License-Identifier: Apache-2.0

// References:
// - std::set lookup:
//   https://en.cppreference.com/w/cpp/container/set
//   https://eel.is/c++draft/set.ops

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/set.hpp>

namespace {

using ::testing::ElementsAre;
using ::testing::Pair;

TEST(SetLookup, Find) {
  chaistl::set<int> s{1, 3, 5, 7, 9};

  auto it = s.find(5);
  EXPECT_NE(it, s.end());
  EXPECT_EQ(*it, 5);

  EXPECT_EQ(s.find(99), s.end());
}

TEST(SetLookup, Contains) {
  chaistl::set<int> s{1, 3, 5};

  EXPECT_TRUE(s.contains(1));
  EXPECT_TRUE(s.contains(3));
  EXPECT_TRUE(s.contains(5));
  EXPECT_FALSE(s.contains(2));
  EXPECT_FALSE(s.contains(99));
}

TEST(SetLookup, Count) {
  chaistl::set<int> s{1, 3, 5};

  EXPECT_EQ(s.count(1), 1uz);
  EXPECT_EQ(s.count(3), 1uz);
  EXPECT_EQ(s.count(99), 0uz);
}

TEST(SetLookup, LowerBound) {
  chaistl::set<int> s{1, 3, 5, 7};

  EXPECT_EQ(*s.lower_bound(0), 1);
  EXPECT_EQ(*s.lower_bound(1), 1);
  EXPECT_EQ(*s.lower_bound(2), 3);
  EXPECT_EQ(*s.lower_bound(3), 3);
  EXPECT_EQ(*s.lower_bound(4), 5);
  EXPECT_EQ(*s.lower_bound(7), 7);
  EXPECT_EQ(s.lower_bound(8), s.end());
  EXPECT_EQ(s.lower_bound(99), s.end());
}

TEST(SetLookup, UpperBound) {
  chaistl::set<int> s{1, 3, 5, 7};

  EXPECT_EQ(*s.upper_bound(0), 1);
  EXPECT_EQ(*s.upper_bound(1), 3);
  EXPECT_EQ(*s.upper_bound(2), 3);
  EXPECT_EQ(*s.upper_bound(3), 5);
  EXPECT_EQ(*s.upper_bound(6), 7);
  EXPECT_EQ(s.upper_bound(7), s.end());
  EXPECT_EQ(s.upper_bound(99), s.end());
}

TEST(SetLookup, EqualRange) {
  chaistl::set<int> s{1, 3, 5, 7};

  auto [first, last] = s.equal_range(3);
  EXPECT_EQ(*first, 3);
  EXPECT_EQ(*last, 5);

  // Non-existent key: lower_bound == upper_bound
  auto [f2, l2] = s.equal_range(4);
  EXPECT_EQ(*f2, 5);
  EXPECT_EQ(f2, l2);
}

// Regression: when the match has no right subtree, its successor is an
// ancestor. The upper bound must be that ancestor, not end().
TEST(SetLookup, EqualRangeSuccessorIsAncestor) {
  chaistl::set<int> s;
  s.insert(2);  // root
  s.insert(1);  // left leaf: successor of 1 is the root
  s.insert(3);

  auto [first, last] = s.equal_range(1);
  EXPECT_EQ(*first, 1);
  ASSERT_NE(last, s.end());
  EXPECT_EQ(*last, 2);
  EXPECT_EQ(std::distance(first, last), 1);
}

// Regression: --end() / rbegin() must yield the maximum. The header's
// left/right cache the extremes, so decrement must not treat the header
// like an ordinary node.
TEST(SetLookup, DecrementFromEnd) {
  chaistl::set<int> s{2, 1, 3};

  EXPECT_EQ(*s.rbegin(), 3);

  auto it = s.end();
  --it;
  EXPECT_EQ(*it, 3);
  --it;
  EXPECT_EQ(*it, 2);
  --it;
  EXPECT_EQ(*it, 1);
  EXPECT_EQ(it, s.begin());
}

// Regression: increment from the maximum walks up through the header;
// trees whose root is also the maximum used to loop instead of reaching
// end().
TEST(SetLookup, IncrementReachesEnd) {
  chaistl::set<int> single{1};
  auto it = single.begin();
  ++it;
  EXPECT_EQ(it, single.end());
  EXPECT_EQ(std::distance(single.begin(), single.end()), 1);

  chaistl::set<int> root_is_max{2, 1};  // 2 is root and maximum
  EXPECT_THAT(root_is_max, ElementsAre(1, 2));
}

// Const overloads must compile and agree with the non-const ones.
TEST(SetLookup, ConstLookup) {
  const chaistl::set<int> s{1, 3, 5};

  EXPECT_EQ(*s.find(3), 3);
  EXPECT_EQ(s.find(2), s.end());
  EXPECT_TRUE(s.contains(5));
  EXPECT_EQ(s.count(1), 1uz);
  EXPECT_EQ(*s.lower_bound(2), 3);
  EXPECT_EQ(*s.upper_bound(3), 5);

  auto [first, last] = s.equal_range(3);
  EXPECT_EQ(*first, 3);
  EXPECT_EQ(*last, 5);
}

TEST(SetLookup, IterationOrder) {
  chaistl::set<int> s{5, 2, 8, 1, 3, 7, 9};

  // Forward iteration should be sorted
  EXPECT_THAT(s, ElementsAre(1, 2, 3, 5, 7, 8, 9));

  // Reverse iteration should be reverse sorted
  std::vector<int> reverse;
  for (auto it = s.rbegin(); it != s.rend(); ++it) {
    reverse.push_back(*it);
  }
  EXPECT_THAT(reverse, ElementsAre(9, 8, 7, 5, 3, 2, 1));
}

}  // namespace
