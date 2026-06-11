// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <type_traits>

namespace chaistl::detail::tree {

/**
 * @brief Base node type for all binary search trees.
 *
 * Contains only the structural pointers (left, right, parent).
 * The value and policy-specific state live in the derived @ref bst_node.
 *
 * Every tree owns one @c bst_node_base directly as its <em>header</em>
 * sentinel (it is never a @ref bst_node and never carries a value).
 * The header and the root are mutually linked parents:
 *
 * @code
 *   header.parent = root          root.parent  = &header
 *   header.left   = leftmost      header.right = rightmost
 *   empty tree:   header.parent = nullptr, header.left = header.right = &header
 * @endcode
 *
 * end() is an iterator holding @c &header, so increment from the maximum
 * and decrement from end() must both treat the header specially
 * (see @ref node_successor, @ref node_predecessor, @ref node_is_header).
 */
struct bst_node_base {
  bst_node_base* left = nullptr;
  bst_node_base* right = nullptr;
  bst_node_base* parent = nullptr;
};

/**
 * @brief Node containing the value, independent of balance policy.
 *
 * This intermediate type allows iterators to cast from @c bst_node_base*
 * to a typed node without knowing the policy type.
 */
template <class Value>
struct bst_value_node : bst_node_base {
  using value_type = Value;

  union {
    Value value;
  };

  constexpr bst_value_node() noexcept : bst_node_base{} {}
  constexpr ~bst_value_node() {}
};

/**
 * @brief Typed BST node, parameterized by value type and balance policy.
 *
 * The policy injects its per-node state via @c node_extension.
 * When the policy has no state (e.g. an empty struct),
 * @c [[no_unique_address]] ensures zero space overhead.
 *
 * @tparam Value  The element type stored in the node.
 * @tparam Policy The balance policy; must provide a nested @c node_extension type.
 */
template <class Value, class Policy>
struct bst_node : bst_value_node<Value> {
  using value_type = Value;
  using policy_type = Policy;
  using extension_type = typename Policy::node_extension;

  [[no_unique_address]] extension_type extension;

