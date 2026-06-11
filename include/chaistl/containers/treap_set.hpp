// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// treap_set - set alias using treap balancing
// ============================================================================
//
// Non-standard extension:
//   - Same public interface as chaistl::set, but selects treap_tree_policy.
//   - Treaps are a compact teaching contrast to deterministic rb/avl policies.

#include <chaistl/containers/set.hpp>
#include <chaistl/containers/tree/policy/treap_tree.hpp>

#include <functional>

namespace chaistl {

template <class Key, class Compare = std::less<Key>, class Allocator = allocator<Key>>
using treap_set = set<Key, Compare, Allocator, detail::tree::treap_tree_policy>;

}  // namespace chaistl
