// SPDX-License-Identifier: Apache-2.0

// References:
// - std::deque erase and non-member erasure:
//   https://en.cppreference.com/w/cpp/container/deque/erase
//   https://en.cppreference.com/w/cpp/container/deque/erase2
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/containers/deque.hpp>

#include <array>
#include <ranges>

TEST(DequeTest, EraseEmptyRangeReturnsPositionWithoutChangingDeque) {
  chaistl::deque<int> values{1, 2, 3};

  auto next = values.erase(values.begin() + 1, values.begin() + 1);

  EXPECT_EQ(next, values.begin() + 1);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
}

TEST(DequeTest, EraseAtBeginningReturnsNewBegin) {
  chaistl::deque<int> values{1, 2, 3, 4};

  auto next = values.erase(values.begin(), values.begin() + 2);

  EXPECT_EQ(next, values.begin());
  EXPECT_EQ(*next, 3);
  EXPECT_TRUE(std::ranges::equal(values, std::array{3, 4}));
}

TEST(DequeTest, EraseAtEndReturnsEnd) {
  chaistl::deque<int> values{1, 2, 3, 4};

  auto next = values.erase(values.end() - 2, values.end());

  EXPECT_EQ(next, values.end());
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2}));
}

TEST(DequeTest, EraseMiddleMovesLesserSideAndPreservesOrder) {
  chaistl::deque<int> values{1, 2, 3, 4, 5, 6, 7};

  auto next = values.erase(values.begin() + 2, values.begin() + 5);

  EXPECT_EQ(next, values.begin() + 2);
  EXPECT_EQ(*next, 6);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 6, 7}));
}

TEST(DequeTest, PopFrontAndPopBackPreserveRemainingReferences) {
  chaistl::deque<int> values{1, 2, 3, 4};
  int& middle = values[1];

  values.pop_front();
  values.pop_back();
  middle = 9;

  EXPECT_TRUE(std::ranges::equal(values, std::array{9, 3}));
}

TEST(DequeTest, NonMemberEraseOperations) {
  chaistl::deque<int> values{1, 2, 3, 2, 4};

  EXPECT_EQ(erase(values, 2), 2uz);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 3, 4}));

  EXPECT_EQ(erase_if(values,
                     [](int value) {
                       return value % 2 != 0;
                     }),
            2uz);
  EXPECT_TRUE(std::ranges::equal(values, std::array{4}));
}
