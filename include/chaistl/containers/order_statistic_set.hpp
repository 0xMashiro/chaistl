// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// order_statistic_set - set alias with rank/select operations
// ============================================================================
//
// Non-standard extension:
//   - Same ordered unique-key interface as chaistl::set.
//   - Adds find_by_order(k): iterator to the k-th smallest element, or end().
//   - Adds order_of_key(key): number of elements strictly less than key.
//
// Implemented as a treap augmented with subtree sizes, so operations are
// expected O(log n) while keeping the policy mechanics visible.

#include <chaistl/containers/set.hpp>
#include <chaistl/containers/tree/policy/order_statistic_tree.hpp>

#include <functional>

namespace chaistl {

template <class Key, class Compare = std::less<Key>, class Allocator = allocator<Key>>
using order_statistic_set = set<Key, Compare, Allocator, detail::tree::order_statistic_tree_policy>;

}  // namespace chaistl
