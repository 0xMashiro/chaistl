// SPDX-License-Identifier: Apache-2.0

// References:
// - std::vector modifiers:
//   https://en.cppreference.com/w/cpp/container/vector
//   https://eel.is/c++draft/vector.modifiers
// - Reallocation self-reference cases are motivated by implementation notes in
//   libstdc++ vector.tcc (_M_realloc_insert and _M_realloc_append).
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/containers/vector.hpp>

#include <algorithm>
#include <array>
#include <ranges>

#include "../support.hpp"

TEST(VectorTest, AppendAndInsertRange) {
  chaistl::vector<int> values{1, 2};

  values.append_range(std::views::iota(3, 6));
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3, 4, 5}));

  std::array middle{8, 9};
  auto it = values.insert_range(values.begin() + 2, middle);

  EXPECT_EQ(*it, 8);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 8, 9, 3, 4, 5}));
}

TEST(VectorTest, AppendEmptyRangeDoesNotChangeVector) {
  chaistl::vector<int> values{1, 2, 3};
  const auto* old_data = values.data();
  const auto old_capacity = values.capacity();

  values.append_range(std::views::empty<int>);

  EXPECT_EQ(values.data(), old_data);
  EXPECT_EQ(values.capacity(), old_capacity);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
}

TEST(VectorTest, SwapPreservesNonPropagatingAllocator) {
  using Allocator = chaistl::test::CompatibleTaggedAllocator<int>;
  using Vector = chaistl::vector<int, Allocator>;

  Vector lhs({1, 2}, Allocator{7});
  Vector rhs({3, 4, 5}, Allocator{9});

  lhs.swap(rhs);

  EXPECT_TRUE(std::ranges::equal(lhs, std::array{3, 4, 5}));
  EXPECT_TRUE(std::ranges::equal(rhs, std::array{1, 2}));
  EXPECT_EQ(lhs.get_allocator().id, 7);
  EXPECT_EQ(rhs.get_allocator().id, 9);
}

TEST(VectorTest, SingleInsertUsesSpareCapacityInPlace) {
  chaistl::test::Counted::move_assignments = 0;
  chaistl::vector<chaistl::test::Counted> values;
  values.reserve(8);
  values.emplace_back(1);
  values.emplace_back(3);
  values.emplace_back(4);

  const auto old_capacity = values.capacity();
  auto it = values.emplace(values.begin() + 1, 2);

  EXPECT_EQ(old_capacity, values.capacity());
  EXPECT_EQ(it->value, 2);
  EXPECT_EQ(values.size(), 4uz);
  EXPECT_EQ(values[0].value, 1);
  EXPECT_EQ(values[1].value, 2);
  EXPECT_EQ(values[2].value, 3);
  EXPECT_EQ(values[3].value, 4);
  EXPECT_EQ(chaistl::test::Counted::move_assignments, 2);
}

TEST(VectorTest, InsertCountUsesSpareCapacityInPlace) {
  chaistl::vector<int> values;
  values.reserve(8);
  values.push_back(1);
  values.push_back(4);
  values.push_back(5);

  const auto* old_data = values.data();
  const auto old_capacity = values.capacity();
  auto it = values.insert(values.begin() + 1, 2, 7);

  EXPECT_EQ(values.data(), old_data);
  EXPECT_EQ(values.capacity(), old_capacity);
  EXPECT_EQ(*it, 7);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 7, 7, 4, 5}));
}

TEST(VectorTest, InsertZeroCountReturnsPositionWithoutChangingVector) {
  chaistl::vector<int> values{1, 2, 3};
  const auto* old_data = values.data();
  const auto old_capacity = values.capacity();

  auto it = values.insert(values.begin() + 1, 0, 9);

  EXPECT_EQ(it, values.begin() + 1);
  EXPECT_EQ(values.data(), old_data);
  EXPECT_EQ(values.capacity(), old_capacity);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
}

