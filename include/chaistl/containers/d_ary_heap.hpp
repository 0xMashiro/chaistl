// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// d_ary_heap - priority_queue alias using a configurable implicit-array heap
// ============================================================================
//
// Provides the same priority_queue facade as the default binary heap, but uses
// heap_policy::d_ary_heap_policy<Arity>. Larger arity shortens heap height and
// makes the push/pop tradeoff explicit for benchmarking and teaching.

#include <chaistl/containers/heap/policy/d_ary_heap.hpp>
#include <chaistl/containers/priority_queue.hpp>
#include <chaistl/memory/allocator.hpp>

#include <functional>

namespace chaistl {

template <class T, std::size_t Arity = 4, class Compare = std::less<T>, class Allocator = allocator<T>>
using d_ary_heap = priority_queue<T, Compare, Allocator, heap_policy::d_ary_heap_policy<Arity>>;

}  // namespace chaistl
