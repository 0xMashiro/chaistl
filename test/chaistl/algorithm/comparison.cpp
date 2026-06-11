// SPDX-License-Identifier: Apache-2.0

// References:
// - Lexicographical comparison operations:
//   https://en.cppreference.com/w/cpp/algorithm/lexicographical_compare_three_way
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/algorithm/comparison.hpp>
#include <chaistl/containers/list.hpp>
#include <chaistl/containers/vector.hpp>

#include <array>
#include <cctype>
#include <compare>
#include <concepts>
#include <limits>
#include <string_view>

TEST(AlgorithmComparisonTest, LexicographicalCompareThreeWayUsesDefaultComparator) {
  const std::array lhs{1, 2};
  const std::array rhs{1, 3};

  EXPECT_EQ(chaistl::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end()),
            std::strong_ordering::less);
  EXPECT_EQ(chaistl::lexicographical_compare_three_way(rhs.begin(), rhs.end(), lhs.begin(), lhs.end()),
            std::strong_ordering::greater);
  EXPECT_EQ(chaistl::lexicographical_compare_three_way(lhs.begin(), lhs.end(), lhs.begin(), lhs.end()),
            std::strong_ordering::equal);
}

TEST(AlgorithmComparisonTest, LexicographicalCompareThreeWayAcceptsComparator) {
  constexpr auto case_insensitive = [](char lhs, char rhs) {
    return static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(lhs))) <=>
           static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(rhs)));
  };

  constexpr std::string_view lhs = "abc";
  constexpr std::string_view rhs = "ABD";

  EXPECT_EQ(
      chaistl::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), case_insensitive),
      std::strong_ordering::less);
}

TEST(AlgorithmComparisonTest, DefaultThreeWayComparePreservesPartialOrderingForFloatVector) {
  chaistl::vector<float> lhs{1.0F, 2.0F};
  chaistl::vector<float> rhs{1.0F, 3.0F};

  static_assert(std::same_as<decltype(lhs <=> rhs), std::partial_ordering>);

  EXPECT_EQ(lhs <=> rhs, std::partial_ordering::less);
  EXPECT_EQ(rhs <=> lhs, std::partial_ordering::greater);
  EXPECT_EQ(lhs <=> lhs, std::partial_ordering::equivalent);
}

TEST(AlgorithmComparisonTest, DefaultThreeWayComparePreservesPartialOrderingForFloatList) {
  chaistl::list<float> lhs{1.0F, 2.0F};
  chaistl::list<float> rhs{1.0F, 3.0F};

  static_assert(std::same_as<decltype(lhs <=> rhs), std::partial_ordering>);

  EXPECT_EQ(lhs <=> rhs, std::partial_ordering::less);
  EXPECT_EQ(rhs <=> lhs, std::partial_ordering::greater);
  EXPECT_EQ(lhs <=> lhs, std::partial_ordering::equivalent);
}

TEST(AlgorithmComparisonTest, DefaultThreeWayCompareReturnsUnorderedForNaN) {
  const float nan = std::numeric_limits<float>::quiet_NaN();

  chaistl::vector<float> vector_lhs{1.0F, nan};
  chaistl::vector<float> vector_rhs{1.0F, 2.0F};
  chaistl::list<float> list_lhs{1.0F, nan};
  chaistl::list<float> list_rhs{1.0F, 2.0F};

  EXPECT_EQ(vector_lhs <=> vector_rhs, std::partial_ordering::unordered);
  EXPECT_EQ(list_lhs <=> list_rhs, std::partial_ordering::unordered);
}
