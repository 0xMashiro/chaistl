// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/tree/node.hpp>

#include <cstdint>
#include <utility>

namespace chaistl::detail::tree {

// ============================================================================
// AVL Tree Balance Policy
// ============================================================================

/**
 * @brief Balance policy implementing AVL tree invariants.
 *
 * Maintains the AVL property: for every node, the heights of the two child
 * subtrees differ by at most one. Each node stores its balance factor
 *
 *   balance = height(right subtree) - height(left subtree)  ∈ {-1, 0, +1}
 *
 * in @c node_extension. Compared to the red-black policy, AVL trees are
 * more rigidly balanced (height <= ~1.44 log2 n vs ~2 log2 n), trading
 * cheaper lookups for potentially more rotations on mutation — exactly the
 * kind of trade-off the policy seam exists to benchmark.
 *
 * The structural contract is identical to @ref rb_tree_policy (see the
 * @c balance_policy concept): link/unlink maintain the header's
 * root/leftmost/rightmost pointers, and the balance factor — like the
 * red-black color — is written explicitly before use because node
 * extensions are never default-initialized.
 *
 * The algorithms follow the classic balance-factor formulation
 * (Knuth TAOCP 6.2.3; the retracing loops match the presentation on
 * Wikipedia's "AVL tree" page), adapted to the shared header-sentinel
 * layout and the successor-splice erase used by the red-black policy.
 */
struct avl_tree_policy {
  // ========================================================================
  // Per-node state
  // ========================================================================

  // Deliberately no default member initializer: the tree creates nodes
  // without starting the node object's lifetime, so an initializer would
  // never run (see the balance_policy lifetime contract).
  // insert_and_rebalance writes the balance before the node is linked.
  struct node_extension {
    std::int8_t balance;
  };

  // ========================================================================
  // Insert
  // ========================================================================

  /**
   * @brief Link the detached node @p x into the tree and restore invariants.
   *
   * After linking, the retrace walks upward from the new leaf. At each
   * ancestor the balance factor absorbs the growth: a node that becomes
   * perfectly balanced (0) stops the walk (its height did not change), a
   * node leaning one deeper (±1) propagates, and a node that would reach
   * ±2 is repaired by one single or double rotation — which always
   * restores the subtree's original height, so the walk then stops.
   *
   * @tparam Node The concrete bst_node instantiation (provides the balance).
   */
  template <class Node>
  static constexpr void insert_and_rebalance(const bool insert_left,
                                             Node* x,
                                             bst_node_base* parent,
                                             bst_node_base& header) noexcept {
    bst_node_base*& root = header.parent;

    node_link_leaf(insert_left, x, parent, header);
    balance_of<Node>(x) = 0;

    // Retrace: n is the root of the subtree that grew one level.
    bst_node_base* n = x;
    bst_node_base* p = n->parent;
    while (p != &header) {
      std::int8_t& bf = balance_of<Node>(p);
      if (n == p->right) {
        if (bf > 0) {
          // Right-right or right-left violation; n is the grown child, so
          // its own lean picks the rotation. Either repair restores the
          // pre-insert height: stop.
          if (balance_of<Node>(n) < 0) {
            promote_right_left<Node>(p, root);
          } else {
            promote_right<Node>(p, root);
          }
          break;
        }
        if (bf < 0) {
          bf = 0;  // growth absorbed by the shallower side
          break;
        }
        bf = 1;
      } else {
        if (bf < 0) {
          if (balance_of<Node>(n) > 0) {
            promote_left_right<Node>(p, root);
          } else {
            promote_left<Node>(p, root);
          }
          break;
        }
        if (bf > 0) {
          bf = 0;
          break;
        }
        bf = -1;
      }
      n = p;
      p = n->parent;
    }
  }

  // ========================================================================
  // Erase
  // ========================================================================

