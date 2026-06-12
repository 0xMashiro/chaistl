// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/skip_list_set.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

namespace {

static_assert(chaistl::skip_list_set<int>{}.empty());

template <class Set>
std::vector<int> to_vector(const Set& set) {
  std::vector<int> out;
  for (int value : set) {
    out.push_back(value);
  }
  return out;
}

struct TrackingAllocatorStats {
  std::size_t allocate_calls = 0;
  std::size_t deallocate_calls = 0;
  std::size_t allocated_objects = 0;
  std::size_t deallocated_objects = 0;
  std::size_t link_allocations = 0;
  std::size_t links_height_at_least_2 = 0;
};

template <class T>
struct TrackingAllocator {
  using value_type = T;

  std::shared_ptr<TrackingAllocatorStats> stats;

  TrackingAllocator() : stats(std::make_shared<TrackingAllocatorStats>()) {}
  explicit TrackingAllocator(std::shared_ptr<TrackingAllocatorStats> init) : stats(std::move(init)) {}

  template <class U>
  TrackingAllocator(const TrackingAllocator<U>& other) : stats(other.stats) {}

  [[nodiscard]] T* allocate(std::size_t count) {
    ++stats->allocate_calls;
    stats->allocated_objects += count;
    if constexpr (std::is_pointer_v<T>) {
      ++stats->link_allocations;
      if (count >= 2) {
        ++stats->links_height_at_least_2;
      }
    }
    return std::allocator<T>{}.allocate(count);
  }

  void deallocate(T* pointer, std::size_t count) noexcept {
    ++stats->deallocate_calls;
    stats->deallocated_objects += count;
    std::allocator<T>{}.deallocate(pointer, count);
  }

  template <class U>
  [[nodiscard]] bool operator==(const TrackingAllocator<U>& other) const noexcept {
    return stats == other.stats;
  }
};

struct DirectionalCompare {
  bool descending = false;

  constexpr bool operator()(int lhs, int rhs) const noexcept { return descending ? lhs > rhs : lhs < rhs; }
};

struct ThrowingMoveCompare {
  ThrowingMoveCompare() = default;
  ThrowingMoveCompare(const ThrowingMoveCompare&) = default;
  ThrowingMoveCompare(ThrowingMoveCompare&&) noexcept(false) {}

