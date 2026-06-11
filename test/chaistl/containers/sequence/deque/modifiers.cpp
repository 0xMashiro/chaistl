// SPDX-License-Identifier: Apache-2.0

// References:
// - std::deque modifiers:
//   https://en.cppreference.com/w/cpp/container/deque/insert
//   https://en.cppreference.com/w/cpp/container/deque/insert_range
//   https://en.cppreference.com/w/cpp/container/deque/prepend_range
//   https://en.cppreference.com/w/cpp/container/deque/append_range
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/containers/deque.hpp>

#include <array>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <vector>

#include "../support.hpp"

TEST(DequeTest, AppendPrependAndInsertRangePreserveOrder) {
  chaistl::deque<int> values{3, 4};

  values.prepend_range(std::array{1, 2});
  values.append_range(std::views::iota(7, 10));
  auto inserted = values.insert_range(values.begin() + 4, std::array{5, 6});

  EXPECT_EQ(inserted, values.begin() + 4);
  EXPECT_EQ(*inserted, 5);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3, 4, 5, 6, 7, 8, 9}));
}

TEST(DequeTest, InsertEmptyRangeReturnsPositionWithoutChangingDeque) {
  chaistl::deque<int> values{1, 2, 3};

  auto inserted = values.insert_range(values.begin() + 1, std::views::empty<int>);

  EXPECT_EQ(inserted, values.begin() + 1);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
}

TEST(DequeTest, InsertZeroCountReturnsPositionWithoutChangingDeque) {
  chaistl::deque<int> values{1, 2, 3};

  auto inserted = values.insert(values.begin() + 2, 0, 9);

  EXPECT_EQ(inserted, values.begin() + 2);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
}

TEST(DequeTest, InsertRangeAtBothEndsReturnsFirstInsertedElement) {
  chaistl::deque<int> values{3, 4};

  auto front_inserted = values.insert_range(values.begin(), std::array{1, 2});
  auto back_inserted = values.insert_range(values.end(), std::array{5, 6});

  EXPECT_EQ(front_inserted, values.begin());
  EXPECT_EQ(*front_inserted, 1);
  EXPECT_EQ(back_inserted, values.end() - 2);
  EXPECT_EQ(*back_inserted, 5);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3, 4, 5, 6}));
}

TEST(DequeTest, AppendRangeAcceptsMoveOnlyInputRange) {
  std::vector<std::unique_ptr<int>> source;
  source.push_back(std::make_unique<int>(3));
  source.push_back(std::make_unique<int>(4));

  chaistl::deque<std::unique_ptr<int>> values;
  values.push_back(std::make_unique<int>(1));
  values.push_back(std::make_unique<int>(2));
  values.append_range(
      std::ranges::subrange(std::make_move_iterator(source.begin()), std::make_move_iterator(source.end())));

  ASSERT_EQ(values.size(), 4uz);
  EXPECT_EQ(*values[0], 1);
  EXPECT_EQ(*values[1], 2);
  EXPECT_EQ(*values[2], 3);
  EXPECT_EQ(*values[3], 4);
  EXPECT_EQ(source[0], nullptr);
  EXPECT_EQ(source[1], nullptr);
}

TEST(DequeTest, PrependRangeAcceptsMoveOnlyInputRange) {
  std::vector<std::unique_ptr<int>> source;
  source.push_back(std::make_unique<int>(1));
  source.push_back(std::make_unique<int>(2));

  chaistl::deque<std::unique_ptr<int>> values;
  values.push_back(std::make_unique<int>(3));
  values.push_back(std::make_unique<int>(4));
  values.prepend_range(
      std::ranges::subrange(std::make_move_iterator(source.begin()), std::make_move_iterator(source.end())));

  ASSERT_EQ(values.size(), 4uz);
  EXPECT_EQ(*values[0], 1);
  EXPECT_EQ(*values[1], 2);
  EXPECT_EQ(*values[2], 3);
  EXPECT_EQ(*values[3], 4);
  EXPECT_EQ(source[0], nullptr);
  EXPECT_EQ(source[1], nullptr);
}

