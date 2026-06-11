// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/skip_list_multiset.hpp>

#include <algorithm>
#include <functional>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace {

static_assert(chaistl::skip_list_multiset<int>{}.empty());

template <class Set>
std::vector<int> to_vector(const Set& set) {
  std::vector<int> out;
  for (int value : set) {
    out.push_back(value);
  }
  return out;
}

TEST(SkipListMultisetTest, InsertsEquivalentKeysAfterExistingRun) {
  chaistl::skip_list_multiset<int> set;

  EXPECT_EQ(*set.insert(3), 3);
  EXPECT_EQ(*set.insert(1), 1);
  EXPECT_EQ(*set.insert(3), 3);
  EXPECT_EQ(*set.insert(2), 2);
  EXPECT_EQ(*set.insert(3), 3);

  EXPECT_EQ(set.size(), 5);
  EXPECT_EQ(to_vector(set), (std::vector<int>{1, 2, 3, 3, 3}));
}

TEST(SkipListMultisetTest, LookupBoundsAndEqualRangeCoverEquivalentRun) {
  chaistl::skip_list_multiset<int> set{1, 3, 5, 3, 7, 3};

  EXPECT_TRUE(set.contains(3));
  EXPECT_FALSE(set.contains(4));
  EXPECT_EQ(*set.find(3), 3);
  EXPECT_EQ(set.find(6), set.end());
  EXPECT_EQ(set.count(3), 3);
  EXPECT_EQ(set.count(4), 0);
  EXPECT_EQ(*set.lower_bound(4), 5);
  EXPECT_EQ(*set.upper_bound(3), 5);

  const auto [first, last] = set.equal_range(3);
  EXPECT_EQ((std::vector<int>(first, last)), (std::vector<int>{3, 3, 3}));
}

TEST(SkipListMultisetTest, EraseRemovesPositionRangeAndAllEquivalentKeys) {
  chaistl::skip_list_multiset<int> set{1, 2, 2, 3, 3, 3, 4};

  EXPECT_EQ(set.erase(2), 2);
  EXPECT_EQ(to_vector(set), (std::vector<int>{1, 3, 3, 3, 4}));

  auto next = set.erase(set.find(3));
  EXPECT_EQ(*next, 3);
  EXPECT_EQ(to_vector(set), (std::vector<int>{1, 3, 3, 4}));

  set.erase(set.lower_bound(3), set.end());
  EXPECT_EQ(to_vector(set), (std::vector<int>{1}));
}

TEST(SkipListMultisetTest, EraseCanRemoveIteratorInsideEquivalentRun) {
  chaistl::skip_list_multiset<int> set{1, 3, 3, 3, 5};

  auto it = set.lower_bound(3);
  ++it;
  auto next = set.erase(it);

  EXPECT_EQ(*next, 3);
  EXPECT_EQ(set.count(3), 2);
  EXPECT_EQ(to_vector(set), (std::vector<int>{1, 3, 3, 5}));
}

TEST(SkipListMultisetTest, ComparatorCanInvertOrder) {
  chaistl::skip_list_multiset<int, std::greater<int>> set{1, 4, 2, 3, 3};

  EXPECT_EQ(to_vector(set), (std::vector<int>{4, 3, 3, 2, 1}));
  EXPECT_EQ(*set.lower_bound(3), 3);
  EXPECT_EQ(*set.upper_bound(3), 2);
}

TEST(SkipListMultisetTest, CopyMoveAndClear) {
  chaistl::skip_list_multiset<std::string> set{"b", "a", "b", "c"};
  chaistl::skip_list_multiset<std::string> copy = set;
  chaistl::skip_list_multiset<std::string> moved = std::move(copy);

  EXPECT_EQ((std::vector<std::string>(moved.begin(), moved.end())), (std::vector<std::string>{"a", "b", "b", "c"}));

  moved.clear();
  EXPECT_TRUE(moved.empty());
}

TEST(SkipListMultisetTest, MatchesStdMultisetUnderRandomOperations) {
  std::mt19937 rng(20260612u);
  std::uniform_int_distribution<int> value_dist(-40, 40);
  std::uniform_int_distribution<int> op_dist(0, 3);

  chaistl::skip_list_multiset<int> set;
  std::multiset<int> oracle;

  for (int step = 0; step < 2000; ++step) {
    const int value = value_dist(rng);
    switch (op_dist(rng)) {
      case 0:
      case 1:
        set.insert(value);
        oracle.insert(value);
        break;
      case 2:
        EXPECT_EQ(set.erase(value), oracle.erase(value));
        break;
      default:
        EXPECT_EQ(set.contains(value), oracle.contains(value));
        EXPECT_EQ(set.count(value), oracle.count(value));
        break;
    }

    ASSERT_EQ(set.size(), oracle.size());
    EXPECT_TRUE(std::ranges::equal(set, oracle));
  }
}

}  // namespace
