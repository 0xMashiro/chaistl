// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// treap_multiset - multiset alias using treap balancing
// ============================================================================
//
// Non-standard extension:
//   - Same public interface as chaistl::multiset, but selects treap_tree_policy.
//   - Useful for comparing duplicate-key workloads under probabilistic tree
//     balancing.

#include <chaistl/containers/multiset.hpp>
#include <chaistl/containers/tree/policy/treap_tree.hpp>

#include <functional>

namespace chaistl {

template <class Key, class Compare = std::less<Key>, class Allocator = allocator<Key>>
using treap_multiset = multiset<Key, Compare, Allocator, detail::tree::treap_tree_policy>;

}  // namespace chaistl
