// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/heap/node.hpp>

#include <array>
#include <cstddef>

namespace chaistl::heap_policy {

// ============================================================================
// fibonacci_heap_policy — lazy meldable heap with degree consolidation
//
// This is the non-handle half of a Fibonacci heap:
//   insert       O(1)         prepend a singleton root
//   join         O(r)         concatenate root lists (r = roots in other)
//   top          O(r)         scan roots; no cached max pointer in the policy
//   extract_top  O(r + log n) promote children, then consolidate by degree
//
// A textbook Fibonacci heap stores a cached min/max pointer and supports
// decrease-key through cascading cuts. chaistl's priority_queue policy layer
// currently has no per-container policy state and policies remain
// std::is_empty, so this policy does not cache a tail pointer for the root
// list. join therefore traverses the incoming root list tail and is O(roots),
// not the O(1) meld of a textbook stateful Fibonacci heap. The policy keeps
// the structural idea that is useful for teaching — lazy meld plus
// consolidation — while leaving stable handles to pairing_heap/binomial_heap.
//
// Exception discipline: consolidation compares two roots before either is
// unlinked. If the comparator throws, every node remains reachable from the
// forest. extract_top then rolls the removed maximum root back as a childless
// singleton, preserving content and heap order.
// ============================================================================

struct fibonacci_heap_policy {
  struct node_extension {
    std::size_t degree = 0;
  };

  template <class Node, class Compare>
  static constexpr void insert(Node* node, Node*& root, const Compare&) noexcept {
    node->extension.degree = 0;
    detail::heap::link_root_front(node, root);
  }

  template <class Node, class Compare>
  [[nodiscard]] static constexpr const Node* top(const Node* root, const Compare& cmp) {
    return find_top(root, cmp);
  }

  template <class Node, class Compare>
  [[nodiscard]] static constexpr Node* extract_top(Node*& root, const Compare& cmp) {
    Node* m = find_top(root, cmp);  // no mutation before the first comparison sweep completes
    detail::heap::unlink_from_list(m, root);
    promote_children(m, root);
    try {
      consolidate(root, cmp);
    } catch (...) {
      detail::heap::link_root_front(m, root);
      throw;
    }
    return m;
  }

  template <class Node, class Compare>
  static constexpr void join(Node*& root, Node* other, const Compare&) noexcept {
    if (other == nullptr) {
      return;
    }
    if (root == nullptr) {
      root = other;
      return;
    }
    Node* tail = other;
    while (tail->next_sibling != nullptr) {
      tail = tail->next_sibling;
    }
    tail->next_sibling = root;
    root->prev_or_parent = tail;
    root = other;
  }

  template <class Node, class Compare>
  [[nodiscard]] static constexpr bool verify(const Node* root, const Compare& cmp) {
    if (!detail::heap::links_consistent(root)) {
      return false;
    }
    bool ok = true;
    detail::heap::for_each_node(root, [&](const Node& n) {
      std::size_t degree = 0;
      for (const Node* c = n.first_child; c != nullptr; c = c->next_sibling) {
        ++degree;
        if (cmp(n.value, c->value)) {
          ok = false;
          return;
        }
      }
      if (degree != n.extension.degree) {
        ok = false;
      }
    });
    return ok;
  }

 private:
  static constexpr std::size_t max_degree = sizeof(std::size_t) * 8;

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

  template <class Node>
  static constexpr void promote_children(Node* node, Node*& root) noexcept {
    Node* children = node->first_child;
    node->first_child = nullptr;
    node->extension.degree = 0;
    if (children == nullptr) {
      return;
    }
    Node* prev = nullptr;
    Node* child = children;
    while (child != nullptr) {
      child->prev_or_parent = prev;
      prev = child;
      if (child->next_sibling == nullptr) {
        child->next_sibling = root;
        if (root != nullptr) {
          root->prev_or_parent = child;
        }
        root = children;
        return;
      }
      child = child->next_sibling;
    }
  }

  template <class Node, class Compare>
  static constexpr void consolidate(Node*& root, const Compare& cmp) {
    std::array<Node*, max_degree + 1> slots{};
    Node* cur = root;
    while (cur != nullptr) {
      Node* next = cur->next_sibling;
      std::size_t degree = cur->extension.degree;
      while (slots[degree] != nullptr && slots[degree] != cur) {
        Node* other = slots[degree];
        slots[degree] = nullptr;
        Node* winner = cmp(cur->value, other->value) ? other : cur;
        Node* loser = winner == cur ? other : cur;
        detail::heap::unlink_from_list(loser, root);
        detail::heap::link_first_child(loser, winner);
        winner->extension.degree = degree + 1;
        cur = winner;
        degree = cur->extension.degree;
      }
      slots[degree] = cur;
      cur = next;
    }
  }
};

}  // namespace chaistl::heap_policy
