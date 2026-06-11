// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// treap_multimap - multimap alias using treap balancing
// ============================================================================
//
// Non-standard extension:
//   - Same public interface as chaistl::multimap, but selects treap_tree_policy.
//   - Keeps equivalent-key semantics while exposing randomized/prioritized tree
//     balancing for experiments.

#include <chaistl/containers/multimap.hpp>
#include <chaistl/containers/tree/policy/treap_tree.hpp>

#include <functional>
#include <utility>

namespace chaistl {

template <class Key, class T, class Compare = std::less<Key>, class Allocator = allocator<std::pair<const Key, T>>>
using treap_multimap = multimap<Key, T, Compare, Allocator, detail::tree::treap_tree_policy>;

}  // namespace chaistl
