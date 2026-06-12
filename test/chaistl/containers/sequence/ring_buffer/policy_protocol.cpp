// SPDX-License-Identifier: Apache-2.0

// Tests for the ring_buffer overflow-policy protocol:
// - full-buffer insert/emplace follows Boost.CircularBuffer semantics
// - try_push_back/try_push_front report per-call success
// - third-party policies can be written outside chaistl (core_access)
// - zero-capacity push is a hardened precondition; try_push is total
// - the container is usable in constant evaluation
//
// References:
// - Boost.CircularBuffer insert: "if full, the first (oldest) element is
//   overwritten; if pos == begin(), the item is not inserted."

#include <gtest/gtest.h>

#include <chaistl/experimental/containers/ring_buffer.hpp>

#include <array>
#include <ranges>

// theirs::evict_oldest_policy — third-party policy via core_access.
#include "test_policies.hpp"

namespace chaistl::experimental {

using IntRingBuffer = ring_buffer<int>;
using IntRejectBuffer = ring_buffer<int, ::chaistl::allocator<int>, reject_policy>;
using IntTheirsBuffer = ring_buffer<int, ::chaistl::allocator<int>, theirs::evict_oldest_policy>;

// ============================================================================
// Full-buffer insert: Boost.CircularBuffer semantics
// ============================================================================

TEST(RingBufferPolicyProtocol, InsertMiddleWhenFullDropsOldest) {
  IntRingBuffer rb{1, 2, 3};
  ASSERT_TRUE(rb.full());

  // Conceptually [1, 99, 2, 3]; oldest (1) is dropped -> [99, 2, 3].
  auto it = rb.insert(rb.begin() + 1, 99);

  EXPECT_TRUE(std::ranges::equal(rb, std::array{99, 2, 3}));
  EXPECT_EQ(*it, 99);  // iterator points at the inserted element
}

TEST(RingBufferPolicyProtocol, InsertLastPositionWhenFull) {
  IntRingBuffer rb{1, 2, 3};

  // Conceptually [1, 2, 99, 3]; oldest (1) dropped -> [2, 99, 3].
  auto it = rb.insert(rb.begin() + 2, 99);

  EXPECT_TRUE(std::ranges::equal(rb, std::array{2, 99, 3}));
  EXPECT_EQ(*it, 99);
}

TEST(RingBufferPolicyProtocol, InsertAtEndWhenFullBehavesLikePushBack) {
  IntRingBuffer rb{1, 2, 3};

  auto it = rb.insert(rb.end(), 99);

  EXPECT_TRUE(std::ranges::equal(rb, std::array{2, 3, 99}));
  EXPECT_EQ(*it, 99);
}

TEST(RingBufferPolicyProtocol, InsertAtBeginWhenFullIsNoop) {
  IntRingBuffer rb{1, 2, 3};

  // The inserted element would immediately be the dropped oldest.
  auto it = rb.insert(rb.begin(), 99);

  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 2, 3}));
  EXPECT_EQ(it, rb.begin());
}

TEST(RingBufferPolicyProtocol, InsertWhenNotFull) {
  IntRingBuffer rb(5);
  rb.push_back(1);
  rb.push_back(2);
  rb.push_back(3);

  auto it = rb.insert(rb.begin() + 1, 99);

  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 99, 2, 3}));
  EXPECT_EQ(*it, 99);
}

TEST(RingBufferPolicyProtocol, InsertFullAfterWrap) {
  // Force begin_ != 0 so logical/physical indices differ.
  IntRingBuffer rb(3);
  rb.push_back(0);
  rb.push_back(1);
  rb.push_back(2);
  rb.push_back(3);  // overwrites 0; buffer = [1, 2, 3], begin_ wrapped
  ASSERT_TRUE(std::ranges::equal(rb, std::array{1, 2, 3}));

  auto it = rb.insert(rb.begin() + 1, 99);

  EXPECT_TRUE(std::ranges::equal(rb, std::array{99, 2, 3}));
  EXPECT_EQ(*it, 99);
}