TEST(DequeTest, InsertRangeAcceptsMoveOnlyInputRange) {
  std::vector<std::unique_ptr<int>> source;
  source.push_back(std::make_unique<int>(2));
  source.push_back(std::make_unique<int>(3));

  chaistl::deque<std::unique_ptr<int>> values;
  values.push_back(std::make_unique<int>(1));
  values.push_back(std::make_unique<int>(4));
  auto inserted = values.insert_range(
      values.begin() + 1,
      std::ranges::subrange(std::make_move_iterator(source.begin()), std::make_move_iterator(source.end())));

  ASSERT_EQ(values.size(), 4uz);
  EXPECT_EQ(inserted, values.begin() + 1);
  EXPECT_EQ(**inserted, 2);
  EXPECT_EQ(*values[0], 1);
  EXPECT_EQ(*values[1], 2);
  EXPECT_EQ(*values[2], 3);
  EXPECT_EQ(*values[3], 4);
  EXPECT_EQ(source[0], nullptr);
  EXPECT_EQ(source[1], nullptr);
}

TEST(DequeTest, InsertCountHandlesSelfReference) {
  chaistl::deque<int> values{1, 2, 3, 4};

  auto inserted = values.insert(values.begin() + 2, 3, values.front());

  EXPECT_EQ(inserted, values.begin() + 2);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 1, 1, 1, 3, 4}));
}

TEST(DequeTest, InsertIteratorRangeHandlesSelfRange) {
  chaistl::deque<int> values{1, 2, 3, 4, 5};

  auto inserted = values.insert(values.begin() + 3, values.begin(), values.begin() + 2);

  EXPECT_EQ(inserted, values.begin() + 3);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3, 1, 2, 4, 5}));
}

TEST(DequeTest, PushAtEitherEndDoesNotInvalidateReferences) {
  chaistl::deque<int> values;
  for (int value = 0; value < 1000; ++value) {
    values.push_back(value);
  }

  int& first = values.front();
  int& middle = values[500];
  int& last = values.back();

  for (int value = 0; value < 1500; ++value) {
    values.push_front(-value - 1);
    values.push_back(1000 + value);
  }

  first = 11;
  middle = 22;
  last = 33;

  EXPECT_EQ(values[1500], 11);
  EXPECT_EQ(values[2000], 22);
  EXPECT_EQ(values[2499], 33);
}

TEST(DequeTest, SingleEndInsertionHasNoEffectWhenConstructionThrows) {
  chaistl::test::ThrowOnValue::reset();
  {
    chaistl::deque<chaistl::test::ThrowOnValue> values;
    values.emplace_back(1);
    values.emplace_back(2);

    chaistl::test::ThrowOnValue::throwing_value = 3;
    EXPECT_THROW(values.emplace_back(3), std::runtime_error);

    EXPECT_EQ(values.size(), 2uz);
    EXPECT_EQ(values.front().value, 1);
    EXPECT_EQ(values.back().value, 2);
  }
  EXPECT_EQ(chaistl::test::ThrowOnValue::alive, 0);
}

TEST(DequeTest, SwapPreservesNonPropagatingAllocator) {
  using Allocator = chaistl::test::CompatibleTaggedAllocator<int>;
  using Deque = chaistl::deque<int, Allocator>;

  Deque lhs({1, 2}, Allocator{7});
  Deque rhs({3, 4, 5}, Allocator{9});

  lhs.swap(rhs);

  EXPECT_TRUE(std::ranges::equal(lhs, std::array{3, 4, 5}));
  EXPECT_TRUE(std::ranges::equal(rhs, std::array{1, 2}));
  EXPECT_EQ(lhs.get_allocator().id, 7);
  EXPECT_EQ(rhs.get_allocator().id, 9);
}