  constexpr bool operator()(int lhs, int rhs) const noexcept { return lhs < rhs; }
};

static_assert(!std::is_nothrow_move_constructible_v<chaistl::skip_list_set<int, ThrowingMoveCompare>>);

TEST(SkipListSetTest, InsertsUniqueKeysInOrder) {
  chaistl::skip_list_set<int> set;

  EXPECT_TRUE(set.insert(3).second);
  EXPECT_TRUE(set.insert(1).second);
  EXPECT_TRUE(set.insert(2).second);
  EXPECT_FALSE(set.insert(2).second);

  EXPECT_EQ(set.size(), 3);
  EXPECT_EQ(to_vector(set), (std::vector<int>{1, 2, 3}));
}

TEST(SkipListSetTest, LookupAndBoundsUseComparatorEquivalence) {
  chaistl::skip_list_set<int> set{1, 3, 5, 7};

  EXPECT_TRUE(set.contains(3));
  EXPECT_FALSE(set.contains(4));
  EXPECT_EQ(*set.find(5), 5);
  EXPECT_EQ(set.find(6), set.end());
  EXPECT_EQ(*set.lower_bound(4), 5);
  EXPECT_EQ(*set.upper_bound(5), 7);
  EXPECT_EQ(set.count(3), 1);
  EXPECT_EQ(set.count(4), 0);
}

TEST(SkipListSetTest, EraseByKeyPositionAndRange) {
  chaistl::skip_list_set<int> set{1, 2, 3, 4, 5, 6};

  EXPECT_EQ(set.erase(3), 1);
  EXPECT_EQ(set.erase(3), 0);
  EXPECT_EQ(to_vector(set), (std::vector<int>{1, 2, 4, 5, 6}));

  auto next = set.erase(set.find(2));
  EXPECT_EQ(*next, 4);

  set.erase(set.lower_bound(4), set.end());
  EXPECT_EQ(to_vector(set), (std::vector<int>{1}));
}

TEST(SkipListSetTest, ComparatorCanInvertOrder) {
  chaistl::skip_list_set<int, std::greater<int>> set{1, 4, 2, 3};

  EXPECT_EQ(to_vector(set), (std::vector<int>{4, 3, 2, 1}));
  EXPECT_EQ(*set.lower_bound(3), 3);
}

TEST(SkipListSetTest, CopyMoveAndClear) {
  chaistl::skip_list_set<std::string> set{"b", "a", "c"};
  chaistl::skip_list_set<std::string> copy = set;
  chaistl::skip_list_set<std::string> moved = std::move(copy);

  EXPECT_EQ((std::vector<std::string>(moved.begin(), moved.end())), (std::vector<std::string>{"a", "b", "c"}));

  moved.clear();
  EXPECT_TRUE(moved.empty());
}

TEST(SkipListSetTest, MoveConstructWithAllocatorPreservesComparator) {
  chaistl::skip_list_set<int, DirectionalCompare> set(DirectionalCompare{.descending = true});
  set.insert(1);
  set.insert(3);
  set.insert(2);

  chaistl::skip_list_set<int, DirectionalCompare> moved(std::move(set), set.get_allocator());

  EXPECT_EQ(to_vector(moved), (std::vector<int>{3, 2, 1}));
}

TEST(SkipListSetTest, MoveAssignWithUnequalNonPropagatingAllocatorKeepsTargetAllocator) {
  auto source_stats = std::make_shared<TrackingAllocatorStats>();
  auto target_stats = std::make_shared<TrackingAllocatorStats>();

  chaistl::skip_list_set<int, std::less<int>, TrackingAllocator<int>> source(std::less<int>{},
                                                                             TrackingAllocator<int>{source_stats});
  chaistl::skip_list_set<int, std::less<int>, TrackingAllocator<int>> target(std::less<int>{},
                                                                             TrackingAllocator<int>{target_stats});
  source.insert(1);
  source.insert(3);
  source.insert(2);
  target.insert(9);

  target = std::move(source);

  EXPECT_EQ(target.get_allocator().stats, target_stats);
  EXPECT_EQ(to_vector(target), (std::vector<int>{1, 2, 3}));
  EXPECT_EQ(to_vector(source), (std::vector<int>{1, 2, 3}));
}

TEST(SkipListSetTest, MatchesStdSetUnderRandomOperations) {
  std::mt19937 rng(20260612u);
  std::uniform_int_distribution<int> value_dist(-200, 200);
  std::uniform_int_distribution<int> op_dist(0, 3);

  chaistl::skip_list_set<int> set;
  std::set<int> oracle;

  for (int step = 0; step < 2000; ++step) {
    const int value = value_dist(rng);
    switch (op_dist(rng)) {
      case 0:
      case 1: {
        const auto inserted = set.insert(value).second;
        const auto expected = oracle.insert(value).second;
        EXPECT_EQ(inserted, expected);
        break;
      }
      case 2:
        EXPECT_EQ(set.erase(value), oracle.erase(value));
        break;
      default:
        EXPECT_EQ(set.contains(value), oracle.contains(value));
        break;
    }

    ASSERT_EQ(set.size(), oracle.size());
    EXPECT_TRUE(std::ranges::equal(set, oracle));
  }
}

TEST(SkipListSetTest, RandomHeightDistributionIsGeometric) {
  auto stats = std::make_shared<TrackingAllocatorStats>();
  chaistl::skip_list_set<int, std::less<int>, TrackingAllocator<int>> set(std::less<int>{},
                                                                          TrackingAllocator<int>{stats});

  std::vector<int> values(10000);
  for (int i = 0; i < 10000; ++i) {
    values[static_cast<std::size_t>(i)] = i;
  }
  std::mt19937 rng(20260612u);
  std::ranges::shuffle(values, rng);

  for (int value : values) {
    set.insert(value);
  }

  ASSERT_EQ(stats->link_allocations, set.size());
  const double fraction =
      static_cast<double>(stats->links_height_at_least_2) / static_cast<double>(stats->link_allocations);
  EXPECT_GE(fraction, 0.4);
  EXPECT_LE(fraction, 0.6);
}

TEST(SkipListSetTest, StatefulAllocatorPairsAllocationsAndDeallocations) {
  auto stats = std::make_shared<TrackingAllocatorStats>();
  {
    chaistl::skip_list_set<int, std::less<int>, TrackingAllocator<int>> set(std::less<int>{},
                                                                            TrackingAllocator<int>{stats});
    for (int i = 0; i < 256; ++i) {
      set.insert(i);
    }
    for (int i = 0; i < 128; ++i) {
      set.erase(i);
    }
  }

  EXPECT_EQ(stats->allocate_calls, stats->deallocate_calls);
  EXPECT_EQ(stats->allocated_objects, stats->deallocated_objects);
}

}  // namespace
