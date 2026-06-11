// SPDX-License-Identifier: Apache-2.0

// References:
// - std::vector capacity operations:
//   https://en.cppreference.com/w/cpp/container/vector
//   https://eel.is/c++draft/vector.capacity
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/containers/vector.hpp>

#include <algorithm>
#include <array>
#include <ranges>
#include <stdexcept>

#include "../support.hpp"

TEST(VectorTest, ReserveNoOpAndLengthError) {
  chaistl::vector<int> values{1, 2, 3};
  const auto* old_data = values.data();
  const auto old_capacity = values.capacity();

  values.reserve(values.size());

  EXPECT_EQ(values.data(), old_data);
  EXPECT_EQ(values.capacity(), old_capacity);

  chaistl::vector<int, chaistl::test::SmallMaxAllocator<int>> small;
  EXPECT_EQ(small.max_size(), 3uz);
  EXPECT_THROW(small.reserve(4), std::length_error);
}

TEST(VectorTest, ReservePropagatesAllocationFailureWithoutChangingVector) {
  using Allocator = chaistl::test::ThrowOnAllocateAllocator<int>;
  using Vector = chaistl::vector<int, Allocator>;

  Allocator::reset();
  Vector values{1, 2, 3};
  const auto* old_data = values.data();
  const auto old_capacity = values.capacity();

  Allocator::throw_on_allocate = true;
  EXPECT_THROW(values.reserve(old_capacity + 1), std::runtime_error);

  EXPECT_EQ(values.data(), old_data);
  EXPECT_EQ(values.capacity(), old_capacity);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
  Allocator::reset();
}

TEST(VectorTest, GrowthPolicyClampsAtMaxSize) {
  using Vector = chaistl::vector<int, chaistl::test::SmallMaxAllocator<int>>;

  Vector values;
  values.reserve(2);
  values.push_back(1);
  values.push_back(2);

  values.push_back(3);

  EXPECT_EQ(values.capacity(), values.max_size());
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
  EXPECT_THROW(values.push_back(4), std::length_error);
}

TEST(VectorTest, ResizeRollsBackConstructedSuffixWhenDefaultConstructionThrows) {
  chaistl::test::ThrowOnDefault::reset();
  {
    chaistl::vector<chaistl::test::ThrowOnDefault> values;
    values.reserve(4);
    values.emplace_back(1);
    values.emplace_back(2);

    chaistl::test::ThrowOnDefault::defaults_before_throw = 1;
    EXPECT_THROW(values.resize(4), std::runtime_error);

    EXPECT_EQ(values.size(), 2uz);
    EXPECT_EQ(values[0].value, 1);
    EXPECT_EQ(values[1].value, 2);
    EXPECT_EQ(chaistl::test::ThrowOnDefault::alive, 2);
  }
  EXPECT_EQ(chaistl::test::ThrowOnDefault::alive, 0);
}

TEST(VectorTest, ResizeRollsBackConstructedSuffixWhenCopyThrows) {
  chaistl::test::ThrowOnCopy::reset();
  {
    chaistl::vector<chaistl::test::ThrowOnCopy> values;
    values.reserve(4);
    values.emplace_back(1);
    values.emplace_back(2);
    chaistl::test::ThrowOnCopy fill(9);

    chaistl::test::ThrowOnCopy::copies_before_throw = 1;
    EXPECT_THROW(values.resize(4, fill), std::runtime_error);

    EXPECT_EQ(values.size(), 2uz);
    EXPECT_EQ(values[0].value, 1);
    EXPECT_EQ(values[1].value, 2);
    EXPECT_EQ(chaistl::test::ThrowOnCopy::alive, 3);
  }
  EXPECT_EQ(chaistl::test::ThrowOnCopy::alive, 0);
}

TEST(VectorTest, CountValueConstructionReleasesStorageWhenCopyThrows) {
  chaistl::test::ThrowOnCopy::reset();
  {
    chaistl::test::ThrowOnCopy value(7);
    chaistl::test::ThrowOnCopy::copies_before_throw = 1;

    EXPECT_THROW((chaistl::vector<chaistl::test::ThrowOnCopy>(3, value)), std::runtime_error);

    EXPECT_EQ(chaistl::test::ThrowOnCopy::alive, 1);
  }
  EXPECT_EQ(chaistl::test::ThrowOnCopy::alive, 0);
}

TEST(VectorTest, ReserveCleansUpNewStorageWhenMoveThrows) {
  chaistl::test::ThrowOnMoveOnly::reset();
  {
    chaistl::vector<chaistl::test::ThrowOnMoveOnly> values;
    values.reserve(2);
    values.emplace_back(1);
    values.emplace_back(2);

    chaistl::test::ThrowOnMoveOnly::moves_before_throw = 1;
    EXPECT_THROW(values.reserve(4), std::runtime_error);

    EXPECT_EQ(values.size(), 2uz);
    EXPECT_EQ(chaistl::test::ThrowOnMoveOnly::alive, 2);
  }
  EXPECT_EQ(chaistl::test::ThrowOnMoveOnly::alive, 0);
}

TEST(VectorTest, ShrinkToFit) {
  chaistl::vector<int> values;
  values.reserve(16);
  values.push_back(1);
  values.push_back(2);

  values.shrink_to_fit();

  EXPECT_EQ(values.size(), 2uz);
  EXPECT_EQ(values.capacity(), values.size());
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2}));
}

TEST(VectorTest, ShrinkToFitNoOpWhenTightAndReleasesEmptyStorage) {
  chaistl::vector<int> tight{1, 2, 3};
  tight.shrink_to_fit();
  const auto* tight_data = tight.data();
  const auto tight_capacity = tight.capacity();

  tight.shrink_to_fit();

  EXPECT_EQ(tight.data(), tight_data);
  EXPECT_EQ(tight.capacity(), tight_capacity);

  chaistl::vector<int> empty;
  empty.reserve(8);
  empty.shrink_to_fit();

  EXPECT_TRUE(empty.empty());
  EXPECT_EQ(empty.capacity(), 0uz);
  EXPECT_EQ(empty.data(), nullptr);
}
