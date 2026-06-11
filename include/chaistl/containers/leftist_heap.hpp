// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// leftist_heap - priority_queue alias using a leftist heap policy
// ============================================================================
//
// Provides the same priority_queue facade as the default binary heap, but
// stores elements in a meldable leftist tree. It supports join(); stable
// erase/modify handles are intentionally left to pairing_heap/binomial_heap.

#include <chaistl/containers/heap/policy/leftist_heap.hpp>
#include <chaistl/containers/priority_queue.hpp>
#include <chaistl/memory/allocator.hpp>

#include <functional>

namespace chaistl {

template <class T, class Compare = std::less<T>, class Allocator = allocator<T>>
using leftist_heap = priority_queue<T, Compare, Allocator, heap_policy::leftist_heap_policy>;

}  // namespace chaistl
