// SPDX-License-Identifier: Apache-2.0

// References:
// - std::reverse_iterator:
//   https://en.cppreference.com/w/cpp/iterator/reverse_iterator
//   https://eel.is/c++draft/reverse.iterators
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/containers/array.hpp>
#include <chaistl/containers/vector.hpp>
#include <chaistl/iterator/adapter/reverse.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

using int_iterator = int*;
using const_int_iterator = const int*;
using reverse_int_iterator = chaistl::reverse_iterator<int_iterator>;
using const_reverse_int_iterator = chaistl::reverse_iterator<const_int_iterator>;

static_assert(std::random_access_iterator<reverse_int_iterator>);
static_assert(std::bidirectional_iterator<reverse_int_iterator>);
static_assert(!std::contiguous_iterator<reverse_int_iterator>);
static_assert(std::sentinel_for<const_reverse_int_iterator, reverse_int_iterator>);
static_assert(std::sized_sentinel_for<const_reverse_int_iterator, reverse_int_iterator>);
static_assert(std::constructible_from<const_reverse_int_iterator, reverse_int_iterator>);
static_assert(std::same_as<reverse_int_iterator::iterator_type, int_iterator>);
static_assert(std::same_as<reverse_int_iterator::iterator_concept, std::random_access_iterator_tag>);
static_assert(std::same_as<std::iter_value_t<reverse_int_iterator>, int>);
static_assert(std::same_as<std::iter_reference_t<reverse_int_iterator>, int&>);
static_assert(std::same_as<decltype(std::ranges::iter_move(std::declval<reverse_int_iterator&>())), int&&>);

static_assert(std::same_as<chaistl::array<int, 3>::reverse_iterator,
                           chaistl::reverse_iterator<chaistl::array<int, 3>::iterator>>);
static_assert(
    std::same_as<chaistl::vector<int>::reverse_iterator, chaistl::reverse_iterator<chaistl::vector<int>::iterator>>);

TEST(ReverseIteratorTest, BaseAndDereferenceFollowPastElementModel) {
  std::array values{1, 2, 3, 4};
  reverse_int_iterator first(int_iterator(values.data() + values.size()));
  reverse_int_iterator last(int_iterator(values.data()));

  EXPECT_EQ(first.base(), values.data() + values.size());
  EXPECT_EQ(*first, 4);
  EXPECT_EQ(first.operator->(), values.data() + 3);
  EXPECT_EQ(first[2], 2);
  EXPECT_EQ(last - first, 4);
}

TEST(ReverseIteratorTest, MovementReversesUnderlyingDirection) {
  std::array values{1, 2, 3, 4};
  reverse_int_iterator iterator(int_iterator(values.data() + values.size()));

  EXPECT_EQ(*iterator, 4);
  EXPECT_EQ(*++iterator, 3);
  EXPECT_EQ(*(iterator++), 3);
  EXPECT_EQ(*iterator, 2);
  EXPECT_EQ(*--iterator, 3);
  EXPECT_EQ(*(iterator--), 3);
  EXPECT_EQ(*iterator, 4);
  EXPECT_EQ(*(iterator + 3), 1);
  EXPECT_EQ(*(3 + iterator), 1);
}

TEST(ReverseIteratorTest, ComparisonsUseReversedOrder) {
  std::array values{1, 2, 3};
  reverse_int_iterator first(int_iterator(values.data() + values.size()));
  reverse_int_iterator second(int_iterator(values.data() + 2));
  const_reverse_int_iterator const_first(first);

  EXPECT_EQ(first, const_first);
  EXPECT_LT(first, second);
  EXPECT_LT(second.base(), first.base());
  EXPECT_EQ(second - first, 1);
}

TEST(ReverseIteratorTest, IterMoveAndIterSwapUsePreviousBaseElement) {
  std::array values{1, 2};
  reverse_int_iterator first(int_iterator(values.data() + values.size()));
  reverse_int_iterator second(int_iterator(values.data() + 1));

  int moved = std::ranges::iter_move(first);
  std::ranges::iter_swap(first, second);

  EXPECT_EQ(moved, 2);
  EXPECT_TRUE(std::ranges::equal(values, std::array{2, 1}));
}

TEST(ReverseIteratorTest, ContainersUseChaistlReverseIterator) {
  chaistl::array<int, 3> fixed{1, 2, 3};
  chaistl::vector<int> dynamic{1, 2, 3};

  EXPECT_TRUE(std::ranges::equal(std::vector<int>(fixed.rbegin(), fixed.rend()), std::array{3, 2, 1}));
  EXPECT_TRUE(std::ranges::equal(std::vector<int>(dynamic.rbegin(), dynamic.rend()), std::array{3, 2, 1}));
}

// C++23 CTAD deduction guide tests
TEST(ReverseIteratorTest, DeductionGuideForRawPointer) {
  int values[] = {1, 2, 3};
  int* ptr = values + 3;

  chaistl::reverse_iterator reversed(ptr);

  static_assert(std::same_as<decltype(reversed), chaistl::reverse_iterator<int*>>);
  EXPECT_EQ(*reversed, 3);
}

TEST(ReverseIteratorTest, DeductionGuideForConstPointer) {
  const int values[] = {1, 2, 3};
  const int* ptr = values + 3;

  chaistl::reverse_iterator reversed(ptr);

  static_assert(std::same_as<decltype(reversed), chaistl::reverse_iterator<const int*>>);
  EXPECT_EQ(*reversed, 3);
}

TEST(ReverseIteratorTest, DeductionGuideForVectorIterator) {
  std::vector<int> values{1, 2, 3};

  chaistl::reverse_iterator reversed(values.end());

  static_assert(std::same_as<decltype(reversed), chaistl::reverse_iterator<std::vector<int>::iterator>>);
  EXPECT_EQ(*reversed, 3);
}

TEST(ReverseIteratorTest, DeductionGuideForArrayIterator) {
  chaistl::array<int, 3> values{1, 2, 3};

  chaistl::reverse_iterator reversed(values.end());

  static_assert(std::same_as<decltype(reversed), chaistl::reverse_iterator<chaistl::array<int, 3>::iterator>>);
  EXPECT_EQ(*reversed, 3);
}
