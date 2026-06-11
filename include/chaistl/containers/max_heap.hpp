// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// max_heap - explicit max-priority heap alias
// ============================================================================
//
// Same storage and API as priority_queue<T>. The alias exists as a teaching
// entry point: "max heap" names the invariant directly, while priority_queue
// follows the standard-library adaptor name.

#include <chaistl/containers/priority_queue.hpp>
#include <chaistl/memory/allocator.hpp>

#include <functional>

namespace chaistl {

template <class T, class Allocator = allocator<T>>
using max_heap = priority_queue<T, std::less<T>, Allocator>;

}  // namespace chaistl
