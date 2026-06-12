// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/heap/node.hpp>

namespace chaistl::heap_policy {

// ============================================================================
// pairing_heap_policy — the practical mutable heap
//
// A pairing heap is a single heap-ordered tree of arbitrary shape. It is the
// structure pb_ds reaches for by default when handles are needed, because
// the constant factors are excellent and every operation is a composition
// of one primitive: *meld* (hang the smaller of two roots under the larger).
//
//   insert       O(1)            meld a singleton with the root
//   top          O(1)            the root
//   join         O(1)            meld two roots
//   extract_top  O(log n) am.    two-pass meld of the root's children
//   detach       O(log n) am.    two-pass meld of the node's children
//
// This policy needs no per-node state (no node_extension — the detection
// trait substitutes the empty placeholder), and models
// mutable_node_heap_policy: erase(it) and modify(it, v) are available.
//
// The two-pass pairing of Fredman/Sedgewick/Sleator/Tarjan is usually
// implemented by unlinking children onto a temporary list. Here the back
// edges (prev_or_parent, node.hpp) let both passes run *in place* inside
// the child list, which buys the exception story for free: the comparator
// is only ever invoked on two nodes that are still linked into the forest,
// so a throwing comparator aborts the operation with every node still owned
// and the heap invariant intact — nothing was removed, nothing leaks.
// ============================================================================

struct pairing_heap_policy {
  /**
   * @brief Link a detached singleton @p node into the heap.
   *
   * Ownership transfers unconditionally (the insert contract): if the
   * comparison throws, the node is still absorbed — hung under the root,
   * which is the memory-safe recovery when the comparator forfeited its
   * chance to say which of the two is larger.
   *
   * @pre @p node is detached and childless.
   */
  template <class Node, class Compare>
  static constexpr void insert(Node* node, Node*& root, const Compare& cmp) {
    if (root == nullptr) {
      root = node;
      return;
    }
    bool node_wins = false;
    try {
      node_wins = static_cast<bool>(cmp(root->value, node->value));
    } catch (...) {
      detail::heap::link_first_child(node, root);  // absorb, then report
      throw;
    }
    if (node_wins) {
      // node is the new maximum: the old tree becomes its first child.
      Node* old_root = root;
      root = node;
      detail::heap::link_first_child(old_root, node);
    } else {
      detail::heap::link_first_child(node, root);
    }
  }

  /// The maximum is always the root of the single tree.
  template <class Node, class Compare>
  [[nodiscard]] static constexpr const Node* top(const Node* root, const Compare&) noexcept {
    return root;
  }

  template <class Node, class Compare>
  [[nodiscard]] static constexpr Node* extract_top(Node*& root, const Compare& cmp) {
    return detach(root, root, cmp);
  }

  /**
   * @brief Unlink an arbitrary @p node; returns it detached and childless.
   *
   * Condenses the node's children into at most one tree (the two passes),
   * then lets that tree take over the node's exact position in its list.
   * That is heap-legal without any further comparison: the condensed root
   * is one of node's former children, so it is ≤ node ≤ node's parent.
   *
   * Strong guarantee: phases 1–2 only compare nodes that remain linked; the
   * relinking in phase 3 is pure pointer surgery.
   */
  template <class Node, class Compare>
  static constexpr Node* detach(Node* node, Node*& root, const Compare& cmp) {
    pair_up_children(node, cmp);
    accumulate_children(node, cmp);
    Node* condensed = node->first_child;
    if (condensed != nullptr) {
      node->first_child = nullptr;
      condensed->prev_or_parent = nullptr;
      detail::heap::replace_in_list(node, condensed, root);
    } else {
      detail::heap::unlink_from_list(node, root);
    }
    return node;
  }

  /**
   * @brief Absorb the detached single tree @p other into @p root. O(1).
   *
   * On a comparator throw the other tree is still absorbed (hung under
   * root), so ownership transfers unconditionally; see the policy concept's
   * join contract.
   */
  template <class Node, class Compare>
  static constexpr void join(Node*& root, Node* other, const Compare& cmp) {
    if (other == nullptr) {
      return;
    }
    if (root == nullptr) {
      root = other;
      return;
    }
    bool other_wins = false;
    try {
      other_wins = static_cast<bool>(cmp(root->value, other->value));
    } catch (...) {
      detail::heap::link_first_child(other, root);  // absorb, then report
      throw;
    }
    if (other_wins) {
      Node* old_root = root;
      root = other;
      detail::heap::link_first_child(old_root, other);
    } else {
      detail::heap::link_first_child(other, root);
    }
  }

  /// Single tree, consistent links, every child ≤ its parent. Tests only.
  template <class Node, class Compare>
  [[nodiscard]] static constexpr bool verify(const Node* root, const Compare& cmp) {
    if (root == nullptr) {
      return true;
    }
    if (root->prev_or_parent != nullptr || root->next_sibling != nullptr) {
      return false;
    }
    if (!detail::heap::links_consistent(root)) {
      return false;
    }
    bool ok = true;
    detail::heap::for_each_node(root, [&](const Node& n) {
      for (const Node* c = n.first_child; c != nullptr; c = c->next_sibling) {
        if (cmp(n.value, c->value)) {
          ok = false;
        }
      }
    });
    return ok;
  }

 private:
  // Meld adjacent pairs of `parent`'s children, left to right. The loser of
  // each loser is unlinked and hung under the winner, so the winner is
  // left occupying the pair's position in the child list — the list shrinks
  // in place and stays well-formed at every comparison point.
  template <class Node, class Compare>
  static constexpr void pair_up_children(Node* parent, const Compare& cmp) {
    Node* a = parent->first_child;
    while (a != nullptr && a->next_sibling != nullptr) {
      Node* b = a->next_sibling;
      Node* rest = b->next_sibling;
      Node* loser = cmp(a->value, b->value) ? a : b;
      Node* winner = (loser == a) ? b : a;
      detail::heap::unlink_from_list(loser);  // never the forest head: it has a predecessor
      detail::heap::link_first_child(loser, winner);
      a = rest;
    }
  }

  // Fold the surviving children right to left — the textbook second pass.
  // A singly linked list cannot be walked backwards, which is why classic
  // implementations build a reversed temporary list in pass one; the back
  // edges make the backward walk direct.
  template <class Node, class Compare>
  static constexpr void accumulate_children(Node* parent, const Compare& cmp) {
    Node* tail = parent->first_child;
    if (tail == nullptr) {
      return;
    }
    while (tail->next_sibling != nullptr) {
      tail = tail->next_sibling;
    }
    while (parent->first_child != tail) {
      Node* prev = tail->prev_or_parent;  // a sibling: tail is not the first child
      Node* loser = cmp(prev->value, tail->value) ? prev : tail;
      Node* winner = (loser == prev) ? tail : prev;
      detail::heap::unlink_from_list(loser);
      detail::heap::link_first_child(loser, winner);
      tail = winner;  // the winner now holds the folded position
    }
  }
};

}  // namespace chaistl::heap_policy
