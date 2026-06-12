// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/heap/node.hpp>

#include <cstddef>
#include <utility>

namespace chaistl::heap_policy {

// ============================================================================
// binomial_heap_policy — the binary-counter heap
//
// A binomial heap is a forest of *binomial trees*: B₀ is a single node, and
// Bₖ is two Bₖ₋₁'s, one hung under the other. A Bₖ therefore has exactly 2ᵏ
// nodes, and its root has children Bₖ₋₁ … B₀ (newest first). A heap of n
// elements keeps one tree per set bit of n — inserting is incrementing a
// binary counter, and the carry is "meld two equal-degree trees" (one
// comparison, one link).
//
//   insert       O(1) amortized    prepend B₀, propagate carries
//   top          O(log n)          scan the root list
//   extract_top  O(log n)          unlink the max root, fold its children in
//   join         O(log n)          zip two root lists, propagate carries
//   detach       O(log² n) worst   bubble the node to its tree root, then
//                                  remove it like a maximum
//
// Per-node state: the tree degree, declared as `node_extension` and injected
// into heap_node by the container (this is what node_extension_of detects).
//
// Root-list invariant — and the exception story built on it:
//   canonical:  degrees strictly increase along the root list.
//   relaxed:    degrees never decrease (equal neighbors allowed).
// Every operation *accepts* the relaxed form and *produces* the canonical
// form, because each ends with the same carry pass (coalesce). A comparator
// that throws mid-carry leaves the relaxed form — a perfectly valid heap
// that the next operation finishes normalizing. The invariant absorbs the
// failure instead of every operation having to unwind it.
//
// Arbitrary removal (detach, the mutable_node_heap_policy capability) must
// not move values between nodes — handle stability tracks the *node*. So an
// interior node is bubbled to its tree root by structural parent/child
// swaps (pb_ds's swap_with_parent idea on this exact layout) and then
// removed like a maximum. Two observations make this clean:
//
//   - Degrees are positional. A Bₖ stays a Bₖ if the two nodes trade
//     positions AND degree fields: the slot keeps the degree, the nodes
//     trade places. Only-first-child-points-up makes the trade O(1) — no
//     child list is re-parented, at most six neighbor edges re-aim.
//   - The bubble needs no comparisons: the node is leaving anyway. After
//     every swap, all heap-order violations have the leaving node as the
//     parent, so once it is removed (or re-linked as a childless B₀ by the
//     extract rollback) the remaining forest is fully heap-ordered again.
//
// The only comparisons in detach are the final carry pass — which makes
// detach strong for free, by the same rollback extract_top uses. The
// O(log² n) worst case comes from walking back edges to find each parent
// (the price of three-pointer nodes; pb_ds pays the same walk).
// ============================================================================

struct binomial_heap_policy {
  /// Degree = number of children = the k of this node's Bₖ subtree.
  /// (Bounded by log₂ of the address space, so even a uint8_t would do;
  /// size_t keeps the layout unremarkable.)
  struct node_extension {
    std::size_t degree = 0;
  };

  /**
   * @brief Link a detached singleton @p node: increment the binary counter.
   *
   * Once the noexcept prepend has happened the node is owned by the forest;
   * a comparator throw during the carry pass only delays canonicalization.
   *
   * @pre @p node is detached and childless.
   */
  template <class Node, class Compare>
  static constexpr void insert(Node* node, Node*& root, const Compare& cmp) {
    node->extension.degree = 0;
    detail::heap::link_root_front(node, root);  // degree 0 in front keeps the list sorted
    coalesce(root, cmp);
  }

  /// Scan the root list — the maximum of a heap forest is some tree's root.
  template <class Node, class Compare>
  [[nodiscard]] static constexpr const Node* top(const Node* root, const Compare& cmp) {
    return find_top(root, cmp);
  }

  /**
   * @brief Unlink and return the maximum root.
   *
   * Its children — binomial trees of degrees k-1 … 0, i.e. exactly a
   * smaller heap — are reversed into root-list order and zipped back in.
   * Strong guarantee: should the final carry pass throw, the extracted
   * node is re-linked as a B₀ singleton (its children were already zipped
   * away, so degree 0 is the truth) and the heap holds every element again.
   */
  template <class Node, class Compare>
  [[nodiscard]] static constexpr Node* extract_top(Node*& root, const Compare& cmp) {
    Node* m = find_top(root, cmp);
    extract_root(m, root);
    try {
      coalesce(root, cmp);
    } catch (...) {
      detail::heap::link_root_front(m, root);  // undo: nothing was extracted after all
      throw;
    }
    return m;
  }

