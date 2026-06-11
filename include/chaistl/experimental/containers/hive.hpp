// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/memory/allocator.hpp>

// ============================================================================
// hive — placeholder for P0447R28 (C++26)
// ============================================================================
//
// Proposal:  P0447R28 — Introduction of std::hive to the standard library
// Status:    Adopted for C++26, BUT under active NB review (IT230 requests
//            removal; author is redesigning with different performance
//            characteristics). HIGHLY UNSTABLE — do not implement yet.
// Reference: https://wg21.link/P0447
//
// std::hive is a block-based container that guarantees pointer/reference
// stability across insertions and erasures. It is inspired by game
// programming "object pool" / "bucket array" patterns (plf::colony).
//
// Key design characteristics (from P0447R28):
//   - Elements stored in non-contiguous memory blocks
//   - Erased slots are tracked via a free-list (skipfield / jump-counting)
//   - Insertion reuses erased slots before allocating new blocks
//   - Iterators skip erased slots automatically
//   - No ordering guarantees; iteration order follows insertion order
//     within each block, blocks in allocation order
//
// API highlights:
//   - hive_limits { min, max } block size constraints
//   - reshape(hive_limits) to change block allocation policy
//   - splice(hive&) to move elements between hives without reallocation
//   - get_iterator(const_pointer) to convert stable pointer → iterator
//   - sort(), unique() member algorithms
//
// Implementation complexity: VERY HIGH. Requires:
//   - Custom iterator that skips erased elements
//   - Block management with configurable size limits
//   - Free-list / skipfield data structure
//   - Careful exception safety during block growth
//
// Recommendation: Wait for C++26 finalization and for the redesign
// mentioned by the author (Matthew Bentley) to stabilize before
// attempting an implementation.
//
// TODO: Monitor P0447 status. Do not implement until NB comments
//       are resolved and the design stabilizes.
//
namespace chaistl {
namespace experimental {

// Placeholder — implementation pending standardization stability.

template <class T, class Allocator = chaistl::allocator<T>>
class hive;

}  // namespace experimental
}  // namespace chaistl
