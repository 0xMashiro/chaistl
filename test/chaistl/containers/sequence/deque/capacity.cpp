// SPDX-License-Identifier: Apache-2.0

// References:
// - std::deque capacity and modifier exception guarantees:
//   https://eel.is/c++draft/deque.capacity
//   https://eel.is/c++draft/deque.modifiers
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/containers/deque.hpp>

#include <array>
#include <memory>
#include <ranges>
#include <stdexcept>

#include "../support.hpp"

TEST(DequeTest, ShrinkToFitPreservesElementsAfterFrontAndBackSpare) {
  chaistl::deque<int> values;
  for (int value = 0; value < 5000; ++value) {
    values.push_back(value);
  }
  for (int count = 0; count < 1400; ++count) {
    values.pop_front();
    values.pop_back();
  }

  values.shrink_to_fit();

  EXPECT_EQ(values.size(), 2200uz);
  for (int index = 0; index < 2200; ++index) {
    EXPECT_EQ(values[static_cast<chaistl::deque<int>::size_type>(index)], 1400 + index);
  }
}

TEST(DequeTest, ShrinkToFitDeallocatesEmptyStorageAndAllowsReuse) {
  chaistl::deque<int> values;
  for (int value = 0; value < 2000; ++value) {
    values.push_back(value);
  }
  values.clear();

  values.shrink_to_fit();

  EXPECT_TRUE(values.empty());
  values.push_front(2);
  values.push_back(3);
  values.push_front(1);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
}

TEST(DequeTest, AllocationFailuresDuringFirstInsertionLeaveDequeReusable) {
  using Allocator = chaistl::test::ThrowOnAllocateAllocator<int>;
  using Deque = chaistl::deque<int, Allocator>;
  using MapAllocator = std::allocator_traits<Allocator>::template rebind_alloc<typename Deque::pointer>;

  Allocator::reset();
  MapAllocator::reset();
  Deque values;

  MapAllocator::throw_on_allocate = true;
  EXPECT_THROW(values.emplace_back(1), std::runtime_error);
  EXPECT_TRUE(values.empty());

  MapAllocator::throw_on_allocate = false;
  Allocator::throw_on_allocate = true;
  EXPECT_THROW(values.emplace_back(1), std::runtime_error);
  EXPECT_TRUE(values.empty());

  Allocator::throw_on_allocate = false;
  values.emplace_back(2);
  values.emplace_front(1);

  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2}));
}

TEST(DequeTest, AssignKeepsOriginalWhenRangeConstructionThrows) {
  chaistl::test::ThrowOnCopy::reset();
  {
    chaistl::deque<chaistl::test::ThrowOnCopy> values;
    values.emplace_back(1);
    values.emplace_back(2);

    std::array source{chaistl::test::ThrowOnCopy(3), chaistl::test::ThrowOnCopy(4), chaistl::test::ThrowOnCopy(5)};

    chaistl::test::ThrowOnCopy::copies_before_throw = 1;
    EXPECT_THROW(values.assign(source.begin(), source.end()), std::runtime_error);

    EXPECT_EQ(values.size(), 2uz);
    EXPECT_EQ(values[0].value, 1);
    EXPECT_EQ(values[1].value, 2);
    EXPECT_EQ(chaistl::test::ThrowOnCopy::alive, 5);
  }
  EXPECT_EQ(chaistl::test::ThrowOnCopy::alive, 0);
}

TEST(DequeTest, EmplaceFrontConstructionFailureDoesNotMoveBegin) {
  chaistl::test::ThrowOnValue::reset();
  {
    chaistl::deque<chaistl::test::ThrowOnValue> values;
    for (int value = 0; value < 600; ++value) {
      values.emplace_front(value);
    }

    chaistl::test::ThrowOnValue::throwing_value = 999;
    EXPECT_THROW(values.emplace_front(999), std::runtime_error);

    EXPECT_EQ(values.size(), 600uz);
    EXPECT_EQ(values.front().value, 599);
    EXPECT_EQ(values.back().value, 0);

    values.emplace_front(700);
    EXPECT_EQ(values.size(), 601uz);
    EXPECT_EQ(values.front().value, 700);
    EXPECT_EQ(values[1].value, 599);
  }
  EXPECT_EQ(chaistl::test::ThrowOnValue::alive, 0);
}