  /**
   * @brief Unlink an *arbitrary* @p node; returns it detached and childless.
   *
   * Comparison-free structural bubble to the tree root (see the header
   * block), then the same removal as extract_top. Strong guarantee: the
   * only comparisons are the final carry pass, and its rollback re-links
   * the node — by then a childless B₀ — at the front of the root list,
   * where the heap invariant holds in full.
   */
  template <class Node, class Compare>
  static constexpr Node* detach(Node* node, Node*& root, const Compare& cmp) {
    while (Node* parent = detail::heap::parent_of(node)) {
      swap_with_parent(node, parent, root);
    }
    extract_root(node, root);
    try {
      coalesce(root, cmp);
    } catch (...) {
      detail::heap::link_root_front(node, root);
      throw;
    }
    return node;
  }

  /**
   * @brief Absorb the forest @p other: binary addition of two counters.
   *
   * The noexcept zip transfers ownership of every node of @p other before
   * the first comparison can throw.
   */
  template <class Node, class Compare>
  static constexpr void join(Node*& root, Node* other, const Compare& cmp) {
    if (other == nullptr) {
      return;
    }
    root = zip_by_degree(root, other);
    coalesce(root, cmp);
  }

  /// Links, (relaxed) root-list degree order, and exact Bₖ shape of every
  /// tree. Tests only.
  template <class Node, class Compare>
  [[nodiscard]] static constexpr bool verify(const Node* root, const Compare& cmp) {
    if (!detail::heap::links_consistent(root)) {
      return false;
    }
    for (const Node* r = root; r != nullptr; r = r->next_sibling) {
      if (r->next_sibling != nullptr && r->next_sibling->extension.degree < r->extension.degree) {
        return false;
      }
    }
    bool ok = true;
    detail::heap::for_each_node(root, [&](const Node& n) {
      std::size_t want = n.extension.degree;
      for (const Node* c = n.first_child; c != nullptr; c = c->next_sibling) {
        if (want == 0) {  // more children than the degree claims
          ok = false;
          return;
        }
        --want;
        if (c->extension.degree != want) {  // children must be B(k-1) … B(0)
          ok = false;
          return;
        }
        if (cmp(n.value, c->value)) {  // heap order
          ok = false;
          return;
        }
      }
      if (want != 0) {  // fewer children than the degree claims
        ok = false;
      }
    });
    return ok;
  }

 private:
  /**
   * @brief Trade places (and degrees) with the parent. Noexcept, O(1).
   *
   * All old edges are read into locals before any write, then the two nodes'
   * link triples are exchanged and the at-most-six neighbors re-aim their
   * edges. Because only a first child points up, no child list needs
   * re-parenting; the back-edge disambiguation (`first_child == x`) is the
   * same one unlink_from_list relies on.
   */
  template <class Node>
  static constexpr void swap_with_parent(Node* node, Node* parent, Node*& root) noexcept {
    Node* const grand = parent->prev_or_parent;  // back edge: parent's parent OR its left neighbor
    Node* const parent_next = parent->next_sibling;
    Node* const node_prev = node->prev_or_parent;  // == parent iff node is the first child
    Node* const node_next = node->next_sibling;
    Node* const parent_first = parent->first_child;
    Node* const node_first = node->first_child;
    const bool node_is_first = parent_first == node;

    // node takes parent's slot ...
    node->prev_or_parent = grand;
    node->next_sibling = parent_next;
    node->first_child = node_is_first ? parent : parent_first;

    // ... parent takes node's slot ...
    parent->prev_or_parent = node_is_first ? node : node_prev;
    parent->next_sibling = node_next;
    parent->first_child = node_first;

    // ... and the neighbors re-aim.
    if (grand == nullptr) {
      root = node;
    } else if (grand->first_child == parent) {
      grand->first_child = node;
    } else {
      grand->next_sibling = node;
    }
    if (parent_next != nullptr) {
      parent_next->prev_or_parent = node;
    }
    if (!node_is_first) {
      parent_first->prev_or_parent = node;  // the first child now hangs off node
      node_prev->next_sibling = parent;     // node's left sibling now precedes parent
    }
    if (node_next != nullptr) {
      node_next->prev_or_parent = parent;
    }
    if (node_first != nullptr) {
      node_first->prev_or_parent = parent;
    }

    // Degrees are positional: the slot keeps the degree, the nodes trade.
    std::swap(node->extension.degree, parent->extension.degree);
  }

