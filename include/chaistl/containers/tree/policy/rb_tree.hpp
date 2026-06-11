// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/tree/node.hpp>

#include <utility>

namespace chaistl::detail::tree {

// ============================================================================
// Red-Black Tree Balance Policy
// ============================================================================

/**
 * @brief Balance policy implementing red-black tree invariants.
 *
 * Maintains the classic red-black properties:
 * 1. Every node is either red or black.
 * 2. The root is black.
 * 3. Every leaf (NIL) is black.
 * 4. If a node is red, both children are black.
 * 5. For each node, all simple paths from the node to descendant leaves
 *    contain the same number of black nodes.
 *
 * The policy injects a single color field into each node via
 * @c node_extension and owns the two structural mutations of the tree:
 *
 * - @ref insert_and_rebalance links a detached node under its parent and
 *   restores the invariants,
 * - @ref unlink_and_rebalance removes a node from the tree structure and
 *   restores the invariants.
 *
 * Both receive the header sentinel by reference. Root, leftmost and
 * rightmost live in the header (see @ref bst_node_base), so rotations can
 * update the root pointer in place — there is no "return the new root"
 * protocol that a caller could forget to honor.
 *
 * The algorithms are ports of libstdc++'s _Rb_tree_insert_and_rebalance /
 * _Rb_tree_rebalance_for_erase (gcc: libstdc++-v3/src/c++98/tree.cc),
 * which in turn follow CLRS chapter 13.
 */
struct rb_tree_policy {
  // ========================================================================
  // Per-node state
  // ========================================================================

  enum class rb_color : bool { red = false, black = true };

  // Deliberately no default member initializer: the tree creates nodes
  // without starting the node object's lifetime, so an initializer would
  // never run (see the balance_policy lifetime contract) — it would only
  // mislead readers into thinking nodes start red. insert_and_rebalance
  // writes the color before the node is linked.
  struct node_extension {
    rb_color color;
  };

  // ========================================================================
  // Insert
  // ========================================================================

  /**
   * @brief Link the detached node @p x into the tree and restore invariants.
   *
   * @p x must be freshly created (its links are overwritten here). It is
   * attached under @p parent on the side selected by @p insert_left; when
   * @p parent is the header the tree is empty and @p x becomes the root.
   * The header's root/leftmost/rightmost pointers are maintained.
   *
   * The fixup walks red-red violations up the tree. Reading a parent's
   * color is safe everywhere: the loop only inspects x->parent while
   * x != root, and only the root's parent is the (colorless) header.
   *
   * @tparam Node The concrete bst_node instantiation (provides the color).
   */
  template <class Node>
  static constexpr void insert_and_rebalance(const bool insert_left,
                                             Node* x,
                                             bst_node_base* parent,
                                             bst_node_base& header) noexcept {
    bst_node_base*& root = header.parent;

    // Link x under parent, maintaining header.{parent,left,right}.
    node_link_leaf(insert_left, x, parent, header);
    color_of<Node>(x) = rb_color::red;

    // Fixup: x is red; repair red-red violations upward.
    bst_node_base* n = x;
    while (n != root && color_of<Node>(n->parent) == rb_color::red) {
      bst_node_base* const grandparent = n->parent->parent;

      if (n->parent == grandparent->left) {
        bst_node_base* const uncle = grandparent->right;
        if (uncle != nullptr && color_of<Node>(uncle) == rb_color::red) {
          // Red uncle: recolor and continue from the grandparent.
          color_of<Node>(n->parent) = rb_color::black;
          color_of<Node>(uncle) = rb_color::black;
          color_of<Node>(grandparent) = rb_color::red;
          n = grandparent;
        } else {
          if (n == n->parent->right) {
            // Inner child: rotate to the outer (left-left) shape first.
            n = n->parent;
            node_rotate_left(n, root);
          }
          color_of<Node>(n->parent) = rb_color::black;
          color_of<Node>(grandparent) = rb_color::red;
          node_rotate_right(grandparent, root);
        }
      } else {
        // Mirror image: parent is a right child.
        bst_node_base* const uncle = grandparent->left;
        if (uncle != nullptr && color_of<Node>(uncle) == rb_color::red) {
          color_of<Node>(n->parent) = rb_color::black;
          color_of<Node>(uncle) = rb_color::black;
          color_of<Node>(grandparent) = rb_color::red;
          n = grandparent;
        } else {
          if (n == n->parent->left) {
            n = n->parent;
            node_rotate_right(n, root);
          }
          color_of<Node>(n->parent) = rb_color::black;
          color_of<Node>(grandparent) = rb_color::red;
          node_rotate_left(grandparent, root);
        }
      }
    }

    color_of<Node>(root) = rb_color::black;
  }