  /**
   * @brief Unlink @p z from the tree, restore invariants, and return the
   *        node that must be destroyed (always @p z).
   *
   * The structural splice is identical to the red-black policy: a node
   * with two children is replaced by its in-order successor (which takes
   * over the balance factor, as it takes over the position), so iterators
   * and references to other elements stay valid.
   *
   * The retrace then walks up from the parent of the spliced-out
   * position. Deletion is the asymmetric case: a rotation repairs the
   * local violation but may itself shrink the subtree by one level, so
   * unlike insertion the walk can cascade all the way to the root. The
   * shrunk side cannot be recovered from child pointers when the removed
   * child slot is null, so it is tracked explicitly.
   *
   * @tparam Node The concrete bst_node instantiation (provides the balance).
   */
  template <class Node>
  static constexpr bst_node_base* unlink_and_rebalance(Node* z_node, bst_node_base& header) noexcept {
    bst_node_base* const z = z_node;
    bst_node_base*& root = header.parent;
    bst_node_base*& leftmost = header.left;
    bst_node_base*& rightmost = header.right;

    // y: the node spliced out of its position
    //    (z itself, or z's successor when z has two children).
    // x: y's only possible child, promoted into y's position (may be null).
    bst_node_base* y = z;
    bst_node_base* x = nullptr;
    bst_node_base* x_parent = nullptr;
    bool left_shrunk = false;

    if (y->left == nullptr) {  // z has at most one (right) child
      x = y->right;
    } else if (y->right == nullptr) {  // z has exactly one (left) child
      x = y->left;
    } else {  // two children: splice out the successor instead
      y = node_minimum(y->right);
      x = y->right;
    }

    if (y != z) {
      // Relink y in place of z. y is z's successor: it has no left child.
      z->left->parent = y;
      y->left = z->left;
      if (y != z->right) {
        x_parent = y->parent;
        left_shrunk = true;  // y was the minimum: always a left child
        if (x != nullptr) {
          x->parent = y->parent;
        }
        y->parent->left = x;
        y->right = z->right;
        z->right->parent = y;
      } else {
        x_parent = y;
        left_shrunk = false;  // y moved up: its right subtree lost a level
      }

      if (root == z) {
        root = y;
      } else if (z->parent->left == z) {
        z->parent->left = y;
      } else {
        z->parent->right = y;
      }
      y->parent = z->parent;
      std::swap(balance_of<Node>(y), balance_of<Node>(z));
      // From here on, "the removed balance" is z's (swapped) state.
    } else {
      // z had at most one child: promote it.
      x_parent = z->parent;
      left_shrunk = root != z && z->parent->left == z;
      if (x != nullptr) {
        x->parent = z->parent;
      }
      if (root == z) {
        root = x;
      } else if (z->parent->left == z) {
        z->parent->left = x;
      } else {
        z->parent->right = x;
      }

      // z was an extreme only if it had no child on that side; the new
      // extreme is then z's parent (the header itself when z was the
      // last node, restoring the empty-tree state).
      if (leftmost == z) {
        leftmost = z->right == nullptr ? z->parent : node_minimum(x);
      }
      if (rightmost == z) {
        rightmost = z->left == nullptr ? z->parent : node_maximum(x);
      }
    }

    // Retrace: the subtree under x_parent on the recorded side lost one
    // level. Stop as soon as a subtree's overall height is unchanged.
    bst_node_base* n = x_parent;
    while (n != &header) {
      bst_node_base* const parent = n->parent;
      // Rotations at n keep the rotated subtree in the same child slot of
      // parent, so the side recorded here stays valid for the next step.
      const bool n_was_left = parent->left == n;
      std::int8_t& bf = balance_of<Node>(n);
      bool height_decreased = true;

      if (left_shrunk) {
        if (bf == 0) {
          bf = 1;  // now leaning right, height unchanged
          height_decreased = false;
        } else if (bf < 0) {
          bf = 0;  // was leaning left, now balanced; height decreased
        } else {
          // bf == +1 and the left side shrank further: +2 violation.
          // Unlike insertion the sibling may be perfectly balanced
          // (zb == 0), in which case the rotation does not shrink the
          // subtree and the walk stops.
          const std::int8_t zb = balance_of<Node>(n->right);
          if (zb < 0) {
            promote_right_left<Node>(n, root);
          } else {
            promote_right<Node>(n, root);
            height_decreased = zb != 0;
          }
        }
      } else {
        if (bf == 0) {
          bf = -1;
          height_decreased = false;
        } else if (bf > 0) {
          bf = 0;
        } else {
          const std::int8_t zb = balance_of<Node>(n->left);
          if (zb > 0) {
            promote_left_right<Node>(n, root);
          } else {
            promote_left<Node>(n, root);
            height_decreased = zb != 0;
          }
        }
      }

      if (!height_decreased) {
        break;
      }
      n = parent;
      left_shrunk = n_was_left;
    }

    return z;
  }

  // ========================================================================
  // Invariant validation (for testing/debugging)
  // ========================================================================

