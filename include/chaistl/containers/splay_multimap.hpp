// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// splay_multimap - multimap alias using splay-tree self adjustment
// ============================================================================

#include <chaistl/containers/multimap.hpp>
#include <chaistl/containers/tree/policy/splay_tree.hpp>

#include <functional>
#include <utility>

namespace chaistl {

template <class Key, class T, class Compare = std::less<Key>, class Allocator = allocator<std::pair<const Key, T>>>
using splay_multimap = multimap<Key, T, Compare, Allocator, detail::tree::splay_tree_policy>;

}  // namespace chaistl