  // ========================================================================
  // Erase
  // ========================================================================

  /**
   * @brief Unlink @p z from the tree, restore invariants, and return the
   *        node that must be destroyed (always @p z).
   *
   * When @p z has two children its in-order successor is relinked into
   * @p z's position (taking over @p z's color) instead of moving the
   * value — iterators and references to other elements stay valid, as the
   * standard requires. The header's root/leftmost/rightmost pointers are
   * maintained, including the transition to the empty tree.
   *
   * If the node structurally removed was black, the black-height deficit
   * is repaired by the double-black fixup walking @c child / @c child_parent
   * upward (@c child may be null — its parent is tracked separately).
   *
   * @tparam Node The concrete bst_node instantiation (provides the color).
   */
  template <class Node>
  static constexpr bst_node_base* unlink_and_rebalance(Node* z_node, bst_node_base& header) noexcept {
    bst_node_base* const z = z_node;
    bst_node_base*& root = header.parent;
    bst_node_base*& leftmost = header.left;
    bst_node_base*& rightmost = header.right;

    // y: the node to splice out of its position
    //    (z itself, or z's successor when z has two children).
    // x: y's only possible child, promoted into y's position (may be null).
    bst_node_base* y = z;
    bst_node_base* x = nullptr;
    bst_node_base* x_parent = nullptr;

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
        if (x != nullptr) {
          x->parent = y->parent;
        }
        y->parent->left = x;  // y was the minimum: always a left child
        y->right = z->right;
        z->right->parent = y;
      } else {
        x_parent = y;
      }

