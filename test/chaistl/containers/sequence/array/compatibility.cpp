// SPDX-License-Identifier: Apache-2.0

// References:
// - std::array requirements and semantics:
//   https://en.cppreference.com/w/cpp/container/array
//   https://eel.is/c++draft/array
// - These typed tests intentionally run the same behavior against std::array
//   and chaistl::array. They are compatibility probes, not copied upstream
//   standard-library tests.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/array.hpp>

#include <algorithm>
#include <array>
#include <compare>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using ::testing::ElementsAre;

template <class Array>
class ArrayCompatibilityTest : public ::testing::Test {};

struct LessOnly {
  int value;

  friend constexpr bool operator<(const LessOnly& lhs, const LessOnly& rhs) { return lhs.value < rhs.value; }
};

using CompatibleIntArrays = ::testing::Types<std::array<int, 3>, chaistl::array<int, 3>>;
TYPED_TEST_SUITE(ArrayCompatibilityTest, CompatibleIntArrays);

TYPED_TEST(ArrayCompatibilityTest, AggregateInitializationAndElementAccess) {
  TypeParam values{1, 2, 3};
  const auto& const_values = values;

  EXPECT_EQ(values.size(), 3uz);
  EXPECT_EQ(values.max_size(), 3uz);
  EXPECT_FALSE(values.empty());
  EXPECT_EQ(values[0], 1);
  EXPECT_EQ(const_values[0], 1);
  EXPECT_EQ(values.at(1), 2);
  EXPECT_EQ(const_values.at(2), 3);
  EXPECT_EQ(values.front(), 1);
  EXPECT_EQ(const_values.front(), 1);
  EXPECT_EQ(values.back(), 3);
  EXPECT_EQ(const_values.back(), 3);
  EXPECT_THROW((void)values.at(3), std::out_of_range);
  EXPECT_THROW((void)const_values.at(3), std::out_of_range);
}

TYPED_TEST(ArrayCompatibilityTest, IteratorsAndDataAreContiguous) {
  TypeParam values{3, 1, 2};
  const auto& const_values = values;

  EXPECT_EQ(values.data(), std::to_address(values.begin()));
  EXPECT_EQ(const_values.data(), std::to_address(const_values.begin()));
  EXPECT_EQ(values.end() - values.begin(), 3);
  EXPECT_EQ(*values.cbegin(), 3);
  EXPECT_EQ(*(values.cend() - 1), 2);
  EXPECT_TRUE(std::ranges::contiguous_range<TypeParam>);

  std::ranges::sort(values);

  EXPECT_THAT(values, ElementsAre(1, 2, 3));
  EXPECT_THAT(std::vector<int>(values.rbegin(), values.rend()), ElementsAre(3, 2, 1));
  EXPECT_EQ(*values.crbegin(), 3);
  EXPECT_EQ(*(values.crend() - 1), 1);
}

TYPED_TEST(ArrayCompatibilityTest, FillSwapAndGetWork) {
  TypeParam lhs{1, 2, 3};
  TypeParam rhs{4, 5, 6};

  lhs.fill(9);
  EXPECT_THAT(lhs, ElementsAre(9, 9, 9));

  swap(lhs, rhs);
  EXPECT_THAT(lhs, ElementsAre(4, 5, 6));
  EXPECT_THAT(rhs, ElementsAre(9, 9, 9));

  EXPECT_EQ(get<0>(lhs), 4);
  EXPECT_EQ(get<1>(std::as_const(lhs)), 5);
  EXPECT_EQ(get<2>(TypeParam{7, 8, 9}), 9);
}

TYPED_TEST(ArrayCompatibilityTest, ComparisonOperatorsAreLexicographical) {
  EXPECT_EQ(TypeParam({1, 2, 3}), TypeParam({1, 2, 3}));
  EXPECT_LT(TypeParam({1, 2, 3}), TypeParam({1, 2, 4}));
}

TEST(ArrayTest, ThreeWayComparisonSynthesizesOrderingFromLessThan) {
  chaistl::array<LessOnly, 2> lhs{LessOnly{1}, LessOnly{2}};
  chaistl::array<LessOnly, 2> rhs{LessOnly{1}, LessOnly{3}};

  EXPECT_EQ(lhs <=> rhs, std::weak_ordering::less);
  EXPECT_EQ(rhs <=> lhs, std::weak_ordering::greater);
  EXPECT_EQ(lhs <=> lhs, std::weak_ordering::equivalent);
}

}  // namespace
