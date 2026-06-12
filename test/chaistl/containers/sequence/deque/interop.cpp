// SPDX-License-Identifier: Apache-2.0

// References:
// - std::deque ranges-aware construction and insertion:
//   https://en.cppreference.com/w/cpp/container/deque/deque
//   https://en.cppreference.com/w/cpp/container/deque/insert_range
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/containers/deque.hpp>
#include <chaistl/memory_resource.hpp>

#include <algorithm>
#include <array>
#include <compare>
#include <ranges>
#include <type_traits>
#include <vector>

TEST(DequeTest, StandardRangesAndAlgorithmsWork) {
  chaistl::deque<int> values{3, 1, 2};

  std::ranges::sort(values);

  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
  EXPECT_EQ(std::ranges::distance(values), 3);
}

TEST(DequeTest, FromRangeDeductionAndPmrAlias) {
  std::array source{1, 2, 3};

  chaistl::deque from_iterators(source.begin(), source.end());
  chaistl::deque from_range(std::from_range, source);

  static_assert(std::same_as<decltype(from_iterators), chaistl::deque<int>>);
  static_assert(std::same_as<decltype(from_range), chaistl::deque<int>>);
  EXPECT_TRUE(std::ranges::equal(from_iterators, source));
  EXPECT_TRUE(std::ranges::equal(from_range, source));

  auto* resource = chaistl::pmr::new_delete_resource();
  chaistl::pmr::deque<int> pmr_values{chaistl::pmr::polymorphic_allocator<int>{resource}};
  pmr_values.append_range(source);

  EXPECT_TRUE(std::ranges::equal(pmr_values, source));
  EXPECT_EQ(pmr_values.get_allocator().resource(), resource);
}

TEST(DequeTest, Comparison) {
  chaistl::deque<int> lhs{1, 2, 3};
  chaistl::deque<int> equal{1, 2, 3};
  chaistl::deque<int> less{1, 2};
  chaistl::deque<int> greater{1, 2, 4};

  EXPECT_TRUE(lhs == equal);
  EXPECT_FALSE(lhs == less);
  EXPECT_EQ(less <=> lhs, std::strong_ordering::less);
  EXPECT_EQ(greater <=> lhs, std::strong_ordering::greater);
}
