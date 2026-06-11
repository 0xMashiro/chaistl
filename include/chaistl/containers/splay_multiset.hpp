// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// splay_multiset - multiset alias using splay-tree self adjustment
// ============================================================================

#include <chaistl/containers/multiset.hpp>
#include <chaistl/containers/tree/policy/splay_tree.hpp>

#include <functional>

namespace chaistl {

template <class Key, class Compare = std::less<Key>, class Allocator = allocator<Key>>
using splay_multiset = multiset<Key, Compare, Allocator, detail::tree::splay_tree_policy>;

}  // namespace chaistl
