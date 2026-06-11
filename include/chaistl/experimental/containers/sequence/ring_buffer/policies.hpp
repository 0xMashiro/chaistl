// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <utility>

namespace chaistl::experimental {

namespace detail::ring_buffer {

// Capacity behavior tags used by policies.
struct fixed_capacity_tag {};

}  // namespace detail::ring_buffer

// ============================================================================
// Policy protocol
// ============================================================================
//
// A full-behavior policy decides what happens when an element is pushed or
// inserted into a full ring_buffer. The container calls:
//
//   policy.push_back_full(core, value)   // value is a forwarding reference
//   policy.push_front_full(core, value)
//   policy.insert_full(core, pos, value)
//
// Each returns bool: true if the element was stored, false if it was not.
// A policy owns any state its behavior needs (e.g. reject_policy's drop
// counter) — the container is unaware of policy state and only forwards
// accessors via requires-constrained members.
//
// Policies mutate the container exclusively through Core::core_access, a
// public protocol surface (boost::iterator_core_access pattern), so policies
// can be written outside chaistl without friendship.
//
// Contract: push_back_full/push_front_full may be called with a zero-capacity
// core; a policy that stores elements must guard against it and return false.
// insert_full is only called with capacity() > 0.

// ============================================================================
// overwrite_policy
// ============================================================================

/**
 * @brief Overwrite the oldest elements when the ring_buffer is full.
 *
 * This is the classic ring buffer semantics. When push_back is called on a
 * full buffer, the oldest element (at begin) is overwritten and begin advances.
 * When push_front is called on a full buffer, the newest element (at end-1) is
 * overwritten and begin retreats.
 *
 * Suitable for: logs, audio buffers, rolling windows, any scenario where
 * fresh data is more valuable than historical data.
 *
 * @par Reference
 * - Hubble Network: "Old logs are the least valuable. Dropping them to keep
 *   the freshest messages is the right tradeoff for debug output."
 * - Boost.CircularBuffer default behavior.
 * - EASTL ring_buffer default behavior.
 */
struct overwrite_policy {
  using capacity_behavior = detail::ring_buffer::fixed_capacity_tag;

  template <class Core, class T>
  static constexpr bool push_back_full(Core& core, T&& value) {
    if (core.capacity() == 0) {
      return false;  // nowhere to store
    }
    using access = typename Core::core_access;
    access::replace_at_logical(core, 0, std::forward<T>(value));  // overwrite oldest
    access::advance_begin(core);                                  // move begin forward
    return true;
  }

  template <class Core, class T>
  static constexpr bool push_front_full(Core& core, T&& value) {
    if (core.capacity() == 0) {
      return false;
    }
    using access = typename Core::core_access;
    access::retreat_begin(core);                                  // move begin backward
    access::replace_at_logical(core, 0, std::forward<T>(value));  // overwrite new begin (was newest)
    return true;
  }

  template <class Core, class T>
  static constexpr bool insert_full(Core& core, typename Core::iterator pos, T&& value) {
    // Boost.CircularBuffer semantics: if full and pos == begin(), the element
    // would immediately become the dropped oldest — do nothing.
    if (pos == core.begin()) {
      return false;
    }
    using access = typename Core::core_access;
    access::shift_tail_and_insert(core, pos, std::forward<T>(value));
    return true;
  }
};

// ============================================================================
// reject_policy
// ============================================================================

/**
 * @brief Reject new elements when the ring_buffer is full.
 *
 * When the buffer is full, push_back/push_front/insert return without
 * modifying the container. The policy owns a drop counter; the container
 * forwards it via dropped_count() / reset_dropped_count().
 *
 * Suitable for: telemetry, network packets, sensor data — any scenario where
 * data integrity is critical and drops must be counted and reported.
 *
 * @par Reference
 * - Hubble Network: "Reject newest, increment dropped_count. Integrity is
 *   critical (CRC/checksum)."
 * - Linux kernel network stack: drop newest packet when ring is full.
 */
struct reject_policy {
  using capacity_behavior = detail::ring_buffer::fixed_capacity_tag;

  template <class Core, class T>
  constexpr bool push_back_full(Core&, T&&) noexcept {
    ++dropped_count_;
    return false;
  }

  template <class Core, class T>
  constexpr bool push_front_full(Core&, T&&) noexcept {
    ++dropped_count_;
    return false;
  }

  template <class Core, class T>
  constexpr bool insert_full(Core&, typename Core::iterator, T&&) noexcept {
    ++dropped_count_;
    return false;
  }

  [[nodiscard]] constexpr std::size_t dropped_count() const noexcept { return dropped_count_; }

  constexpr void reset_dropped_count() noexcept { dropped_count_ = 0; }

 private:
  std::size_t dropped_count_ = 0;
};

}  // namespace chaistl::experimental
