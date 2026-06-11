// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// size_balanced_multimap - multimap alias using Size Balanced Tree policy
// ============================================================================

#include <chaistl/containers/multimap.hpp>
#include <chaistl/containers/tree/policy/size_balanced_tree.hpp>

#include <functional>
#include <utility>

namespace chaistl {

template <class Key, class T, class Compare = std::less<Key>, class Allocator = allocator<std::pair<const Key, T>>>
using size_balanced_multimap = multimap<Key, T, Compare, Allocator, detail::tree::size_balanced_tree_policy>;

}  // namespace chaistl