  /**
   * @brief Check the AVL invariants of the subtree at @p root.
   *
   * Recomputes every subtree height and verifies that each stored balance
   * factor matches the recomputed difference and stays within ±1.
   * Structural soundness (BST ordering, parent links, header consistency)
   * is checked by binary_search_tree::validate().
   */
  template <class Node>
  [[nodiscard]] static constexpr bool verify(const Node* root) noexcept {
    return subtree_height(root) >= 0;
  }

 private:
  /**
   * @brief Access the balance factor stored in a node's policy extension.
   *
   * The structural algorithms walk @c bst_node_base pointers; only balance
   * reads/writes need the concrete node type, so the downcast is kept in
   * this one place. Never called with the header (which is a plain
   * @c bst_node_base and has no balance).
   */
  template <class Node>
  [[nodiscard]] static constexpr std::int8_t& balance_of(bst_node_base* node) noexcept {
    return static_cast<Node*>(node)->extension.balance;
  }

  /**
   * @brief Repair a +2 (right-right) violation at @p x by a left rotation.
   *
   * Precondition: balance(x) == +2 conceptually and z = x->right does not
   * lean left. zb == 0 only occurs during deletion; the rotated subtree
   * then keeps its height (the caller stops retracing).
   */
  template <class Node>
  static constexpr void promote_right(bst_node_base* x, bst_node_base*& root) noexcept {
    bst_node_base* const z = x->right;
    const std::int8_t zb = balance_of<Node>(z);
    node_rotate_left(x, root);
    if (zb == 0) {
      balance_of<Node>(x) = 1;
      balance_of<Node>(z) = -1;
    } else {
      balance_of<Node>(x) = 0;
      balance_of<Node>(z) = 0;
    }
  }

  /// Mirror of @ref promote_right for a -2 (left-left) violation.
  template <class Node>
  static constexpr void promote_left(bst_node_base* x, bst_node_base*& root) noexcept {
    bst_node_base* const z = x->left;
    const std::int8_t zb = balance_of<Node>(z);
    node_rotate_right(x, root);
    if (zb == 0) {
      balance_of<Node>(x) = -1;
      balance_of<Node>(z) = 1;
    } else {
      balance_of<Node>(x) = 0;
      balance_of<Node>(z) = 0;
    }
  }

  /**
   * @brief Repair a +2 violation whose right child leans left
   *        (right-left shape) by a double rotation.
   *
   * y = x->right->left becomes the subtree root; the balance factors of
   * x and z are determined by which of y's sides carried the extra level.
   */
  template <class Node>
  static constexpr void promote_right_left(bst_node_base* x, bst_node_base*& root) noexcept {
    bst_node_base* const z = x->right;
    bst_node_base* const y = z->left;
    const std::int8_t yb = balance_of<Node>(y);
    node_rotate_right(z, root);
    node_rotate_left(x, root);
    balance_of<Node>(x) = yb > 0 ? -1 : 0;
    balance_of<Node>(z) = yb < 0 ? 1 : 0;
    balance_of<Node>(y) = 0;
  }

  /// Mirror of @ref promote_right_left for a left-right shape.
  template <class Node>
  static constexpr void promote_left_right(bst_node_base* x, bst_node_base*& root) noexcept {
    bst_node_base* const z = x->left;
    bst_node_base* const y = z->right;
    const std::int8_t yb = balance_of<Node>(y);
    node_rotate_left(z, root);
    node_rotate_right(x, root);
    balance_of<Node>(x) = yb < 0 ? 1 : 0;
    balance_of<Node>(z) = yb > 0 ? -1 : 0;
    balance_of<Node>(y) = 0;
  }

  /**
   * @brief Height of the subtree at @p node, or -1 if an AVL invariant is
   *        violated. An empty subtree has height 0.
   */
  template <class Node>
  [[nodiscard]] static constexpr int subtree_height(const Node* node) noexcept {
    if (node == nullptr) {
      return 0;
    }
    const int left = subtree_height(static_cast<const Node*>(node->left));
    const int right = subtree_height(static_cast<const Node*>(node->right));
    if (left < 0 || right < 0) {
      return -1;
    }
    const int diff = right - left;
    if (diff < -1 || diff > 1 || diff != node->extension.balance) {
      return -1;
    }
    return 1 + (left > right ? left : right);
  }
};

}  // namespace chaistl::detail::tree
