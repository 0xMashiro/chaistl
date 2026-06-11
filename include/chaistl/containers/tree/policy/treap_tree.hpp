// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/tree/node.hpp>
#include <chaistl/containers/tree/policy/detail/splitmix64.hpp>

#include <cstdint>
#include <limits>
#include <utility>

namespace chaistl::detail::tree {

// ============================================================================
// Treap Tree Balance Policy
// ============================================================================
//
// Architecture:
//   - Implements the balance_policy concept (binary_search_tree.hpp)
//   - Each node carries a random priority; the tree is a binary search tree
//     by key and a heap by priority (min-heap: parent.priority <= child.priority)
//   - Policy instance owns the RNG state (splitmix64); priorities are generated
//     independently per insertion, so tree copy/clear do not affect correctness
//
// Algorithm source:
//   - Seidel & Aragon, "Randomized Search Trees" (J. Algorithms 1996)
//   - The insert rotate-up / erase rotate-down formulation follows
//     the textbook presentation in Cormen et al., Introduction to Algorithms
//
// References:
//   - Cormen et al., CLRS 3rd ed., Chapter 13 (treap problem)
//   - Wikipedia: https://en.wikipedia.org/wiki/Treap
//   - ADR-004: Tree Policy-Based Design

struct treap_tree_policy {
  // ========================================================================
  // Per-node state
  // ========================================================================

  // Deliberately no default member initializer: the tree creates nodes
  // without starting the node object's lifetime, so an initializer would
  // never run (see the balance_policy lifetime contract).
  // insert_and_rebalance writes the priority before the node is linked.
  struct node_extension {
    std::uint64_t priority;
  };

  // ========================================================================
  // Insert
  // ========================================================================

  /**
   * @brief Link the detached node @p x into the tree and restore heap order.
   *
   * After linking as a leaf, the node is rotated up while its priority is
   * smaller than its parent's. Each rotation preserves BST order (the
   * rotated subtree's keys are unchanged) and restores the heap property
   * locally. The walk stops at the root or when the parent has lower priority.
   *
   * The header's root/leftmost/rightmost pointers are maintained by the
   * shared rotation primitives.
   *
   * @tparam Node The concrete bst_node instantiation (provides the priority).
   */
  template <class Node>
  constexpr void insert_and_rebalance(const bool insert_left,
                                      Node* x,
                                      bst_node_base* parent,
                                      bst_node_base& header) noexcept {
    bst_node_base*& root = header.parent;

    // Assign a random priority before linking. The node is not yet part of
    // the tree, so no invariant is violated during generation.
    priority_of<Node>(x) = next_priority();

    node_link_leaf(insert_left, x, parent, header);

    // Rotate up while the heap order is violated (min-heap).
    bst_node_base* n = x;
    while (n != root && priority_of<Node>(n) < priority_of<Node>(n->parent)) {
      bst_node_base* p = n->parent;
      if (n == p->left) {
        node_rotate_right(p, root);
      } else {
        node_rotate_left(p, root);
      }
    }
  }

  // ========================================================================
  // Erase
  // ========================================================================

  /**
   * @brief Unlink @p z from the tree and return the node to destroy.
   *
   * Treap deletion rotates the target down until it becomes a leaf, always
   * rotating toward the child with the smaller priority. This preserves both
   * the BST order and the heap property at every step. Once a leaf, the node
   * is unlinked directly.
   *
   * The structural splice for a node with two children is not needed here:
   * rotating down naturally moves the successor (or predecessor) up through
   * the rotations. The node that is physically removed is always @p z.
   *
   * The header's root/leftmost/rightmost pointers are maintained.
   *
   * @tparam Node The concrete bst_node instantiation (provides the priority).
   */
  template <class Node>
  constexpr bst_node_base* unlink_and_rebalance(Node* z_node, bst_node_base& header) noexcept {
    bst_node_base* const z = z_node;
    bst_node_base*& root = header.parent;
    bst_node_base*& leftmost = header.left;
    bst_node_base*& rightmost = header.right;

    // Rotate z down until it is a leaf.
    while (z->left != nullptr || z->right != nullptr) {
      if (z->left == nullptr) {
        node_rotate_left(z, root);
      } else if (z->right == nullptr) {
        node_rotate_right(z, root);
      } else if (priority_of<Node>(z->left) < priority_of<Node>(z->right)) {
        node_rotate_right(z, root);
      } else {
        node_rotate_left(z, root);
      }
    }

    // z is now a leaf: unlink it.
    if (z == root) {
      root = nullptr;
      leftmost = &header;
      rightmost = &header;
    } else {
      if (z->parent->left == z) {
        z->parent->left = nullptr;
      } else {
        z->parent->right = nullptr;
      }
      if (leftmost == z) {
        leftmost = z->parent;
      }
      if (rightmost == z) {
        rightmost = z->parent;
      }
    }

    return z;
  }

  // ========================================================================
  // Invariant validation (for testing/debugging)
  // ========================================================================

  /**
   * @brief Check the treap invariants of the subtree at @p root.
   *
   * Verifies the heap property (parent priority <= child priority) and
   * that priorities are within the generated range.
   * Structural soundness (BST ordering, parent links, header consistency)
   * is checked by binary_search_tree::validate().
   */
  template <class Node>
  [[nodiscard]] static constexpr bool verify(const Node* root) noexcept {
    return verify_heap(root);
  }

 private:
  // ========================================================================
  // Priority access
  // ========================================================================

  template <class Node>
  [[nodiscard]] static constexpr std::uint64_t& priority_of(bst_node_base* node) noexcept {
    return static_cast<Node*>(node)->extension.priority;
  }

  template <class Node>
  [[nodiscard]] static constexpr std::uint64_t priority_of(const bst_node_base* node) noexcept {
    return static_cast<const Node*>(node)->extension.priority;
  }

  [[nodiscard]] constexpr std::uint64_t next_priority() noexcept { return rng_.next(); }

  // ========================================================================
  // Heap verification
  // ========================================================================

  template <class Node>
  [[nodiscard]] static constexpr bool verify_heap(const Node* node) noexcept {
    if (node == nullptr) {
      return true;
    }
    const std::uint64_t p = node->extension.priority;
    if (node->left != nullptr) {
      if (static_cast<const Node*>(node->left)->extension.priority < p) {
        return false;
      }
      if (!verify_heap(static_cast<const Node*>(node->left))) {
        return false;
      }
    }
    if (node->right != nullptr) {
      if (static_cast<const Node*>(node->right)->extension.priority < p) {
        return false;
      }
      if (!verify_heap(static_cast<const Node*>(node->right))) {
        return false;
      }
    }
    return true;
  }

  // ========================================================================
  // Data members
  // ========================================================================

  splitmix64 rng_{};
};

}  // namespace chaistl::detail::tree