TEST(VectorTest, InsertCountAtEndUsesSpareCapacityAsAppend) {
  chaistl::vector<int> values{1, 2};
  values.reserve(6);
  const auto* old_data = values.data();
  const auto old_capacity = values.capacity();

  auto it = values.insert(values.end(), 3, 7);

  EXPECT_EQ(it, values.begin() + 2);
  EXPECT_EQ(values.data(), old_data);
  EXPECT_EQ(values.capacity(), old_capacity);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 7, 7, 7}));
}

TEST(VectorTest, InsertCountSmallerThanSuffixUsesSpareCapacityInPlace) {
  chaistl::vector<int> values{1, 3, 4, 5};
  values.reserve(8);
  const auto* old_data = values.data();
  const auto old_capacity = values.capacity();

  auto it = values.insert(values.begin() + 1, 1, 2);

  EXPECT_EQ(it, values.begin() + 1);
  EXPECT_EQ(values.data(), old_data);
  EXPECT_EQ(values.capacity(), old_capacity);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3, 4, 5}));
}

TEST(VectorTest, InsertCountLargerThanSuffixUsesSpareCapacityInPlace) {
  chaistl::vector<int> values{1, 2, 6};
  values.reserve(8);
  const auto* old_data = values.data();
  const auto old_capacity = values.capacity();

  auto it = values.insert(values.begin() + 2, 3, 9);

  EXPECT_EQ(it, values.begin() + 2);
  EXPECT_EQ(values.data(), old_data);
  EXPECT_EQ(values.capacity(), old_capacity);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 9, 9, 9, 6}));
}

TEST(VectorTest, InsertRangeUsesSpareCapacityInPlace) {
  chaistl::vector<int> values;
  values.reserve(8);
  values.push_back(1);
  values.push_back(4);
  values.push_back(5);
  std::array inserted{2, 3};

  const auto* old_data = values.data();
  const auto old_capacity = values.capacity();
  auto it = values.insert_range(values.begin() + 1, inserted);

  EXPECT_EQ(values.data(), old_data);
  EXPECT_EQ(values.capacity(), old_capacity);
  EXPECT_EQ(*it, 2);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3, 4, 5}));
}

TEST(VectorTest, InsertEmptyRangeReturnsPositionWithoutChangingVector) {
  chaistl::vector<int> values{1, 2, 3};
  const auto* old_data = values.data();
  const auto old_capacity = values.capacity();

  auto it = values.insert_range(values.begin() + 1, std::views::empty<int>);

  EXPECT_EQ(it, values.begin() + 1);
  EXPECT_EQ(values.data(), old_data);
  EXPECT_EQ(values.capacity(), old_capacity);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
}

TEST(VectorTest, InsertRangeAtEndUsesSpareCapacityAsAppend) {
  chaistl::vector<int> values{1, 2};
  values.reserve(6);
  std::array inserted{3, 4, 5};
  const auto* old_data = values.data();
  const auto old_capacity = values.capacity();

  auto it = values.insert_range(values.end(), inserted);

  EXPECT_EQ(it, values.begin() + 2);
  EXPECT_EQ(values.data(), old_data);
  EXPECT_EQ(values.capacity(), old_capacity);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3, 4, 5}));
}

TEST(VectorTest, InsertRangeSmallerThanSuffixUsesSpareCapacityInPlace) {
  chaistl::vector<int> values{1, 3, 4, 5};
  values.reserve(8);
  std::array inserted{2};
  const auto* old_data = values.data();
  const auto old_capacity = values.capacity();

  auto it = values.insert_range(values.begin() + 1, inserted);

  EXPECT_EQ(it, values.begin() + 1);
  EXPECT_EQ(values.data(), old_data);
  EXPECT_EQ(values.capacity(), old_capacity);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3, 4, 5}));
}

