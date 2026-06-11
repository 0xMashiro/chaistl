// SPDX-License-Identifier: Apache-2.0

// Tests for ring_buffer with overwrite_policy (default).
//
// References:
// - Boost.CircularBuffer: https://www.boost.org/doc/libs/release/doc/html/circular_buffer.html
// - EASTL ring_buffer: https://github.com/electronicarts/EASTL

#include <gtest/gtest.h>

#include <chaistl/experimental/containers/ring_buffer.hpp>

#include <array>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>

namespace chaistl::experimental {

using IntRingBuffer = ring_buffer<int>;

namespace {

struct ThrowingCopyValue {
  int value{};

  static inline int alive = 0;
  static inline int copies_before_throw = -1;

  ThrowingCopyValue() { ++alive; }
  explicit ThrowingCopyValue(int init) : value(init) { ++alive; }
  ThrowingCopyValue(const ThrowingCopyValue& other) : value(other.value) {
    if (copies_before_throw == 0) {
      throw std::runtime_error("copy construction failed");
    }
    if (copies_before_throw > 0) {
      --copies_before_throw;
    }
    ++alive;
  }
  ThrowingCopyValue(ThrowingCopyValue&& other) : value(other.value) {
    other.value = -1;
    ++alive;
  }
  ThrowingCopyValue& operator=(const ThrowingCopyValue&) = default;
  ThrowingCopyValue& operator=(ThrowingCopyValue&& other) noexcept {
    value = other.value;
    other.value = -1;
    return *this;
  }
  ~ThrowingCopyValue() { --alive; }

