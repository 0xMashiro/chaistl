// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// order_statistic_multiset - multiset alias with rank/select operations
// ============================================================================

#include <chaistl/containers/multiset.hpp>
#include <chaistl/containers/tree/policy/order_statistic_tree.hpp>

#include <functional>

namespace chaistl {

template <class Key, class Compare = std::less<Key>, class Allocator = allocator<Key>>
using order_statistic_multiset = multiset<Key, Compare, Allocator, detail::tree::order_statistic_tree_policy>;

}  // namespace chaistl