TEST(VectorTest, InsertRangeLargerThanSuffixUsesSpareCapacityInPlace) {
  chaistl::vector<int> values{1, 2, 6};
  values.reserve(8);
  std::array inserted{3, 4, 5};
  const auto* old_data = values.data();
  const auto old_capacity = values.capacity();

  auto it = values.insert_range(values.begin() + 2, inserted);

  EXPECT_EQ(it, values.begin() + 2);
  EXPECT_EQ(values.data(), old_data);
  EXPECT_EQ(values.capacity(), old_capacity);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3, 4, 5, 6}));
}

TEST(VectorTest, InsertRangeWithReallocationPreservesPrefixAndSuffix) {
  chaistl::vector<int> values;
  values.reserve(3);
  values.push_back(1);
  values.push_back(4);
  values.push_back(5);
  std::array inserted{2, 3};

  const auto old_capacity = values.capacity();
  auto it = values.insert_range(values.begin() + 1, inserted);

  EXPECT_GT(values.capacity(), old_capacity);
  EXPECT_EQ(*it, 2);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3, 4, 5}));
}

TEST(VectorTest, EmplaceIntoEmptyVectorAllocatesFirstElement) {
  chaistl::vector<int> values;

  auto it = values.emplace(values.begin(), 42);

  EXPECT_EQ(*it, 42);
  EXPECT_EQ(values.size(), 1uz);
  EXPECT_EQ(values.front(), 42);
  EXPECT_GE(values.capacity(), values.size());
}

TEST(VectorTest, EmplaceAtEndUsesSpareCapacityAsAppend) {
  chaistl::vector<int> values{1, 2};
  values.reserve(4);
  const auto old_capacity = values.capacity();

  auto it = values.emplace(values.end(), 3);

  EXPECT_EQ(values.capacity(), old_capacity);
  EXPECT_EQ(*it, 3);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
}

TEST(VectorTest, SingleInsertWithReallocationPreservesOrder) {
  chaistl::vector<int> values;
  values.reserve(3);
  values.push_back(1);
  values.push_back(3);
  values.push_back(4);

  const auto old_capacity = values.capacity();
  auto it = values.insert(values.begin() + 1, 2);

  EXPECT_GT(values.capacity(), old_capacity);
  EXPECT_EQ(*it, 2);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3, 4}));
}

TEST(VectorTest, InsertCountWithReallocationPreservesPrefixAndSuffix) {
  chaistl::vector<int> values;
  values.reserve(3);
  values.push_back(1);
  values.push_back(4);
  values.push_back(5);

  const auto old_capacity = values.capacity();
  auto it = values.insert(values.begin() + 1, 2, 7);

  EXPECT_GT(values.capacity(), old_capacity);
  EXPECT_EQ(*it, 7);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 7, 7, 4, 5}));
}

TEST(VectorTest, InsertHandlesSelfReference) {
  chaistl::vector<int> values{1, 2, 3};
  values.reserve(8);

  values.insert(values.begin() + 1, values.front());

  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 1, 2, 3}));
}

TEST(VectorTest, ReallocationInsertHandlesSelfReferenceBeforeMovedPrefix) {
  chaistl::vector<chaistl::test::MoveMarksSource> values;
  values.reserve(3);
  values.emplace_back(11);
  values.emplace_back(22);
  values.emplace_back(33);

  values.insert(values.begin() + 2, values.front());

  EXPECT_EQ(values.size(), 4uz);
  EXPECT_EQ(values[0].value, 11);
  EXPECT_EQ(values[1].value, 22);
  EXPECT_EQ(values[2].value, 11);
  EXPECT_EQ(values[3].value, 33);
}

TEST(VectorTest, ReallocationPushBackHandlesSelfReference) {
  chaistl::vector<chaistl::test::MoveMarksSource> values;
  values.reserve(2);
  values.emplace_back(11);
  values.emplace_back(22);

  values.push_back(values.front());

  EXPECT_EQ(values.size(), 3uz);
  EXPECT_EQ(values[0].value, 11);
  EXPECT_EQ(values[1].value, 22);
  EXPECT_EQ(values[2].value, 11);
}

