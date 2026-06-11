// SPDX-License-Identifier: Apache-2.0

// References:
// - std::multiset lookup:
//   https://en.cppreference.com/w/cpp/container/multiset
//   https://eel.is/c++draft/associative.reqmts (count/find/equal_range
//   semantics under equivalent keys)

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/multiset.hpp>

#include <iterator>

namespace {

TEST(MultisetLookup, CountAcrossRuns) {
  chaistl::multiset<int> ms{1, 2, 2, 3, 3, 3};

  EXPECT_EQ(ms.count(0), 0uz);
  EXPECT_EQ(ms.count(1), 1uz);
  EXPECT_EQ(ms.count(2), 2uz);
  EXPECT_EQ(ms.count(3), 3uz);
  EXPECT_EQ(ms.count(4), 0uz);
}

TEST(MultisetLookup, FindReturnsFirstOfRun) {
  chaistl::multiset<int> ms{1, 2, 2, 2, 3};

  auto it = ms.find(2);
  ASSERT_NE(it, ms.end());
  EXPECT_EQ(*it, 2);
  // The first of the run: its predecessor is 1.
  EXPECT_EQ(*std::prev(it), 1);

  EXPECT_EQ(ms.find(42), ms.end());
}

TEST(MultisetLookup, EqualRangeSpansRun) {
  chaistl::multiset<int> ms{1, 2, 2, 2, 3};

  auto [first, last] = ms.equal_range(2);
  EXPECT_EQ(std::distance(first, last), 3);
  EXPECT_EQ(*first, 2);
  EXPECT_EQ(*last, 3);

  // Missing key: empty range positioned at the would-be location.
  auto [mfirst, mlast] = ms.equal_range(0);
  EXPECT_EQ(mfirst, mlast);
  EXPECT_EQ(mfirst, ms.begin());

  auto [efirst, elast] = ms.equal_range(99);
  EXPECT_EQ(efirst, elast);
  EXPECT_EQ(efirst, ms.end());
}

TEST(MultisetLookup, LowerUpperBoundBracketRun) {
  chaistl::multiset<int> ms{1, 2, 2, 4};

  EXPECT_EQ(*ms.lower_bound(2), 2);
  EXPECT_EQ(*ms.upper_bound(2), 4);
  EXPECT_EQ(std::distance(ms.lower_bound(2), ms.upper_bound(2)), 2);

  // Between runs.
  EXPECT_EQ(*ms.lower_bound(3), 4);
  EXPECT_EQ(*ms.upper_bound(3), 4);
}

TEST(MultisetLookup, Contains) {
  chaistl::multiset<int> ms{5, 5};
  EXPECT_TRUE(ms.contains(5));
  EXPECT_FALSE(ms.contains(4));
}

TEST(MultisetLookup, ConstOverloads) {
  const chaistl::multiset<int> ms{1, 1, 2};

  EXPECT_EQ(ms.count(1), 2uz);
  EXPECT_NE(ms.find(1), ms.end());
  auto [first, last] = ms.equal_range(1);
  EXPECT_EQ(std::distance(first, last), 2);
  EXPECT_EQ(*ms.lower_bound(1), 1);
  EXPECT_EQ(*ms.upper_bound(1), 2);
}

}  // namespace
