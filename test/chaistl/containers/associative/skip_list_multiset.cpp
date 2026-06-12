// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/skip_list_multiset.hpp>

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

static_assert(chaistl::skip_list_multiset<int>{}.empty());

template <class Set>
std::vector<int> to_vector(const Set& set) {
  std::vector<int> out;
  for (int value : set) {
    out.push_back(value);
  }
  return out;
}

struct DirectionalCompare {
  bool descending = false;

  constexpr bool operator()(int lhs, int rhs) const noexcept {
    return descending ? lhs > rhs : lhs < rhs;
  }
};

struct TrackingAllocatorStats {
  std::size_t allocate_calls = 0;
  std::size_t deallocate_calls = 0;
  std::size_t allocated_objects = 0;
  std::size_t deallocated_objects = 0;
  std::size_t link_allocations = 0;
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

struct ThrowingMoveCompare {
  ThrowingMoveCompare() = default;
  ThrowingMoveCompare(const ThrowingMoveCompare&) = default;
  ThrowingMoveCompare(ThrowingMoveCompare&&) noexcept(false) {}

  constexpr bool operator()(int lhs, int rhs) const noexcept { return lhs < rhs; }
};

static_assert(!std::is_nothrow_move_constructible_v<chaistl::skip_list_multiset<int, ThrowingMoveCompare>>);

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

TEST(SkipListMultisetTest, MoveConstructWithAllocatorPreservesComparator) {
  chaistl::skip_list_multiset<int, DirectionalCompare> set(DirectionalCompare{.descending = true});
  set.insert(1);
  set.insert(3);
  set.insert(2);
  set.insert(3);

  chaistl::skip_list_multiset<int, DirectionalCompare> moved(std::move(set), set.get_allocator());

  EXPECT_EQ(to_vector(moved), (std::vector<int>{3, 3, 2, 1}));
}

TEST(SkipListMultisetTest, StatefulAllocatorOwnsNodeLinks) {
  auto stats = std::make_shared<TrackingAllocatorStats>();
  {
    chaistl::skip_list_multiset<int, std::less<int>, TrackingAllocator<int>> set(std::less<int>{},
                                                                                 TrackingAllocator<int>{stats});
    for (int i = 0; i < 256; ++i) {
      set.insert(i % 32);
    }
    ASSERT_EQ(stats->link_allocations, set.size());
  }

  EXPECT_EQ(stats->allocate_calls, stats->deallocate_calls);
  EXPECT_EQ(stats->allocated_objects, stats->deallocated_objects);
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