  constexpr bst_node() noexcept(std::is_nothrow_default_constructible_v<extension_type>)
      : bst_value_node<Value>(), extension{} {}
  constexpr ~bst_node() {}
};

// ============================================================================
// Node algorithms (independent of balance policy)
// ============================================================================

/**
 * @brief Returns the left-most descendant of @p x.
 *
 * Precondition: @p x != nullptr.
 */
[[nodiscard]] constexpr bst_node_base* node_minimum(bst_node_base* x) noexcept {
  while (x->left != nullptr) {
    x = x->left;
  }
  return x;
}

[[nodiscard]] constexpr const bst_node_base* node_minimum(const bst_node_base* x) noexcept {
  while (x->left != nullptr) {
    x = x->left;
  }
  return x;
}

/**
 * @brief Returns the right-most descendant of @p x.
 *
 * Precondition: @p x != nullptr.
 */
[[nodiscard]] constexpr bst_node_base* node_maximum(bst_node_base* x) noexcept {
  while (x->right != nullptr) {
    x = x->right;
  }
  return x;
}

[[nodiscard]] constexpr const bst_node_base* node_maximum(const bst_node_base* x) noexcept {
  while (x->right != nullptr) {
    x = x->right;
  }
  return x;
}

/**
 * @brief Returns true if @p x is a left child of its parent.
 *
 * Precondition: @p x != nullptr && @p x->parent != nullptr.
 */
[[nodiscard]] constexpr bool node_is_left_child(const bst_node_base* x) noexcept {
  return x == x->parent->left;
}

/**
 * @brief Returns true if @p x is the header sentinel rather than a tree node.
 *
 * Purely structural detection — no flag or color is stored. A header is
 * recognizable because its @c left / @c right point at the leftmost and
 * rightmost nodes, which are not its children:
 *
 * - empty tree: only the header has a null @c parent;
 * - one node:   only the header has @c left == @c right (the single node);
 * - otherwise:  a real node's non-null children point back to it via
 *               @c parent, while leftmost/rightmost do not point back to
 *               the header (and a real node never has @c left == @c right).
 *
 * Reference: boost::intrusive::bstree_algorithms::is_header.
 */
[[nodiscard]] constexpr bool node_is_header(const bst_node_base* x) noexcept {
  if (x->parent == nullptr) {
    return true;  // empty tree: in-tree nodes always have a parent
  }
  if (x->left == nullptr || x->right == nullptr) {
    return false;  // header always has leftmost/rightmost set
  }
  return x->left == x->right        // single-node tree
         || x->left->parent != x    // leftmost is not our child
         || x->right->parent != x;  // rightmost is not our child
}

/**
 * @brief In-order successor of @p x.
 *
 * If @p x has a right child, the successor is the minimum of the right
 * subtree. Otherwise, walk up until @p x is no longer a right child.
 *
 * The walk can pass through the header when @p x is the maximum: the
 * header/root parent cycle then makes the loop stop with either
 * x == header (root was not the maximum) or x == root, p == header
 * (root was the maximum). The final adjustment maps both cases to the
 * header, so ++max yields end().
 *
 * Reference: libstdc++ _Rb_tree_increment (tree.cc).
 */
[[nodiscard]] constexpr bst_node_base* node_successor(bst_node_base* x) noexcept {
  if (x->right != nullptr) {
    return node_minimum(x->right);
  }
  bst_node_base* p = x->parent;
  while (x == p->right) {
    x = p;
    p = p->parent;
  }
  // When the maximum is the root, the walk overshoots into the header
  // (x == header, p == root). Detect this via x->right == p and stay put;
  // in every other case the successor is the ancestor p.
  if (x->right != p) {
    x = p;
  }
  return x;
}

[[nodiscard]] constexpr const bst_node_base* node_successor(const bst_node_base* x) noexcept {
  return node_successor(const_cast<bst_node_base*>(x));
}

/**
 * @brief In-order predecessor of @p x.
 *
 * Decrementing end() must yield the maximum, so the header is detected
 * first: its @c right caches the rightmost node. For tree nodes: if @p x
 * has a left child, the predecessor is the maximum of the left subtree;
 * otherwise walk up until @p x is no longer a left child.
 *
 * Decrementing begin() is undefined, so unlike @ref node_successor no
 * header overshoot adjustment is needed.
 *
 * Reference: libstdc++ _Rb_tree_decrement (tree.cc), with the color-based
 * header test replaced by the structural @ref node_is_header.
 */
[[nodiscard]] constexpr bst_node_base* node_predecessor(bst_node_base* x) noexcept {
  if (node_is_header(x)) {
    return x->right;  // --end() is the rightmost node
  }
  if (x->left != nullptr) {
    return node_maximum(x->left);
  }
  bst_node_base* p = x->parent;
  while (x == p->left) {
    x = p;
    p = p->parent;
  }
  return p;
}

[[nodiscard]] constexpr const bst_node_base* node_predecessor(const bst_node_base* x) noexcept {
  return node_predecessor(const_cast<bst_node_base*>(x));
}

// ============================================================================
// Structural mutations shared by balance policies
//
// These move only the structural pointers; balancing state (color, balance
// factor, ...) is untouched, so they operate on bst_node_base alone and are
// shared by every policy.
// ============================================================================

/**
 * @brief Link the detached node @p x as a leaf under @p parent.
 *
 * @p x is attached on the side selected by @p insert_left; when @p parent
 * is the header the tree is empty and @p x becomes the root. The header's
 * root/leftmost/rightmost pointers are maintained. Balancing state is not
 * touched — the policy initializes it afterwards.
 */
constexpr void node_link_leaf(const bool insert_left,
                              bst_node_base* x,
                              bst_node_base* parent,
                              bst_node_base& header) noexcept {
  x->parent = parent;
  x->left = nullptr;
  x->right = nullptr;

  if (insert_left) {
    parent->left = x;  // when parent == &header this also sets leftmost
    if (parent == &header) {
      header.parent = x;
      header.right = x;
    } else if (parent == header.left) {
      header.left = x;
    }
  } else {
    parent->right = x;
    if (parent == header.right) {
      header.right = x;
    }
  }
}

/**
 * @brief Left rotate at @p x.
 *
 *       x              y
 *      / \            / \
 *     a   y    =>    x   c
 *        / \        / \
 *       b   c      a   b
 *
 * @p root is the header's root slot, passed by reference: rotating at
 * the root retargets it directly. This is the protocol that makes
 * rotations safe under the header-sentinel layout — the root's parent
 * is the header, so "fix the parent's child pointer" would otherwise
 * clobber the header's leftmost/rightmost caches.
 */
constexpr void node_rotate_left(bst_node_base* x, bst_node_base*& root) noexcept {
  bst_node_base* const y = x->right;

  x->right = y->left;
  if (y->left != nullptr) {
    y->left->parent = x;
  }
  y->parent = x->parent;

  if (x == root) {
    root = y;
  } else if (x == x->parent->left) {
    x->parent->left = y;
  } else {
    x->parent->right = y;
  }
  y->left = x;
  x->parent = y;
}

/**
 * @brief Right rotate at @p x (mirror of @ref node_rotate_left).
 *
 *         x            y
 *        / \          / \
 *       y   c   =>   a   x
 *      / \              / \
 *     a   b            b   c
 */
constexpr void node_rotate_right(bst_node_base* x, bst_node_base*& root) noexcept {
  bst_node_base* const y = x->left;

  x->left = y->right;
  if (y->right != nullptr) {
    y->right->parent = x;
  }
  y->parent = x->parent;

  if (x == root) {
    root = y;
  } else if (x == x->parent->right) {
    x->parent->right = y;
  } else {
    x->parent->left = y;
  }
  y->right = x;
  x->parent = y;
}

}  // namespace chaistl::detail::tree
