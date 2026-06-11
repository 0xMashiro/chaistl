// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// weight_balanced_map - map alias using weight-balanced tree policy
// ============================================================================

#include <chaistl/containers/map.hpp>
#include <chaistl/containers/tree/policy/weight_balanced_tree.hpp>

#include <functional>
#include <utility>

namespace chaistl {

template <class Key, class T, class Compare = std::less<Key>, class Allocator = allocator<std::pair<const Key, T>>>
using weight_balanced_map = map<Key, T, Compare, Allocator, detail::tree::weight_balanced_tree_policy>;

}  // namespace chaistl
