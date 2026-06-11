// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// size_balanced_multiset - multiset alias using Size Balanced Tree policy
// ============================================================================

#include <chaistl/containers/multiset.hpp>
#include <chaistl/containers/tree/policy/size_balanced_tree.hpp>

#include <functional>

namespace chaistl {

template <class Key, class Compare = std::less<Key>, class Allocator = allocator<Key>>
using size_balanced_multiset = multiset<Key, Compare, Allocator, detail::tree::size_balanced_tree_policy>;

}  // namespace chaistl
