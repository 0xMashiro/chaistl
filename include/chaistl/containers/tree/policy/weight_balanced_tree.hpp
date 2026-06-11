// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/tree/node.hpp>

#include <cstddef>

namespace chaistl::detail::tree {

// ============================================================================
// Weight-Balanced Tree Policy
// ============================================================================
//
// A deterministic BST policy that stores subtree sizes and rotates when one
// side becomes too heavy. This makes a useful teaching contrast with AVL
// (height metadata), RB (color metadata), Treap (random priority), and
// Order-Statistic Tree (the same size metadata exposed as rank/select).
//
// The invariant used here is intentionally simple and local:
//
//   size(left)  <= 4 * (size(right) + 1)
//   size(right) <= 4 * (size(left)  + 1)
//
// The "+1" treats an empty sibling as one unit of slack, avoiding noisy
// rotations in tiny subtrees while keeping pathological chains from growing.
// ============================================================================

struct weight_balanced_tree_policy {
  struct node_extension {
    std::size_t subtree_size;
  };

  template <class Node>
  static constexpr void insert_and_rebalance(const bool insert_left,
                                             Node* x,
                                             bst_node_base* parent,
                                             bst_node_base& header) noexcept {
    node_link_leaf(insert_left, x, parent, header);
    subtree_size_ref<Node>(x) = 1;
    rebalance_ancestors<Node>(x, header);
  }

  template <class Node>
  static constexpr bst_node_base* unlink_and_rebalance(Node* z_node, bst_node_base& header) noexcept {
    bst_node_base* const z = z_node;
    bst_node_base*& root = header.parent;
    bst_node_base*& leftmost = header.left;
    bst_node_base*& rightmost = header.right;

    while (z->left != nullptr || z->right != nullptr) {
      if (z->left == nullptr) {
        rotate_left<Node>(z, root);
      } else if (z->right == nullptr) {
        rotate_right<Node>(z, root);
      } else if (subtree_size_of<Node>(z->left) > subtree_size_of<Node>(z->right)) {
        rotate_right<Node>(z, root);
      } else {
        rotate_left<Node>(z, root);
      }
    }

    if (z == root) {
      root = nullptr;
      leftmost = &header;
      rightmost = &header;
    } else {
      bst_node_base* const parent = z->parent;
      if (parent->left == z) {
        parent->left = nullptr;
      } else {
        parent->right = nullptr;
      }
      if (leftmost == z) {
        leftmost = parent;
      }
      if (rightmost == z) {
        rightmost = parent;
      }
      rebalance_ancestors<Node>(parent, header);
    }

    return z;
  }

  template <class Node>
  [[nodiscard]] static constexpr bool verify(const Node* root) noexcept {
    return verify_node(root);
  }

 private:
  template <class Node>
  [[nodiscard]] static constexpr std::size_t& subtree_size_ref(bst_node_base* node) noexcept {
    return static_cast<Node*>(node)->extension.subtree_size;
  }

  template <class Node>
  [[nodiscard]] static constexpr std::size_t subtree_size_of(const bst_node_base* node) noexcept {
    return node == nullptr ? 0 : static_cast<const Node*>(node)->extension.subtree_size;
  }

  template <class Node>
  static constexpr void refresh(bst_node_base* node) noexcept {
    subtree_size_ref<Node>(node) = 1 + subtree_size_of<Node>(node->left) + subtree_size_of<Node>(node->right);
  }

  [[nodiscard]] static constexpr bool left_too_heavy(const std::size_t left, const std::size_t right) noexcept {
    return left > 4 * (right + 1);
  }

  [[nodiscard]] static constexpr bool right_too_heavy(const std::size_t left, const std::size_t right) noexcept {
    return right > 4 * (left + 1);
  }

  template <class Node>
  static constexpr bst_node_base* rebalance_at(bst_node_base* node, bst_node_base*& root) noexcept {
    for (;;) {
      refresh<Node>(node);
      const std::size_t left = subtree_size_of<Node>(node->left);
      const std::size_t right = subtree_size_of<Node>(node->right);

      if (left_too_heavy(left, right)) {
        bst_node_base* const child = node->left;
        if (subtree_size_of<Node>(child->right) > subtree_size_of<Node>(child->left)) {
          rotate_left<Node>(child, root);
        }
        rotate_right<Node>(node, root);
        node = node->parent;
        continue;
      }

      if (right_too_heavy(left, right)) {
        bst_node_base* const child = node->right;
        if (subtree_size_of<Node>(child->left) > subtree_size_of<Node>(child->right)) {
          rotate_right<Node>(child, root);
        }
        rotate_left<Node>(node, root);
        node = node->parent;
        continue;
      }

      return node;
    }
  }

  template <class Node>
  static constexpr void rebalance_ancestors(bst_node_base* node, bst_node_base& header) noexcept {
    bst_node_base*& root = header.parent;
    while (node != &header) {
      bst_node_base* const balanced = rebalance_at<Node>(node, root);
      node = balanced->parent;
    }
  }

  template <class Node>
  static constexpr void rotate_left(bst_node_base* x, bst_node_base*& root) noexcept {
    node_rotate_left(x, root);
    refresh<Node>(x);
    refresh<Node>(x->parent);
  }

  template <class Node>
  static constexpr void rotate_right(bst_node_base* x, bst_node_base*& root) noexcept {
    node_rotate_right(x, root);
    refresh<Node>(x);
    refresh<Node>(x->parent);
  }

  template <class Node>
  [[nodiscard]] static constexpr bool verify_node(const Node* node) noexcept {
    if (node == nullptr) {
      return true;
    }

    const std::size_t left = subtree_size_of<Node>(node->left);
    const std::size_t right = subtree_size_of<Node>(node->right);
    if (node->extension.subtree_size != 1 + left + right) {
      return false;
    }
    if (left_too_heavy(left, right) || right_too_heavy(left, right)) {
      return false;
    }
    return verify_node(static_cast<const Node*>(node->left)) && verify_node(static_cast<const Node*>(node->right));
  }
};

}  // namespace chaistl::detail::tree
