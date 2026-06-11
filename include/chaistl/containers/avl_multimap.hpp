// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// avl_multimap - multimap alias using AVL balancing
// ============================================================================
//
// Non-standard extension:
//   - Same public interface as chaistl::multimap, but selects avl_tree_policy.
//   - Keeps equivalent-key semantics while making the balance strategy
//     explicit for experiments.

#include <chaistl/containers/multimap.hpp>
#include <chaistl/containers/tree/policy/avl_tree.hpp>

#include <functional>
#include <utility>

namespace chaistl {

template <class Key, class T, class Compare = std::less<Key>, class Allocator = allocator<std::pair<const Key, T>>>
using avl_multimap = multimap<Key, T, Compare, Allocator, detail::tree::avl_tree_policy>;

}  // namespace chaistl
