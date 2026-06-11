// SPDX-License-Identifier: Apache-2.0

// References:
// - std::deque:
//   https://en.cppreference.com/w/cpp/container/deque
//   https://eel.is/c++draft/deque
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/concepts.hpp>
#include <chaistl/containers/deque.hpp>

#include <algorithm>
#include <array>
#include <compare>
#include <concepts>
#include <deque>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

struct LessOnly {
  int value;

  friend constexpr bool operator<(const LessOnly& lhs, const LessOnly& rhs) { return lhs.value < rhs.value; }
};

template <class Deque>
class DequeCompatibilityTest : public ::testing::Test {};

using CompatibleIntDeques = ::testing::Types<std::deque<int>, chaistl::deque<int>>;
TYPED_TEST_SUITE(DequeCompatibilityTest, CompatibleIntDeques);

using DequeIterator = chaistl::deque<int>::iterator;
using ConstDequeIterator = chaistl::deque<int>::const_iterator;

static_assert(std::ranges::range<chaistl::deque<int>>);
static_assert(std::ranges::sized_range<chaistl::deque<int>>);
static_assert(std::ranges::random_access_range<chaistl::deque<int>>);
static_assert(!std::ranges::contiguous_range<chaistl::deque<int>>);

static_assert(std::random_access_iterator<DequeIterator>);
static_assert(std::random_access_iterator<ConstDequeIterator>);
static_assert(!std::contiguous_iterator<DequeIterator>);
static_assert(!std::contiguous_iterator<ConstDequeIterator>);

static_assert(chaistl::concepts::random_access_iterator<DequeIterator>);
static_assert(chaistl::concepts::random_access_iterator<ConstDequeIterator>);
static_assert(!chaistl::concepts::contiguous_iterator<DequeIterator>);
static_assert(!chaistl::concepts::contiguous_iterator<ConstDequeIterator>);

static_assert(chaistl::concepts::legacy::random_access_iterator<DequeIterator>);
static_assert(chaistl::concepts::legacy::random_access_iterator<ConstDequeIterator>);
static_assert(!chaistl::concepts::legacy::contiguous_iterator<DequeIterator>);
static_assert(!chaistl::concepts::legacy::contiguous_iterator<ConstDequeIterator>);

static_assert(chaistl::concepts::sequence_container<chaistl::deque<int>>);
static_assert(chaistl::concepts::front_sequence_container<chaistl::deque<int>>);
static_assert(chaistl::concepts::back_sequence_container<chaistl::deque<int>>);
static_assert(chaistl::concepts::random_access_sequence_container<chaistl::deque<int>>);

static_assert(std::sentinel_for<ConstDequeIterator, DequeIterator>);
static_assert(std::sized_sentinel_for<ConstDequeIterator, DequeIterator>);
static_assert(std::constructible_from<ConstDequeIterator, DequeIterator>);
static_assert(std::same_as<std::iter_value_t<DequeIterator>, int>);
static_assert(std::same_as<std::iter_reference_t<DequeIterator>, int&>);
static_assert(std::same_as<std::iter_reference_t<ConstDequeIterator>, const int&>);
static_assert(std::same_as<std::iterator_traits<DequeIterator>::iterator_category, std::random_access_iterator_tag>);

TYPED_TEST(DequeCompatibilityTest, ConstructionAssignmentAndElementAccess) {
  TypeParam values{1, 2, 3};
  EXPECT_EQ(values.size(), 3uz);
  EXPECT_FALSE(values.empty());
  EXPECT_EQ(values.front(), 1);
  EXPECT_EQ(values.back(), 3);
  EXPECT_EQ(values[1], 2);
  EXPECT_EQ(values.at(2), 3);
  EXPECT_THROW((void)values.at(3), std::out_of_range);

  TypeParam copied(values);
  EXPECT_TRUE(std::ranges::equal(copied, std::array{1, 2, 3}));

  copied.assign(2, 9);
  EXPECT_TRUE(std::ranges::equal(copied, std::array{9, 9}));

  copied.assign(values.begin(), values.end());
  EXPECT_TRUE(std::ranges::equal(copied, std::array{1, 2, 3}));
}

