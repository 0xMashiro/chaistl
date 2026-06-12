// SPDX-License-Identifier: Apache-2.0

// References:
// - std::vector interoperability with iterators, ranges, comparison, CTAD, and
//   pmr alias:
//   https://en.cppreference.com/w/cpp/container/vector
//   https://eel.is/c++draft/vector.overview
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/containers/vector.hpp>
#include <chaistl/memory_resource.hpp>

#include <algorithm>
#include <array>
#include <compare>
#include <numeric>
#include <ranges>
#include <type_traits>

namespace {

struct LessOnly {
  int value;

  friend constexpr bool operator<(const LessOnly& lhs, const LessOnly& rhs) { return lhs.value < rhs.value; }
};

}  // namespace

TEST(VectorTest, StandardRangesAndAlgorithmsWork) {
  chaistl::vector<int> values{3, 1, 2};

  std::ranges::sort(values);

  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
  EXPECT_EQ(std::accumulate(values.begin(), values.end(), 0), 6);
}

TEST(VectorTest, FromRangeDeductionAndPmrAlias) {
  auto range = std::views::iota(1, 4);
  chaistl::vector values(std::from_range, range);
  static_assert(std::same_as<decltype(values)::value_type, int>);

  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));

  auto* resource = chaistl::pmr::new_delete_resource();
  chaistl::pmr::vector<int> pmr_values{chaistl::pmr::polymorphic_allocator<int>{resource}};
  pmr_values.push_back(42);
  EXPECT_EQ(pmr_values.front(), 42);
  EXPECT_EQ(pmr_values.get_allocator().resource(), resource);
}

TEST(VectorTest, Comparison) {
  EXPECT_EQ((chaistl::vector<int>{1, 2}), (chaistl::vector<int>{1, 2}));
  EXPECT_NE((chaistl::vector<int>{1, 2}), (chaistl::vector<int>{1, 2, 3}));
  EXPECT_NE((chaistl::vector<int>{1, 2}), (chaistl::vector<int>{1, 3}));
  EXPECT_LT((chaistl::vector<int>{1, 2}), (chaistl::vector<int>{1, 3}));
}

TEST(VectorTest, ThreeWayComparisonSynthesizesOrderingFromLessThan) {
  chaistl::vector<LessOnly> lhs{LessOnly{1}, LessOnly{2}};
  chaistl::vector<LessOnly> rhs{LessOnly{1}, LessOnly{3}};

  EXPECT_EQ(lhs <=> rhs, std::weak_ordering::less);
  EXPECT_EQ(rhs <=> lhs, std::weak_ordering::greater);
  EXPECT_EQ(lhs <=> lhs, std::weak_ordering::equivalent);
}