TEST(RingBufferPolicyProtocol, InsertRejectedWhenFull) {
  IntRejectBuffer rb(3);
  rb.push_back(1);
  rb.push_back(2);
  rb.push_back(3);

  auto it = rb.insert(rb.begin() + 1, 99);

  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 2, 3}));
  EXPECT_EQ(it, rb.begin() + 1);
  EXPECT_EQ(rb.dropped_count(), 1);
}

// ============================================================================
// try_push_back / try_push_front
// ============================================================================

TEST(RingBufferPolicyProtocol, TryPushBackReportsSuccess) {
  IntRingBuffer rb(2);
  EXPECT_TRUE(rb.try_push_back(1));
  EXPECT_TRUE(rb.try_push_back(2));
  // Overwrite policy stores the element even when full.
  EXPECT_TRUE(rb.try_push_back(3));
  EXPECT_TRUE(std::ranges::equal(rb, std::array{2, 3}));
}

TEST(RingBufferPolicyProtocol, TryPushBackReportsRejection) {
  IntRejectBuffer rb(2);
  EXPECT_TRUE(rb.try_push_back(1));
  EXPECT_TRUE(rb.try_push_back(2));
  EXPECT_FALSE(rb.try_push_back(3));
  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 2}));
  EXPECT_EQ(rb.dropped_count(), 1);
}

TEST(RingBufferPolicyProtocol, TryPushFrontReportsSuccessAndRejection) {
  IntRingBuffer overwrite(2);
  EXPECT_TRUE(overwrite.try_push_front(1));
  EXPECT_TRUE(overwrite.try_push_front(2));
  EXPECT_TRUE(overwrite.try_push_front(3));
  EXPECT_TRUE(std::ranges::equal(overwrite, std::array{3, 2}));

  IntRejectBuffer reject(2);
  EXPECT_TRUE(reject.try_push_front(1));
  EXPECT_TRUE(reject.try_push_front(2));
  EXPECT_FALSE(reject.try_push_front(3));
  EXPECT_EQ(reject.dropped_count(), 1);
}

TEST(RingBufferPolicyProtocol, TryPushIsTotalOnZeroCapacity) {
  IntRingBuffer overwrite;
  EXPECT_FALSE(overwrite.try_push_back(1));
  EXPECT_FALSE(overwrite.try_push_front(1));

  IntRejectBuffer reject;
  EXPECT_FALSE(reject.try_push_back(1));
  EXPECT_EQ(reject.dropped_count(), 1);
}

// ============================================================================
// Zero-capacity push_back is a hardened precondition (returns a reference,
// so there is no valid result to hand back).
// ============================================================================

#ifdef CHAI_HARDENING_ENABLED
TEST(RingBufferPolicyProtocolDeathTest, PushBackOnZeroCapacityTerminates) {
  IntRingBuffer rb;
  EXPECT_DEATH(rb.push_back(1), "");
}
#endif

// ============================================================================
// Third-party policy via core_access
// ============================================================================

TEST(RingBufferPolicyProtocol, ThirdPartyPolicyOverwritesOldest) {
  IntTheirsBuffer rb(3);
  rb.push_back(1);
  rb.push_back(2);
  rb.push_back(3);
  rb.push_back(4);  // policy evicts oldest

  EXPECT_TRUE(std::ranges::equal(rb, std::array{2, 3, 4}));

  rb.push_front(0);  // policy overwrites newest
  EXPECT_TRUE(std::ranges::equal(rb, std::array{0, 2, 3}));

  auto it = rb.insert(rb.begin() + 1, 99);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{99, 2, 3}));
  EXPECT_EQ(*it, 99);
}

// ============================================================================
// Constant evaluation
// ============================================================================

constexpr int ring_buffer_constexpr_smoke() {
  ring_buffer<int> rb(3);
  rb.push_back(1);
  rb.push_back(2);
  rb.push_back(3);
  rb.push_back(4);  // wraps: [2, 3, 4]

  ring_buffer<int> copy(rb);  // exercises allocator_uninitialized_copy

  int sum = 0;
  for (int v : copy) {
    sum += v;
  }
  return sum;
}

TEST(RingBufferPolicyProtocol, ConstantEvaluation) {
  static_assert(ring_buffer_constexpr_smoke() == 9);
  EXPECT_EQ(ring_buffer_constexpr_smoke(), 9);
}

}  // namespace chaistl::experimental
