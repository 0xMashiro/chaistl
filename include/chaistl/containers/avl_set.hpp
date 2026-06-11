// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// avl_set - set alias using AVL balancing
// ============================================================================
//
// Non-standard extension:
//   - Same public interface as chaistl::set, but selects avl_tree_policy.
//   - Useful for teaching and benchmarking stricter AVL height balance against
//     the default red-black policy.

#include <chaistl/containers/set.hpp>
#include <chaistl/containers/tree/policy/avl_tree.hpp>

#include <functional>

namespace chaistl {

template <class Key, class Compare = std::less<Key>, class Allocator = allocator<Key>>
using avl_set = set<Key, Compare, Allocator, detail::tree::avl_tree_policy>;

}  // namespace chaistl
