// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// weight_balanced_multiset - multiset alias using weight-balanced policy
// ============================================================================

#include <chaistl/containers/multiset.hpp>
#include <chaistl/containers/tree/policy/weight_balanced_tree.hpp>

#include <functional>

namespace chaistl {

template <class Key, class Compare = std::less<Key>, class Allocator = allocator<Key>>
using weight_balanced_multiset = multiset<Key, Compare, Allocator, detail::tree::weight_balanced_tree_policy>;

}  // namespace chaistl
