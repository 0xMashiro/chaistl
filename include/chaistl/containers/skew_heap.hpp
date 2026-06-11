// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// skew_heap - priority_queue alias using a skew heap policy
// ============================================================================
//
// Provides the same priority_queue facade as the default binary heap, but
// stores elements in a self-adjusting meldable tree. It supports join(); stable
// erase/modify handles are intentionally left to pairing_heap/binomial_heap.

#include <chaistl/containers/heap/policy/skew_heap.hpp>
#include <chaistl/containers/priority_queue.hpp>
#include <chaistl/memory/allocator.hpp>

#include <functional>

namespace chaistl {

template <class T, class Compare = std::less<T>, class Allocator = allocator<T>>
using skew_heap = priority_queue<T, Compare, Allocator, heap_policy::skew_heap_policy>;

}  // namespace chaistl