  static void reset() {
    alive = 0;
    copies_before_throw = -1;
  }
};

}  // namespace

// ============================================================================
// Construction
// ============================================================================

TEST(RingBufferOverwrite, DefaultConstruction) {
  IntRingBuffer rb;
  EXPECT_TRUE(rb.empty());
  EXPECT_EQ(rb.size(), 0);
  EXPECT_EQ(rb.capacity(), 0);
}

TEST(RingBufferOverwrite, CapacityConstruction) {
  IntRingBuffer rb(5);
  EXPECT_TRUE(rb.empty());
  EXPECT_EQ(rb.size(), 0);
  EXPECT_EQ(rb.capacity(), 5);
}

TEST(RingBufferOverwrite, CountValueConstruction) {
  IntRingBuffer rb(3, 42);
  EXPECT_EQ(rb.size(), 3);
  EXPECT_EQ(rb.capacity(), 3);
  EXPECT_EQ(rb[0], 42);
  EXPECT_EQ(rb[1], 42);
  EXPECT_EQ(rb[2], 42);
}

TEST(RingBufferOverwrite, IteratorConstruction) {
  std::array source{1, 2, 3, 4, 5};
  IntRingBuffer rb(source.begin(), source.end());
  EXPECT_EQ(rb.size(), 5);
  EXPECT_EQ(rb.capacity(), 5);
  for (size_t i = 0; i < 5; ++i) {
    EXPECT_EQ(rb[i], static_cast<int>(i) + 1);
  }
}

TEST(RingBufferOverwrite, InitializerListConstruction) {
  IntRingBuffer rb{1, 2, 3};
  EXPECT_EQ(rb.size(), 3);
  EXPECT_EQ(rb[0], 1);
  EXPECT_EQ(rb[1], 2);
  EXPECT_EQ(rb[2], 3);
}

TEST(RingBufferOverwrite, FromRangeConstruction) {
  IntRingBuffer rb(std::from_range, std::views::iota(1, 4));
  EXPECT_EQ(rb.size(), 3);
  EXPECT_EQ(rb[0], 1);
  EXPECT_EQ(rb[1], 2);
  EXPECT_EQ(rb[2], 3);
}

TEST(RingBufferOverwrite, CopyConstruction) {
  IntRingBuffer rb1{1, 2, 3};
  IntRingBuffer rb2(rb1);
  EXPECT_EQ(rb2.size(), 3);
  EXPECT_EQ(rb2[0], 1);
  EXPECT_EQ(rb2[1], 2);
  EXPECT_EQ(rb2[2], 3);
}

TEST(RingBufferOverwrite, CopyConstructionCopiesLogicalOrderFromPartialBuffer) {
  IntRingBuffer rb(5);
  rb.push_back(1);
  rb.push_back(2);
  rb.push_back(3);
  rb.pop_front();
  rb.push_back(4);
  rb.push_back(5);

  IntRingBuffer copy(rb);
  EXPECT_EQ(copy.capacity(), 5);
  EXPECT_TRUE(std::ranges::equal(copy, std::array{2, 3, 4, 5}));
}

TEST(RingBufferOverwrite, MoveConstruction) {
  IntRingBuffer rb1{1, 2, 3};
  IntRingBuffer rb2(std::move(rb1));
  EXPECT_EQ(rb2.size(), 3);
  EXPECT_TRUE(std::ranges::equal(rb2, std::array{1, 2, 3}));
}

// ============================================================================
// Assignment
// ============================================================================

TEST(RingBufferOverwrite, CopyAssignment) {
  IntRingBuffer rb1{1, 2, 3};
  IntRingBuffer rb2;
  rb2 = rb1;
  EXPECT_TRUE(std::ranges::equal(rb2, std::array{1, 2, 3}));
}

TEST(RingBufferOverwrite, MoveAssignment) {
  IntRingBuffer rb1{1, 2, 3};
  IntRingBuffer rb2;
  rb2 = std::move(rb1);
  EXPECT_TRUE(std::ranges::equal(rb2, std::array{1, 2, 3}));
}

TEST(RingBufferOverwrite, InitializerListAssignment) {
  IntRingBuffer rb;
  rb = {4, 5, 6};
  EXPECT_TRUE(std::ranges::equal(rb, std::array{4, 5, 6}));
}

// ============================================================================
// Element Access
// ============================================================================

TEST(RingBufferOverwrite, OperatorBracket) {
  IntRingBuffer rb{10, 20, 30};
  EXPECT_EQ(rb[0], 10);
  EXPECT_EQ(rb[1], 20);
  EXPECT_EQ(rb[2], 30);
}

TEST(RingBufferOverwrite, AtThrowsOnOutOfRange) {
  IntRingBuffer rb{1, 2};
  EXPECT_EQ(rb.at(0), 1);
  EXPECT_EQ(rb.at(1), 2);
  EXPECT_THROW((void)rb.at(2), std::out_of_range);
}

TEST(RingBufferOverwrite, FrontBack) {
  IntRingBuffer rb{1, 2, 3};
  EXPECT_EQ(rb.front(), 1);
  EXPECT_EQ(rb.back(), 3);
}

// ============================================================================
// Capacity
// ============================================================================

TEST(RingBufferOverwrite, EmptyFull) {
  IntRingBuffer rb(2);
  EXPECT_TRUE(rb.empty());
  EXPECT_FALSE(rb.full());

  rb.push_back(1);
  EXPECT_FALSE(rb.empty());
  EXPECT_FALSE(rb.full());

  rb.push_back(2);
  EXPECT_FALSE(rb.empty());
  EXPECT_TRUE(rb.full());
}

// ============================================================================
// Push/Pop - Core Ring Buffer Semantics
// ============================================================================

TEST(RingBufferOverwrite, PushBackOverwritesOldest) {
  IntRingBuffer rb(3);
  rb.push_back(1);
  rb.push_back(2);
  rb.push_back(3);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 2, 3}));

  // Full now - push_back overwrites oldest (1)
  rb.push_back(4);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{2, 3, 4}));

  rb.push_back(5);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{3, 4, 5}));
}

TEST(RingBufferOverwrite, PushFrontOverwritesNewest) {
  IntRingBuffer rb(3);
  rb.push_front(1);
  rb.push_front(2);
  rb.push_front(3);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{3, 2, 1}));

  // Full now - push_front overwrites newest (1)
  rb.push_front(4);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{4, 3, 2}));
}

TEST(RingBufferOverwrite, PopBack) {
  IntRingBuffer rb{1, 2, 3};
  rb.pop_back();
  EXPECT_EQ(rb.size(), 2);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 2}));
}

TEST(RingBufferOverwrite, PopFront) {
  IntRingBuffer rb{1, 2, 3};
  rb.pop_front();
  EXPECT_EQ(rb.size(), 2);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{2, 3}));
}

// ============================================================================
// Iterators
// ============================================================================

TEST(RingBufferOverwrite, Iterators) {
  IntRingBuffer rb{1, 2, 3};

  auto it = rb.begin();
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(it, rb.end());
}

