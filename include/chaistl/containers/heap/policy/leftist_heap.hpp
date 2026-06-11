// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/heap/node.hpp>

#include <cstddef>
#include <utility>

namespace chaistl::heap_policy {

// ============================================================================
// leftist_heap_policy — meldable heap with a null-path-length invariant
//
// A leftist heap is a heap-ordered binary tree. Each node stores the null path
// length (NPL) of its subtree, and meld keeps the shorter null path on the
// right. That makes the right spine logarithmic, so all structural operations
// reduce to a short meld along that spine.
//
//   insert       O(log n)   meld a singleton with the root
//   top          O(1)       the root
//   join         O(log n)   meld two roots
//   extract_top  O(log n)   meld the root's two children
//
// Like skew_heap_policy, this policy models node_heap_policy but not
// mutable_node_heap_policy. Stable arbitrary erase/modify handles remain the
// job of pairing_heap_policy and binomial_heap_policy.
// ============================================================================

struct leftist_heap_policy {
  struct node_extension {
    std::size_t null_path_length = 1;
  };

  template <class Node, class Compare>
  static constexpr void insert(Node* node, Node*& root, const Compare& cmp) {
    node->extension.null_path_length = 1;
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
    if (root != nullptr) {
      root->prev_or_parent = nullptr;
      root->next_sibling = nullptr;
    }
    old_root->prev_or_parent = nullptr;
    old_root->next_sibling = nullptr;
    old_root->first_child = nullptr;
    old_root->extension.null_path_length = 1;
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
      if (npl(left) < npl(right) || n.extension.null_path_length != npl(right) + 1) {
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
  template <class Node>
  [[nodiscard]] static constexpr std::size_t npl(const Node* node) noexcept {
    return node == nullptr ? 0 : node->extension.null_path_length;
  }

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
    right = meld(right, b, cmp);
    if (npl(left) < npl(right)) {
      std::swap(left, right);
    }
    set_children(a, left, right);
    a->extension.null_path_length = npl(right) + 1;
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
