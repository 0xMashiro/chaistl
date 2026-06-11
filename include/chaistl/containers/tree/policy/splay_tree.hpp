// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/tree/node.hpp>

namespace chaistl::detail::tree {

// ============================================================================
// Splay Tree Policy
// ============================================================================
//
// Splay trees are self-adjusting binary search trees: every successful insert,
// erase target, and lookup access rotates the touched node to the root. The
// policy intentionally has no per-node metadata, which makes it a useful
// contrast to rb/avl/treap policies in the teaching API.
//
// The amortized complexity of search/insert/erase is O(log n), but individual
// operations can be linear. That trade-off is the point of the access hook:
// unlike RB/AVL/Treap, Splay is not purely an insertion/deletion balancing
// strategy.

struct splay_tree_policy {
  struct node_extension {};

  template <class Node>
  static constexpr void insert_and_rebalance(const bool insert_left,
                                             Node* x,
                                             bst_node_base* parent,
                                             bst_node_base& header) noexcept {
    node_link_leaf(insert_left, x, parent, header);
    splay<Node>(x, header);
  }

  template <class Node>
  static constexpr bst_node_base* unlink_and_rebalance(Node* z_node, bst_node_base& header) noexcept {
    bst_node_base* const z = z_node;
    splay<Node>(z, header);

    bst_node_base*& root = header.parent;
    bst_node_base* left = z->left;
    bst_node_base* right = z->right;

    z->left = nullptr;
    z->right = nullptr;
    z->parent = nullptr;

    if (left == nullptr) {
      root = right;
      if (root != nullptr) {
        root->parent = &header;
      }
    } else {
      root = left;
      root->parent = &header;

      bst_node_base* const left_max = node_maximum(left);
      splay<Node>(left_max, header);
      root->right = right;
      if (right != nullptr) {
        right->parent = root;
      }
    }

    refresh_header(header);
    return z;
  }

  template <class Node>
  constexpr void after_access(Node* node, bst_node_base& header) noexcept {
    splay<Node>(node, header);
  }

  template <class Node>
  [[nodiscard]] static constexpr bool verify(const Node*) noexcept {
    return true;
  }

 private:
  template <class Node>
  static constexpr void splay(bst_node_base* x, bst_node_base& header) noexcept {
    bst_node_base*& root = header.parent;
    while (x != root) {
      bst_node_base* const p = x->parent;
      bst_node_base* const g = p->parent;

      if (p == root) {
        if (x == p->left) {
          node_rotate_right(p, root);
        } else {
          node_rotate_left(p, root);
        }
      } else if (x == p->left && p == g->left) {
        node_rotate_right(g, root);
        node_rotate_right(p, root);
      } else if (x == p->right && p == g->right) {
        node_rotate_left(g, root);
        node_rotate_left(p, root);
      } else if (x == p->right && p == g->left) {
        node_rotate_left(p, root);
        node_rotate_right(g, root);
      } else {
        node_rotate_right(p, root);
        node_rotate_left(g, root);
      }
    }
    root->parent = &header;
  }

  static constexpr void refresh_header(bst_node_base& header) noexcept {
    if (header.parent == nullptr) {
      header.left = &header;
      header.right = &header;
      return;
    }
    header.parent->parent = &header;
    header.left = node_minimum(header.parent);
    header.right = node_maximum(header.parent);
  }
};

}  // namespace chaistl::detail::tree
