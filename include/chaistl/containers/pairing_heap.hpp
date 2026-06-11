// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// pairing_heap - priority_queue alias using a pairing heap policy
// ============================================================================
//
// Non-standard extension:
//   - Same priority_queue facade, but selects pairing_heap_policy.
//   - Provides stable handles, erase/modify, and O(1) join around the mutable
//     heap design described in heap/policy/pairing_heap.hpp.

#include <chaistl/containers/heap/policy/pairing_heap.hpp>
#include <chaistl/containers/priority_queue.hpp>

#include <functional>

namespace chaistl {

/// Mutable heap: stable handles, erase/modify, O(1) join.
template <class T, class Compare = std::less<T>, class Allocator = allocator<T>>
using pairing_heap = priority_queue<T, Compare, Allocator, heap_policy::pairing_heap_policy>;

}  // namespace chaistl