      if (root == z) {
        root = y;
      } else if (z->parent->left == z) {
        z->parent->left = y;
      } else {
        z->parent->right = y;
      }
      y->parent = z->parent;
      std::swap(color_of<Node>(y), color_of<Node>(z));
      // From here on, "the removed color" is z's (swapped) color.
    } else {
      // z had at most one child: promote it.
      x_parent = y->parent;
      if (x != nullptr) {
        x->parent = y->parent;
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

    // A removed red node never changes black heights.
    if (color_of<Node>(z) == rb_color::black) {
      // x carries a black-height deficit ("double black") until it is
      // either recolored, compensated by a rotation, or reaches the root.
      while (x != root && (x == nullptr || color_of<Node>(x) == rb_color::black)) {
        if (x == x_parent->left) {
          // The sibling exists: the path through the removed black node
          // had black height >= 1, so the other side cannot be empty.
          bst_node_base* sibling = x_parent->right;

          if (color_of<Node>(sibling) == rb_color::red) {
            // Case 1: red sibling — rotate to get a black sibling.
            color_of<Node>(sibling) = rb_color::black;
            color_of<Node>(x_parent) = rb_color::red;
            node_rotate_left(x_parent, root);
            sibling = x_parent->right;
          }

          if ((sibling->left == nullptr || color_of<Node>(sibling->left) == rb_color::black) &&
              (sibling->right == nullptr || color_of<Node>(sibling->right) == rb_color::black)) {
            // Case 2: black sibling, black nephews — push the deficit up.
            color_of<Node>(sibling) = rb_color::red;
            x = x_parent;
            x_parent = x_parent->parent;
          } else {
            if (sibling->right == nullptr || color_of<Node>(sibling->right) == rb_color::black) {
              // Case 3: near nephew red — rotate it into the far slot.
              color_of<Node>(sibling->left) = rb_color::black;
              color_of<Node>(sibling) = rb_color::red;
              node_rotate_right(sibling, root);
              sibling = x_parent->right;
            }
            // Case 4: far nephew red — one rotation absorbs the deficit.
            color_of<Node>(sibling) = color_of<Node>(x_parent);
            color_of<Node>(x_parent) = rb_color::black;
            if (sibling->right != nullptr) {
              color_of<Node>(sibling->right) = rb_color::black;
            }
            node_rotate_left(x_parent, root);
            break;
          }
        } else {
          // Mirror image: x is a right child.
          bst_node_base* sibling = x_parent->left;

          if (color_of<Node>(sibling) == rb_color::red) {
            color_of<Node>(sibling) = rb_color::black;
            color_of<Node>(x_parent) = rb_color::red;
            node_rotate_right(x_parent, root);
            sibling = x_parent->left;
          }

          if ((sibling->right == nullptr || color_of<Node>(sibling->right) == rb_color::black) &&
              (sibling->left == nullptr || color_of<Node>(sibling->left) == rb_color::black)) {
            color_of<Node>(sibling) = rb_color::red;
            x = x_parent;
            x_parent = x_parent->parent;
          } else {
            if (sibling->left == nullptr || color_of<Node>(sibling->left) == rb_color::black) {
              color_of<Node>(sibling->right) = rb_color::black;
              color_of<Node>(sibling) = rb_color::red;
              node_rotate_left(sibling, root);
              sibling = x_parent->left;
            }
            color_of<Node>(sibling) = color_of<Node>(x_parent);
            color_of<Node>(x_parent) = rb_color::black;
            if (sibling->left != nullptr) {
              color_of<Node>(sibling->left) = rb_color::black;
            }
            node_rotate_right(x_parent, root);
            break;
          }
        }
      }
      if (x != nullptr) {
        color_of<Node>(x) = rb_color::black;
      }
    }

    return z;
  }

  // ========================================================================
  // Invariant validation (for testing/debugging)
  // ========================================================================

  /**
   * @brief Check the red-black color invariants of the subtree at @p root.
   *
   * Verifies properties 2 (black root), 4 (no red-red edges) and 5 (equal
   * black heights). Structural soundness (BST ordering, parent links,
   * header consistency) is checked by binary_search_tree::validate(),
   * which has access to the comparator and the header.
   */
  template <class Node>
  [[nodiscard]] static constexpr bool verify(const Node* root) noexcept {
    if (root == nullptr) {
      return true;
    }
    if (root->extension.color != rb_color::black) {
      return false;
    }
    return black_height(root) >= 0;
  }

 private:
  /**
   * @brief Access the color stored in the policy extension of a node.
   *
   * The structural algorithms walk @c bst_node_base pointers; only color
   * reads/writes need the concrete node type, so the downcast is kept in
   * this one place. Never called with the header (which is a plain
   * @c bst_node_base and has no color).
   */
  template <class Node>
  [[nodiscard]] static constexpr rb_color& color_of(bst_node_base* node) noexcept {
    return static_cast<Node*>(node)->extension.color;
  }

  /**
   * @brief Black height of the subtree at @p node, or -1 if invalid.
   *
   * Checks properties 4 and 5 recursively. NIL counts as one black node.
   */
  template <class Node>
  [[nodiscard]] static constexpr int black_height(const Node* node) noexcept {
    if (node == nullptr) {
      return 1;
    }

    if (node->extension.color == rb_color::red) {
      if (node->left != nullptr && static_cast<const Node*>(node->left)->extension.color == rb_color::red) {
        return -1;
      }
      if (node->right != nullptr && static_cast<const Node*>(node->right)->extension.color == rb_color::red) {
        return -1;
      }
    }

    const int left_height = black_height(static_cast<const Node*>(node->left));
    const int right_height = black_height(static_cast<const Node*>(node->right));
    if (left_height < 0 || right_height < 0 || left_height != right_height) {
      return -1;
    }
    return left_height + (node->extension.color == rb_color::black ? 1 : 0);
  }
};

}  // namespace chaistl::detail::tree