TEST(RingBufferOverwrite, ReverseIterators) {
  IntRingBuffer rb{1, 2, 3};

  auto it = rb.rbegin();
  EXPECT_EQ(*it, 3);
  ++it;
  EXPECT_EQ(*it, 2);
  ++it;
  EXPECT_EQ(*it, 1);
  ++it;
  EXPECT_EQ(it, rb.rend());
}

TEST(RingBufferOverwrite, IteratorAfterOverwrite) {
  IntRingBuffer rb(3);
  rb.push_back(1);
  rb.push_back(2);
  rb.push_back(3);
  rb.push_back(4);  // overwrites 1

  std::array expected{2, 3, 4};
  EXPECT_TRUE(std::ranges::equal(rb, expected));
}

// ============================================================================
// Set Capacity
// ============================================================================

TEST(RingBufferOverwrite, SetCapacityGrow) {
  IntRingBuffer rb{1, 2, 3};
  rb.set_capacity(5);
  EXPECT_EQ(rb.capacity(), 5);
  EXPECT_EQ(rb.size(), 3);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 2, 3}));
}

TEST(RingBufferOverwrite, SetCapacityShrink) {
  IntRingBuffer rb{1, 2, 3, 4, 5};
  rb.set_capacity(3);
  EXPECT_EQ(rb.capacity(), 3);
  EXPECT_EQ(rb.size(), 3);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 2, 3}));
}

TEST(RingBufferOverwrite, SetCapacityCleansUpAndLeavesOriginalWhenCopyThrows) {
  ThrowingCopyValue::reset();
  {
    ring_buffer<ThrowingCopyValue> rb(3);
    rb.emplace_back(1);
    rb.emplace_back(2);
    rb.emplace_back(3);

    ThrowingCopyValue::copies_before_throw = 1;
    EXPECT_THROW(rb.set_capacity(5), std::runtime_error);

    EXPECT_EQ(rb.capacity(), 3);
    ASSERT_EQ(rb.size(), 3);
    EXPECT_EQ(rb[0].value, 1);
    EXPECT_EQ(rb[1].value, 2);
    EXPECT_EQ(rb[2].value, 3);
    EXPECT_EQ(ThrowingCopyValue::alive, 3);
    ThrowingCopyValue::copies_before_throw = -1;
  }
  EXPECT_EQ(ThrowingCopyValue::alive, 0);
}

// ============================================================================
// Clear
// ============================================================================

TEST(RingBufferOverwrite, Clear) {
  IntRingBuffer rb{1, 2, 3};
  rb.clear();
  EXPECT_TRUE(rb.empty());
  EXPECT_EQ(rb.size(), 0);
}

// ============================================================================
// Swap
// ============================================================================

TEST(RingBufferOverwrite, Swap) {
  IntRingBuffer rb1{1, 2, 3};
  IntRingBuffer rb2{4, 5};
  rb1.swap(rb2);
  EXPECT_TRUE(std::ranges::equal(rb1, std::array{4, 5}));
  EXPECT_TRUE(std::ranges::equal(rb2, std::array{1, 2, 3}));
}

// ============================================================================
// Comparison
// ============================================================================

TEST(RingBufferOverwrite, Equality) {
  IntRingBuffer rb1{1, 2, 3};
  IntRingBuffer rb2{1, 2, 3};
  IntRingBuffer rb3{1, 2, 4};

  EXPECT_TRUE(rb1 == rb2);
  EXPECT_FALSE(rb1 == rb3);
}

TEST(RingBufferOverwrite, ThreeWayComparison) {
  IntRingBuffer rb1{1, 2, 3};
  IntRingBuffer rb2{1, 2, 4};

  EXPECT_TRUE(rb1 < rb2);
  EXPECT_TRUE(rb2 > rb1);
}

// ============================================================================
// String element type
// ============================================================================

TEST(RingBufferOverwrite, StringElements) {
  ring_buffer<std::string> rb(3);
  rb.push_back("a");
  rb.push_back("b");
  rb.push_back("c");
  EXPECT_TRUE(rb.full());

  rb.push_back("d");  // overwrites "a"
  EXPECT_EQ(rb[0], "b");
  EXPECT_EQ(rb[1], "c");
  EXPECT_EQ(rb[2], "d");
}

}  // namespace chaistl::experimental
