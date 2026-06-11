// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// min_heap - explicit min-priority heap alias
// ============================================================================
//
// Same storage and API as priority_queue<T, std::greater<T>>. The alias keeps
// the standard comparator model visible while giving beginners the conventional
// "min heap" name.

#include <chaistl/containers/priority_queue.hpp>
#include <chaistl/memory/allocator.hpp>

#include <functional>

namespace chaistl {

template <class T, class Allocator = allocator<T>>
using min_heap = priority_queue<T, std::greater<T>, Allocator>;

}  // namespace chaistl
