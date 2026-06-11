// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// treap_map - map alias using treap balancing
// ============================================================================
//
// Non-standard extension:
//   - Same public interface as chaistl::map, but selects treap_tree_policy.
//   - Treaps combine binary-search ordering with heap-ordered priorities, a
//     useful contrast to deterministic rb/avl balancing.

#include <chaistl/containers/map.hpp>
#include <chaistl/containers/tree/policy/treap_tree.hpp>

#include <functional>
#include <utility>

namespace chaistl {

template <class Key, class T, class Compare = std::less<Key>, class Allocator = allocator<std::pair<const Key, T>>>
using treap_map = map<Key, T, Compare, Allocator, detail::tree::treap_tree_policy>;

}  // namespace chaistl
