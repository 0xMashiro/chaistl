// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// avl_map - map alias using AVL balancing
// ============================================================================
//
// Non-standard extension:
//   - Same public interface as chaistl::map, but selects avl_tree_policy.
//   - Useful for teaching and benchmarking the lookup/update trade-off between
//     stricter AVL balance and the default red-black policy.

#include <chaistl/containers/map.hpp>
#include <chaistl/containers/tree/policy/avl_tree.hpp>

#include <functional>
#include <utility>

namespace chaistl {

template <class Key, class T, class Compare = std::less<Key>, class Allocator = allocator<std::pair<const Key, T>>>
using avl_map = map<Key, T, Compare, Allocator, detail::tree::avl_tree_policy>;

}  // namespace chaistl
