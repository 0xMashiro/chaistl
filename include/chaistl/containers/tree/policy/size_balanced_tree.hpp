// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/tree/node.hpp>

#include <cstddef>

namespace chaistl::detail::tree {

// ============================================================================
// Size-Balanced Tree Policy
// ============================================================================
//
// Size Balanced Tree (SBT) stores subtree sizes and repairs a node when a
// grandchild subtree outgrows the opposite child subtree:
//
//   size(left.left)   <= size(right)
//   size(left.right)  <= size(right)
//   size(right.left)  <= size(left)
//   size(right.right) <= size(left)
//
// This is stricter and more structural than the ratio-based
// weight_balanced_tree_policy. It is common in competitive-programming and
// Chinese data-structure teaching material, making it a useful bridge between
// textbook AVL/RB trees and augmentation-driven order-statistic trees.
// ============================================================================

struct size_balanced_tree_policy {
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

  template <class Node>
  static constexpr bool needs_left_left_rotation(const bst_node_base* node) noexcept {
    return subtree_size_of<Node>(node->left != nullptr ? node->left->left : nullptr) >
           subtree_size_of<Node>(node->right);
  }

  template <class Node>
  static constexpr bool needs_left_right_rotation(const bst_node_base* node) noexcept {
    return subtree_size_of<Node>(node->left != nullptr ? node->left->right : nullptr) >
           subtree_size_of<Node>(node->right);
  }

  template <class Node>
  static constexpr bool needs_right_right_rotation(const bst_node_base* node) noexcept {
    return subtree_size_of<Node>(node->right != nullptr ? node->right->right : nullptr) >
           subtree_size_of<Node>(node->left);
  }

  template <class Node>
  static constexpr bool needs_right_left_rotation(const bst_node_base* node) noexcept {
    return subtree_size_of<Node>(node->right != nullptr ? node->right->left : nullptr) >
           subtree_size_of<Node>(node->left);
  }

  template <class Node>
  static constexpr bst_node_base* maintain(bst_node_base* node, bst_node_base*& root) noexcept {
    if (node == nullptr) {
      return nullptr;
    }
    refresh<Node>(node);

    bool changed = false;
    if (needs_left_left_rotation<Node>(node)) {
      rotate_right<Node>(node, root);
      node = node->parent;
      changed = true;
    } else if (needs_left_right_rotation<Node>(node)) {
      rotate_left<Node>(node->left, root);
      rotate_right<Node>(node, root);
      node = node->parent;
      changed = true;
    } else if (needs_right_right_rotation<Node>(node)) {
      rotate_left<Node>(node, root);
      node = node->parent;
      changed = true;
    } else if (needs_right_left_rotation<Node>(node)) {
      rotate_right<Node>(node->right, root);
      rotate_left<Node>(node, root);
      node = node->parent;
      changed = true;
    }

    if (!changed) {
      return node;
    }

    maintain<Node>(node->left, root);
    maintain<Node>(node->right, root);
    return maintain<Node>(node, root);
  }

  template <class Node>
  static constexpr void rebalance_ancestors(bst_node_base* node, bst_node_base& header) noexcept {
    bst_node_base*& root = header.parent;
    while (node != &header) {
      bst_node_base* const balanced = maintain<Node>(node, root);
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
    if (node->extension.subtree_size != 1 + subtree_size_of<Node>(node->left) + subtree_size_of<Node>(node->right)) {
      return false;
    }
    if (needs_left_left_rotation<Node>(node) || needs_left_right_rotation<Node>(node) ||
        needs_right_right_rotation<Node>(node) || needs_right_left_rotation<Node>(node)) {
      return false;
    }
    return verify_node(static_cast<const Node*>(node->left)) && verify_node(static_cast<const Node*>(node->right));
  }
};

}  // namespace chaistl::detail::tree
