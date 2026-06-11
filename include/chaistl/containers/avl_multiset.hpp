// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// avl_multiset - multiset alias using AVL balancing
// ============================================================================
//
// Non-standard extension:
//   - Same public interface as chaistl::multiset, but selects avl_tree_policy.
//   - Useful for comparing stricter height balance against red-black mutation
//     costs on duplicate-key workloads.

#include <chaistl/containers/multiset.hpp>
#include <chaistl/containers/tree/policy/avl_tree.hpp>

#include <functional>

namespace chaistl {

template <class Key, class Compare = std::less<Key>, class Allocator = allocator<Key>>
using avl_multiset = multiset<Key, Compare, Allocator, detail::tree::avl_tree_policy>;

}  // namespace chaistl