TYPED_TEST(DequeCompatibilityTest, PushPopAtBothEnds) {
  TypeParam values;

  values.push_back(2);
  values.push_back(3);
  values.push_front(1);
  values.emplace_back(4);
  values.emplace_front(0);

  EXPECT_TRUE(std::ranges::equal(values, std::array{0, 1, 2, 3, 4}));

  values.pop_front();
  values.pop_back();

  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
}

TYPED_TEST(DequeCompatibilityTest, RandomAccessIteratorsWorkAcrossBlocks) {
  TypeParam values;
  for (int index = 0; index < 1500; ++index) {
    if (index % 2 == 0) {
      values.push_back(index);
    } else {
      values.push_front(index);
    }
  }

  EXPECT_EQ(values.size(), 1500uz);
  EXPECT_EQ(values.end() - values.begin(), 1500);
  EXPECT_EQ(*(values.begin() + 10), values[10]);
  EXPECT_EQ(values.end()[-1], values.back());

  std::sort(values.begin(), values.end());
  for (int index = 0; index < 1500; ++index) {
    EXPECT_EQ(values[static_cast<typename TypeParam::size_type>(index)], index);
  }
}

TEST(DequeTest, IteratorOperationsMatchRandomAccessRequirements) {
  chaistl::deque<int> values;
  for (int index = 0; index < 1500; ++index) {
    values.push_back(index);
  }

  auto first = values.begin();
  auto middle = first;
  middle += 700;
  EXPECT_EQ(*middle, 700);

  middle -= 200;
  EXPECT_EQ(*middle, 500);
  EXPECT_EQ(*(500 + first), 500);
  EXPECT_EQ(first[123], 123);

  const chaistl::deque<int>& const_values = values;
  chaistl::deque<int>::const_iterator const_middle = middle;
  EXPECT_EQ(const_middle - const_values.begin(), 500);
  EXPECT_EQ(const_values.end() - first, 1500);
  EXPECT_LT(first, middle);
  EXPECT_GT(values.end(), middle);
  EXPECT_EQ(first <=> values.begin(), std::strong_ordering::equal);
  EXPECT_EQ(std::ranges::distance(values.begin(), values.end()), 1500);
}

TYPED_TEST(DequeCompatibilityTest, InsertEraseAndResizePreserveOrder) {
  TypeParam values{1, 2, 5};

  auto inserted = values.insert(values.begin() + 2, 3);
  EXPECT_EQ(*inserted, 3);
  values.insert(values.begin() + 3, 1, 4);
  values.insert(values.end(), {6, 7});

  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3, 4, 5, 6, 7}));

  auto next = values.erase(values.begin() + 1, values.begin() + 4);
  EXPECT_EQ(*next, 5);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 5, 6, 7}));

  values.resize(6, 9);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 5, 6, 7, 9, 9}));

  values.resize(2);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 5}));
}

TYPED_TEST(DequeCompatibilityTest, NonMemberEraseOperations) {
  TypeParam values{1, 2, 3, 2, 4};

  EXPECT_EQ(erase(values, 2), 2uz);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 3, 4}));

  EXPECT_EQ(erase_if(values,
                     [](int value) {
                       return value % 2 != 0;
                     }),
            2uz);
  EXPECT_TRUE(std::ranges::equal(values, std::array{4}));
}

TEST(DequeTest, ThreeWayComparisonSynthesizesOrderingFromLessThan) {
  chaistl::deque<LessOnly> lhs{LessOnly{1}, LessOnly{2}};
  chaistl::deque<LessOnly> rhs{LessOnly{1}, LessOnly{3}};

  EXPECT_EQ(lhs <=> rhs, std::weak_ordering::less);
  EXPECT_EQ(rhs <=> lhs, std::weak_ordering::greater);
  EXPECT_EQ(lhs <=> lhs, std::weak_ordering::equivalent);
}

TEST(DequeTest, RangeOperationsWork) {
  chaistl::deque<int> values{2, 3};

  values.prepend_range(std::array{0, 1});
  values.append_range(std::views::iota(4, 7));

  EXPECT_TRUE(std::ranges::equal(values, std::array{0, 1, 2, 3, 4, 5, 6}));

  chaistl::deque<int> copied(std::from_range, values);
  EXPECT_TRUE(std::ranges::equal(copied, values));
}

}  // namespace
