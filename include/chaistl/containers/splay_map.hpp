// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// splay_map - map alias using splay-tree self adjustment
// ============================================================================

#include <chaistl/containers/map.hpp>
#include <chaistl/containers/tree/policy/splay_tree.hpp>

#include <functional>
#include <utility>

namespace chaistl {

template <class Key, class T, class Compare = std::less<Key>, class Allocator = allocator<std::pair<const Key, T>>>
using splay_map = map<Key, T, Compare, Allocator, detail::tree::splay_tree_policy>;

}  // namespace chaistl
