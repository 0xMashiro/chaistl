// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// binomial_heap - priority_queue alias using a binomial forest policy
// ============================================================================
//
// Non-standard extension:
//   - Same priority_queue facade, but selects binomial_heap_policy.
//   - Provides stable handles, erase/modify, and logarithmic join via the
//     binary-counter forest described in heap/policy/binomial_heap.hpp.

#include <chaistl/containers/heap/policy/binomial_heap.hpp>
#include <chaistl/containers/priority_queue.hpp>

#include <functional>

namespace chaistl {

/// Forest-of-binomial-trees heap with stable handles and logarithmic join.
template <class T, class Compare = std::less<T>, class Allocator = allocator<T>>
using binomial_heap = priority_queue<T, Compare, Allocator, heap_policy::binomial_heap_policy>;

}  // namespace chaistl
