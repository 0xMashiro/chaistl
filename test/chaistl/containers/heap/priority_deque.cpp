// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/priority_deque.hpp>

#include <algorithm>
#include <functional>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace {

static_assert(chaistl::priority_deque<int>{}.empty());

struct Flip {
  bool min_first = true;
  constexpr bool operator()(int lhs, int rhs) const noexcept { return min_first ? lhs < rhs : lhs > rhs; }
};

template <class Queue>
std::vector<int> drain_min(Queue queue) {
  std::vector<int> values;
  while (!queue.empty()) {
    values.push_back(queue.min());
    queue.pop_min();
  }
  return values;
}

template <class Queue>
std::vector<int> drain_max(Queue queue) {
  std::vector<int> values;
  while (!queue.empty()) {
    values.push_back(queue.max());
    queue.pop_max();
  }
  return values;
}

TEST(PriorityDequeTest, PushTracksBothEnds) {
  chaistl::priority_deque<int> queue;

  for (int value : {4, 1, 7, 3, 9, 2}) {
    queue.push(value);
    EXPECT_TRUE(queue.verify());
  }

  EXPECT_EQ(queue.size(), 6);
  EXPECT_EQ(queue.min(), 1);
  EXPECT_EQ(queue.max(), 9);
}

TEST(PriorityDequeTest, InitializerListBuildsValidMinMaxHeap) {
  chaistl::priority_deque<int> queue{8, 5, 1, 9, 2, 7, 3};

  EXPECT_TRUE(queue.verify());
  EXPECT_EQ(drain_min(queue), (std::vector<int>{1, 2, 3, 5, 7, 8, 9}));
  EXPECT_EQ(drain_max(queue), (std::vector<int>{9, 8, 7, 5, 3, 2, 1}));
}

TEST(PriorityDequeTest, PopMinAndPopMaxCanInterleave) {
  chaistl::priority_deque<int> queue{6, 2, 8, 1, 9, 3, 7, 4, 5};

  EXPECT_EQ(queue.min(), 1);
  EXPECT_TRUE(queue.verify());
  queue.pop_min();
  EXPECT_TRUE(queue.verify());
  EXPECT_EQ(queue.max(), 9);
  queue.pop_max();
  EXPECT_TRUE(queue.verify());
  EXPECT_EQ(queue.min(), 2);
  EXPECT_EQ(queue.max(), 8);
  queue.pop_min();
  EXPECT_TRUE(queue.verify());
  queue.pop_max();
  EXPECT_TRUE(queue.verify());

  EXPECT_EQ(drain_min(queue), (std::vector<int>{3, 4, 5, 6, 7}));
}

TEST(PriorityDequeTest, ComparatorDefinesBothEnds) {
  chaistl::priority_deque<int, Flip> queue({1, 2, 3, 4}, Flip{false});

  EXPECT_TRUE(queue.verify());
  EXPECT_EQ(queue.min(), 4);
  EXPECT_EQ(queue.max(), 1);
}

TEST(PriorityDequeTest, EmplaceAndClear) {
  chaistl::priority_deque<std::string> queue;

  queue.emplace(3, 'c');
  EXPECT_TRUE(queue.verify());
  queue.emplace(1, 'a');
  EXPECT_TRUE(queue.verify());
  queue.emplace(2, 'b');
  EXPECT_TRUE(queue.verify());

  EXPECT_EQ(queue.min(), "a");
  EXPECT_EQ(queue.max(), "ccc");

  queue.clear();
  EXPECT_TRUE(queue.empty());
}

TEST(PriorityDequeTest, MatchesMultisetUnderRandomOperations) {
  std::mt19937 rng(20260612u);
  std::uniform_int_distribution<int> value_dist(-1000, 1000);
  std::uniform_int_distribution<int> op_dist(0, 5);

  chaistl::priority_deque<int> queue;
  std::multiset<int> oracle;

  for (int step = 0; step < 1000; ++step) {
    if (oracle.empty() || op_dist(rng) < 3) {
      const int value = value_dist(rng);
      queue.push(value);
      oracle.insert(value);
    } else if (op_dist(rng) % 2 == 0) {
      EXPECT_EQ(queue.min(), *oracle.begin());
      queue.pop_min();
      oracle.erase(oracle.begin());
      EXPECT_TRUE(queue.verify());
    } else {
      EXPECT_EQ(queue.max(), *oracle.rbegin());
      queue.pop_max();
      oracle.erase(std::prev(oracle.end()));
      EXPECT_TRUE(queue.verify());
    }

    ASSERT_EQ(queue.size(), oracle.size());
    EXPECT_TRUE(queue.verify());
    if (!oracle.empty()) {
      EXPECT_EQ(queue.min(), *oracle.begin());
      EXPECT_EQ(queue.max(), *oracle.rbegin());
    }
  }
}

}  // namespace
