// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// fibonacci_heap - priority_queue alias using fibonacci_heap_policy
// ============================================================================
//
// Provides the same priority_queue facade as the default binary heap, but uses
// a lazy meldable root-list policy with degree consolidation on pop(). It
// supports join(); stable erase/modify handles remain the domain of
// pairing_heap and binomial_heap.

#include <chaistl/containers/heap/policy/fibonacci_heap.hpp>
#include <chaistl/containers/priority_queue.hpp>
#include <chaistl/memory/allocator.hpp>

#include <functional>

namespace chaistl {

template <class T, class Compare = std::less<T>, class Allocator = allocator<T>>
using fibonacci_heap = priority_queue<T, Compare, Allocator, heap_policy::fibonacci_heap_policy>;

}  // namespace chaistl