TEST(VectorTest, ReallocationPushBackCleansUpWhenMovingOldElementsThrows) {
  chaistl::test::ThrowOnMoveOnly::reset();
  {
    chaistl::vector<chaistl::test::ThrowOnMoveOnly> values;
    values.reserve(2);
    values.emplace_back(1);
    values.emplace_back(2);

    chaistl::test::ThrowOnMoveOnly::moves_before_throw = 1;
    EXPECT_THROW(values.emplace_back(3), std::runtime_error);

    EXPECT_EQ(values.size(), 2uz);
    EXPECT_EQ(chaistl::test::ThrowOnMoveOnly::alive, 2);
  }
  EXPECT_EQ(chaistl::test::ThrowOnMoveOnly::alive, 0);
}

TEST(VectorTest, ReallocationPushBackMovesOldElementsOnSuccess) {
  chaistl::test::ThrowOnMoveOnly::reset();
  {
    chaistl::vector<chaistl::test::ThrowOnMoveOnly> values;
    values.reserve(2);
    values.emplace_back(1);
    values.emplace_back(2);

    values.emplace_back(3);

    EXPECT_EQ(values.size(), 3uz);
    EXPECT_EQ(values[0].value, 1);
    EXPECT_EQ(values[1].value, 2);
    EXPECT_EQ(values[2].value, 3);
    EXPECT_EQ(chaistl::test::ThrowOnMoveOnly::alive, 3);
  }
  EXPECT_EQ(chaistl::test::ThrowOnMoveOnly::alive, 0);
}

TEST(VectorTest, ReallocationPushBackCleansUpWhenNewElementConstructionThrows) {
  chaistl::test::ThrowOnValue::reset();
  {
    chaistl::vector<chaistl::test::ThrowOnValue> values;
    values.reserve(2);
    values.emplace_back(1);
    values.emplace_back(2);

    chaistl::test::ThrowOnValue::throwing_value = 3;
    EXPECT_THROW(values.emplace_back(3), std::runtime_error);

    EXPECT_EQ(values.size(), 2uz);
    EXPECT_EQ(values[0].value, 1);
    EXPECT_EQ(values[1].value, 2);
    EXPECT_EQ(chaistl::test::ThrowOnValue::alive, 2);
  }
  EXPECT_EQ(chaistl::test::ThrowOnValue::alive, 0);
}

TEST(VectorTest, InsertCountKeepsOriginalWhenCopyThrows) {
  chaistl::test::ThrowOnCopy::reset();
  {
    chaistl::vector<chaistl::test::ThrowOnCopy> values;
    values.emplace_back(1);
    values.emplace_back(4);
    chaistl::test::ThrowOnCopy inserted(2);

    chaistl::test::ThrowOnCopy::copies_before_throw = 1;
    EXPECT_THROW(values.insert(values.begin() + 1, 2, inserted), std::runtime_error);

    EXPECT_EQ(values.size(), 2uz);
    EXPECT_EQ(values[0].value, 1);
    EXPECT_EQ(values[1].value, 4);
    EXPECT_EQ(chaistl::test::ThrowOnCopy::alive, 3);
  }
  EXPECT_EQ(chaistl::test::ThrowOnCopy::alive, 0);
}

TEST(VectorTest, InsertRangeKeepsOriginalWhenCopyThrows) {
  chaistl::test::ThrowOnCopy::reset();
  {
    chaistl::vector<chaistl::test::ThrowOnCopy> values;
    values.emplace_back(1);
    values.emplace_back(4);

    std::array source{chaistl::test::ThrowOnCopy(2), chaistl::test::ThrowOnCopy(3)};

    chaistl::test::ThrowOnCopy::copies_before_throw = 1;
    EXPECT_THROW(values.insert_range(values.begin() + 1, source), std::runtime_error);

    EXPECT_EQ(values.size(), 2uz);
    EXPECT_EQ(values[0].value, 1);
    EXPECT_EQ(values[1].value, 4);
    EXPECT_EQ(chaistl::test::ThrowOnCopy::alive, 4);
  }
  EXPECT_EQ(chaistl::test::ThrowOnCopy::alive, 0);
}

