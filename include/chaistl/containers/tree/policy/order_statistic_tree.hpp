// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/tree/node.hpp>
#include <chaistl/containers/tree/policy/detail/splitmix64.hpp>

#include <cstddef>
#include <cstdint>

namespace chaistl::detail::tree {

// ============================================================================
// Order-Statistic Tree Balance Policy
// ============================================================================
//
// Treap-balanced BST augmented with subtree sizes. The random priority keeps
// expected height logarithmic; subtree_size turns the same tree into a rank
// structure, enabling find_by_order(k) and order_of_key(key) in O(log n)
// expected time from the container facade.
//
// This policy deliberately owns its rotations instead of using the generic
// node_rotate_left/right helpers: size metadata must be recomputed exactly at
// each rotation boundary.
// ============================================================================

struct order_statistic_tree_policy {
  struct node_extension {
    std::uint64_t priority;
    std::size_t subtree_size;
  };

  template <class Node>
  constexpr void insert_and_rebalance(const bool insert_left,
                                      Node* x,
                                      bst_node_base* parent,
                                      bst_node_base& header) noexcept {
    bst_node_base*& root = header.parent;

    priority_ref<Node>(x) = next_priority();
    subtree_size_ref<Node>(x) = 1;
    node_link_leaf(insert_left, x, parent, header);
    refresh_ancestors<Node>(x, &header);

    bst_node_base* n = x;
    while (n != root && priority_of<Node>(n) < priority_of<Node>(n->parent)) {
      bst_node_base* p = n->parent;
      if (n == p->left) {
        rotate_right<Node>(p, root);
      } else {
        rotate_left<Node>(p, root);
      }
    }
  }

  template <class Node>
  constexpr bst_node_base* unlink_and_rebalance(Node* z_node, bst_node_base& header) noexcept {
    bst_node_base* const z = z_node;
    bst_node_base*& root = header.parent;
    bst_node_base*& leftmost = header.left;
    bst_node_base*& rightmost = header.right;

    while (z->left != nullptr || z->right != nullptr) {
      if (z->left == nullptr) {
        rotate_left<Node>(z, root);
      } else if (z->right == nullptr) {
        rotate_right<Node>(z, root);
      } else if (priority_of<Node>(z->left) < priority_of<Node>(z->right)) {
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
      refresh_ancestors<Node>(parent, &header);
    }

    return z;
  }

  template <class Node>
  [[nodiscard]] static constexpr bool verify(const Node* root) noexcept {
    return verify_node(root);
  }

 private:
  template <class Node>
  [[nodiscard]] static constexpr std::uint64_t& priority_ref(bst_node_base* node) noexcept {
    return static_cast<Node*>(node)->extension.priority;
  }

  template <class Node>
  [[nodiscard]] static constexpr std::uint64_t priority_of(const bst_node_base* node) noexcept {
    return static_cast<const Node*>(node)->extension.priority;
  }

  template <class Node>
  [[nodiscard]] static constexpr std::size_t& subtree_size_ref(bst_node_base* node) noexcept {
    return static_cast<Node*>(node)->extension.subtree_size;
  }

  template <class Node>
  [[nodiscard]] static constexpr std::size_t subtree_size_of(const bst_node_base* node) noexcept {
    return node == nullptr ? 0 : static_cast<const Node*>(node)->extension.subtree_size;
  }

  template <class Node>
  static constexpr void refresh(Node* node) noexcept {
    node->extension.subtree_size = 1 + subtree_size_of<Node>(node->left) + subtree_size_of<Node>(node->right);
  }

  template <class Node>
  static constexpr void refresh_ancestors(bst_node_base* node, const bst_node_base* header) noexcept {
    while (node != header) {
      refresh(static_cast<Node*>(node));
      node = node->parent;
    }
  }

  template <class Node>
  static constexpr void rotate_left(bst_node_base* x, bst_node_base*& root) noexcept {
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

    refresh(static_cast<Node*>(x));
    refresh(static_cast<Node*>(y));
  }

  template <class Node>
  static constexpr void rotate_right(bst_node_base* x, bst_node_base*& root) noexcept {
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

    refresh(static_cast<Node*>(x));
    refresh(static_cast<Node*>(y));
  }

  [[nodiscard]] constexpr std::uint64_t next_priority() noexcept { return rng_.next(); }

  template <class Node>
  [[nodiscard]] static constexpr bool verify_node(const Node* node) noexcept {
    if (node == nullptr) {
      return true;
    }
    const std::uint64_t priority = node->extension.priority;
    const std::size_t expected_size = 1 + subtree_size_of<Node>(node->left) + subtree_size_of<Node>(node->right);
    if (node->extension.subtree_size != expected_size) {
      return false;
    }
    if (node->left != nullptr) {
      if (static_cast<const Node*>(node->left)->extension.priority < priority) {
        return false;
      }
      if (!verify_node(static_cast<const Node*>(node->left))) {
        return false;
      }
    }
    if (node->right != nullptr) {
      if (static_cast<const Node*>(node->right)->extension.priority < priority) {
        return false;
      }
      if (!verify_node(static_cast<const Node*>(node->right))) {
        return false;
      }
    }
    return true;
  }

  splitmix64 rng_{};
};

}  // namespace chaistl::detail::tree