  /**
   * @brief Unlink root-list member @p node and fold its children back in.
   *
   * Pure pointer surgery, noexcept. Leaves @p node detached, childless, and
   * degree 0; leaves the root list in the relaxed (nondecreasing) form,
   * pending one coalesce.
   */
  template <class Node>
  static constexpr void extract_root(Node* node, Node*& root) noexcept {
    detail::heap::unlink_from_list(node, root);

    // Strip the children (degrees k-1 … 0) and reverse: a root list (0 … k-1).
    Node* reversed = nullptr;
    Node* child = node->first_child;
    node->first_child = nullptr;
    node->extension.degree = 0;
    while (child != nullptr) {
      Node* next = child->next_sibling;
      child->prev_or_parent = nullptr;
      child->next_sibling = reversed;
      if (reversed != nullptr) {
        reversed->prev_or_parent = child;
      }
      reversed = child;
      child = next;
    }

    root = zip_by_degree(root, reversed);
  }

  template <class Node, class Compare>
  [[nodiscard]] static constexpr Node* find_top(Node* root, const Compare& cmp) {
    Node* best = root;
    for (Node* cur = root->next_sibling; cur != nullptr; cur = cur->next_sibling) {
      if (cmp(best->value, cur->value)) {
        best = cur;
      }
    }
    return best;
  }

  /**
   * @brief Merge two degree-sorted root lists into one. Pure pointer relinking —
   * no comparator, noexcept; this is what makes join's ownership transfer
   * unconditional. Output is sorted in the relaxed (nondecreasing) sense.
   */
  template <class Node>
  [[nodiscard]] static constexpr Node* zip_by_degree(Node* a, Node* b) noexcept {
    Node* head = nullptr;
    Node** out = &head;
    Node* prev = nullptr;
    while (a != nullptr && b != nullptr) {
      Node*& source = (a->extension.degree <= b->extension.degree) ? a : b;
      Node* n = source;
      source = source->next_sibling;
      n->prev_or_parent = prev;
      *out = n;
      out = &n->next_sibling;
      prev = n;
    }
    Node* rest = (a != nullptr) ? a : b;
    *out = rest;  // the remainder's internal back edges are already right
    if (rest != nullptr) {
      rest->prev_or_parent = prev;
    }
    return head;
  }

  /**
   * @brief The carry pass: meld adjacent equal-degree trees, left to right.
   *
   * One subtlety, straight from CLRS's BINOMIAL-HEAP-UNION: with *three*
   * equal degrees in a row [Bₖ Bₖ Bₖ], melding the first pair would put a
   * Bₖ₊₁ in front of a Bₖ and unsort the list — so step past the first and
   * let the later pair meld. The winner of a meld is always the tree left
   * sitting in the list (the loser is unlinked from under it), so the
   * list stays sorted and every comparison happens between linked nodes.
   */
  template <class Node, class Compare>
  static constexpr void coalesce(Node*& root, const Compare& cmp) {
    Node* cur = root;
    while (cur != nullptr && cur->next_sibling != nullptr) {
      Node* next = cur->next_sibling;
      Node* after = next->next_sibling;
      const bool mergeable = cur->extension.degree == next->extension.degree &&
                             (after == nullptr || after->extension.degree != cur->extension.degree);
      if (!mergeable) {
        cur = next;
        continue;
      }
      if (cmp(cur->value, next->value)) {
        detail::heap::unlink_from_list(cur, root);
        detail::heap::link_first_child(cur, next);
        ++next->extension.degree;
        cur = next;
      } else {
        detail::heap::unlink_from_list(next, root);
        detail::heap::link_first_child(next, cur);
        ++cur->extension.degree;
        // stay on cur: its new degree may match the new neighbor's
      }
    }
  }
};

}  // namespace chaistl::heap_policy