TEST(VectorTest, ReallocationInsertCleansUpWhenMovingOldElementsThrows) {
  chaistl::test::ThrowOnMoveOnly::reset();
  {
    chaistl::vector<chaistl::test::ThrowOnMoveOnly> values;
    values.reserve(2);
    values.emplace_back(1);
    values.emplace_back(2);

    chaistl::test::ThrowOnMoveOnly::moves_before_throw = 1;
    EXPECT_THROW(values.emplace(values.begin() + 1, 3), std::runtime_error);

    EXPECT_EQ(values.size(), 2uz);
    EXPECT_EQ(chaistl::test::ThrowOnMoveOnly::alive, 2);
  }
  EXPECT_EQ(chaistl::test::ThrowOnMoveOnly::alive, 0);
}

TEST(VectorTest, ReallocationInsertMovesPrefixAndSuffixOnSuccess) {
  chaistl::test::ThrowOnMoveOnly::reset();
  {
    chaistl::vector<chaistl::test::ThrowOnMoveOnly> values;
    values.reserve(2);
    values.emplace_back(1);
    values.emplace_back(3);

    auto it = values.emplace(values.begin() + 1, 2);

    EXPECT_EQ(it->value, 2);
    EXPECT_EQ(values.size(), 3uz);
    EXPECT_EQ(values[0].value, 1);
    EXPECT_EQ(values[1].value, 2);
    EXPECT_EQ(values[2].value, 3);
    EXPECT_EQ(chaistl::test::ThrowOnMoveOnly::alive, 3);
  }
  EXPECT_EQ(chaistl::test::ThrowOnMoveOnly::alive, 0);
}

TEST(VectorTest, ReallocationInsertCleansUpWhenNewElementConstructionThrows) {
  chaistl::test::ThrowOnValue::reset();
  {
    chaistl::vector<chaistl::test::ThrowOnValue> values;
    values.reserve(2);
    values.emplace_back(1);
    values.emplace_back(2);

    chaistl::test::ThrowOnValue::throwing_value = 3;
    EXPECT_THROW(values.emplace(values.begin() + 1, 3), std::runtime_error);

    EXPECT_EQ(values.size(), 2uz);
    EXPECT_EQ(values[0].value, 1);
    EXPECT_EQ(values[1].value, 2);
    EXPECT_EQ(chaistl::test::ThrowOnValue::alive, 2);
  }
  EXPECT_EQ(chaistl::test::ThrowOnValue::alive, 0);
}

TEST(VectorTest, EraseEmptyRangeReturnsPositionWithoutChangingVector) {
  chaistl::vector<int> values{1, 2, 3};
  const auto old_capacity = values.capacity();

  auto it = values.erase(values.begin() + 1, values.begin() + 1);

  EXPECT_EQ(*it, 2);
  EXPECT_EQ(values.capacity(), old_capacity);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3}));
}

TEST(VectorTest, EraseMovesSuffixInPlaceAndKeepsCapacity) {
  chaistl::test::Counted::move_assignments = 0;
  chaistl::vector<chaistl::test::Counted> values;
  values.reserve(8);
  values.emplace_back(1);
  values.emplace_back(2);
  values.emplace_back(3);
  values.emplace_back(4);

  const auto old_capacity = values.capacity();
  auto it = values.erase(values.begin() + 1);

  EXPECT_EQ(old_capacity, values.capacity());
  EXPECT_EQ(it->value, 3);
  EXPECT_EQ(values.size(), 3uz);
  EXPECT_EQ(values[0].value, 1);
  EXPECT_EQ(values[1].value, 3);
  EXPECT_EQ(values[2].value, 4);
  EXPECT_EQ(chaistl::test::Counted::move_assignments, 2);
}
