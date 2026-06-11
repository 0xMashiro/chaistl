// SPDX-License-Identifier: Apache-2.0

// Tests for ring_buffer with reject_policy.
//
// When full, reject_policy silently drops new elements and counts them.

#include <gtest/gtest.h>

#include <chaistl/experimental/containers/ring_buffer.hpp>

#include <array>
#include <ranges>

namespace chaistl::experimental {

using IntRejectBuffer = ring_buffer<int, ::chaistl::allocator<int>, reject_policy>;

// ============================================================================
// Basic reject semantics
// ============================================================================

TEST(RingBufferReject, PushBackRejectsWhenFull) {
  IntRejectBuffer rb(3);
  rb.push_back(1);
  rb.push_back(2);
  rb.push_back(3);
  EXPECT_TRUE(rb.full());
  EXPECT_EQ(rb.dropped_count(), 0);

  // Full - push_back should reject
  rb.push_back(4);
  EXPECT_EQ(rb.size(), 3);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 2, 3}));
  EXPECT_EQ(rb.dropped_count(), 1);

  rb.push_back(5);
  EXPECT_EQ(rb.dropped_count(), 2);
}

TEST(RingBufferReject, PushFrontRejectsWhenFull) {
  IntRejectBuffer rb(3);
  rb.push_front(1);
  rb.push_front(2);
  rb.push_front(3);
  EXPECT_TRUE(rb.full());

  rb.push_front(4);
  EXPECT_EQ(rb.size(), 3);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{3, 2, 1}));
  EXPECT_EQ(rb.dropped_count(), 1);
}

TEST(RingBufferReject, ResetDroppedCount) {
  IntRejectBuffer rb(2);
  rb.push_back(1);
  rb.push_back(2);
  rb.push_back(3);  // dropped
  rb.push_back(4);  // dropped

  EXPECT_EQ(rb.dropped_count(), 2);
  rb.reset_dropped_count();
  EXPECT_EQ(rb.dropped_count(), 0);
}

// ============================================================================
// Capacity changes preserve drops
// ============================================================================

TEST(RingBufferReject, SetCapacityPreservesDroppedCount) {
  IntRejectBuffer rb(2);
  rb.push_back(1);
  rb.push_back(2);
  rb.push_back(3);  // dropped
  EXPECT_EQ(rb.dropped_count(), 1);

  rb.set_capacity(5);
  EXPECT_EQ(rb.dropped_count(), 1);  // preserved
  EXPECT_EQ(rb.capacity(), 5);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 2}));
}

// ============================================================================
// Copy/move preserves drops
// ============================================================================

TEST(RingBufferReject, CopyPreservesDroppedCount) {
  IntRejectBuffer rb1(2);
  rb1.push_back(1);
  rb1.push_back(2);
  rb1.push_back(3);  // dropped

  IntRejectBuffer rb2(rb1);
  EXPECT_EQ(rb2.dropped_count(), 1);
}

// ============================================================================
// Clear resets size but not drops
// ============================================================================

TEST(RingBufferReject, ClearDoesNotResetDroppedCount) {
  IntRejectBuffer rb(2);
  rb.push_back(1);
  rb.push_back(2);
  rb.push_back(3);  // dropped

  rb.clear();
  EXPECT_TRUE(rb.empty());
  EXPECT_EQ(rb.dropped_count(), 1);  // not reset
}

}  // namespace chaistl::experimental
