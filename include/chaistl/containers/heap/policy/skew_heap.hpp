// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/heap/node.hpp>

#include <utility>

namespace chaistl::heap_policy {

// ============================================================================
// skew_heap_policy — the minimal self-adjusting meldable heap
//
// A skew heap is a heap-ordered binary tree whose balance is maintained only
// by meld(): after recursively melding the right subtree with the other heap,
// the two children are swapped. There is no rank, degree, or null-path-length
// metadata, so the node extension is empty.
//
//   insert       O(log n) amortized   meld a singleton with the root
//   top          O(1)                 the root
//   join         O(log n) amortized   meld two roots
//   extract_top  O(log n) amortized   meld the root's two children
//
// This first version intentionally models node_heap_policy, not
// mutable_node_heap_policy: erase(it) and modify(it) are left to pairing and
// binomial heaps, whose arbitrary detach algorithms are already designed and
// tested. That keeps the teaching surface focused on skew's one primitive.
// ============================================================================

struct skew_heap_policy {
  template <class Node, class Compare>
  static constexpr void insert(Node* node, Node*& root, const Compare& cmp) {
    join(root, node, cmp);
  }

  template <class Node, class Compare>
  [[nodiscard]] static constexpr const Node* top(const Node* root, const Compare&) noexcept {
    return root;
  }

  template <class Node, class Compare>
  [[nodiscard]] static constexpr Node* extract_top(Node*& root, const Compare& cmp) {
    Node* old_root = root;
    auto [left, right] = detach_children(old_root);
    root = meld(left, right, cmp);
    old_root->prev_or_parent = nullptr;
    old_root->next_sibling = nullptr;
    old_root->first_child = nullptr;
    return old_root;
  }

  template <class Node, class Compare>
  static constexpr void join(Node*& root, Node* other, const Compare& cmp) {
    if (other == nullptr) {
      return;
    }
    if (root == nullptr) {
      root = other;
      root->prev_or_parent = nullptr;
      root->next_sibling = nullptr;
      return;
    }
    try {
      root = meld(root, other, cmp);
    } catch (...) {
      detail::heap::link_first_child(other, root);  // absorb, then report
      throw;
    }
    root->prev_or_parent = nullptr;
    root->next_sibling = nullptr;
  }

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
      const Node* left = n.first_child;
      const Node* right = left != nullptr ? left->next_sibling : nullptr;
      if (right != nullptr && right->next_sibling != nullptr) {
        ok = false;
      }
      for (const Node* child = left; child != nullptr; child = child->next_sibling) {
        if (cmp(n.value, child->value)) {
          ok = false;
        }
      }
    });
    return ok;
  }

 private:
  template <class Node, class Compare>
  [[nodiscard]] static constexpr Node* meld(Node* a, Node* b, const Compare& cmp) {
    if (a == nullptr) {
      return b;
    }
    if (b == nullptr) {
      return a;
    }
    if (cmp(a->value, b->value)) {
      std::swap(a, b);
    }

    auto [left, right] = detach_children(a);
    Node* merged = meld(right, b, cmp);
    set_children(a, merged, left);
    return a;
  }

  template <class Node>
  [[nodiscard]] static constexpr std::pair<Node*, Node*> detach_children(Node* node) noexcept {
    Node* left = node->first_child;
    Node* right = left != nullptr ? left->next_sibling : nullptr;
    node->first_child = nullptr;
    if (left != nullptr) {
      left->prev_or_parent = nullptr;
      left->next_sibling = nullptr;
    }
    if (right != nullptr) {
      right->prev_or_parent = nullptr;
      right->next_sibling = nullptr;
    }
    return {left, right};
  }

  template <class Node>
  static constexpr void set_children(Node* parent, Node* left, Node* right) noexcept {
    parent->first_child = left;
    if (left != nullptr) {
      left->prev_or_parent = parent;
      left->next_sibling = right;
    }
    if (right != nullptr) {
      right->prev_or_parent = left;
      right->next_sibling = nullptr;
    }
  }
};

}  // namespace chaistl::heap_policy
