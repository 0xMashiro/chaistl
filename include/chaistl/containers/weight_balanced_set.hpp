// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// weight_balanced_set - set alias using weight-balanced tree policy
// ============================================================================
//
// Non-standard extension:
//   - Same public interface as chaistl::set, but selects
//     weight_balanced_tree_policy.
//   - Stores subtree size at each node and balances by relative subtree weight.

#include <chaistl/containers/set.hpp>
#include <chaistl/containers/tree/policy/weight_balanced_tree.hpp>

#include <functional>

namespace chaistl {

template <class Key, class Compare = std::less<Key>, class Allocator = allocator<Key>>
using weight_balanced_set = set<Key, Compare, Allocator, detail::tree::weight_balanced_tree_policy>;

}  // namespace chaistl
