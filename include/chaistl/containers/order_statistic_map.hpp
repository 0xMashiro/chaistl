// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// order_statistic_map - map alias with rank/select operations
// ============================================================================

#include <chaistl/containers/map.hpp>
#include <chaistl/containers/tree/policy/order_statistic_tree.hpp>

#include <functional>
#include <utility>

namespace chaistl {

template <class Key, class T, class Compare = std::less<Key>, class Allocator = allocator<std::pair<const Key, T>>>
using order_statistic_map = map<Key, T, Compare, Allocator, detail::tree::order_statistic_tree_policy>;

}  // namespace chaistl
