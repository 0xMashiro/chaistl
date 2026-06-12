// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace chaistl::detail::heap {

// ============================================================================
// The left-child / next-sibling forest
//
// Every node-based heap in this library (pairing, binomial, and future
// policies such as thin or rank-pairing heaps) shares one node shape, the
// same one GCC's pb_ds uses in left_child_next_sibling_heap_:
//
//        prev_or_parent                first_child
//   ┌──────────────────► P ◄─────────────────────┐
//   │                                            │
//   A ──next_sibling──► B ──next_sibling──► ... (A is P's first child)
//   ▲                   │
//   └───prev_or_parent──┘
//
// A node of arbitrary degree is encoded with exactly three pointers:
//
//   - first_child:    head of this node's child list.
//   - next_sibling:   the next node in whatever list this node lives in
//                     (a child list, or the top-level root list).
//   - prev_or_parent: the *back* edge. For a first child it points to the
//                     parent; for every other list member it points to the
//                     previous sibling. The forest head has nullptr.
//
// The overloaded back edge is the load-bearing trick: it makes every list
// doubly linked (so removal is O(1)) while still spending only three
// pointers per node. Disambiguation is one comparison:
//
//   n is a first child  ⇔  n->prev_or_parent->first_child == n
//
// A heap is a *forest*: `root` is the head of a top-level sibling list of
// trees. A pairing heap keeps exactly one tree in that list; a binomial
// heap keeps one per binary digit of size(). All ownership code below
// (destroy, clone, traverse) is written against the forest invariant and
// therefore works for every policy — single trees are just forests of one.
// ============================================================================

/// Extension placeholder for policies that need no per-node state.
/// Injected via [[no_unique_address]], so it costs zero bytes.
struct no_extension {};

/**
 * @brief Node of a left-child/next-sibling heap forest.
 *
 * @tparam T         Element type. Stored by value, constructed in place.
 * @tparam Extension Per-node policy state (e.g. a binomial node's degree).
 *
 * The node is deliberately *not* default-constructible unless T is: heap
 * algorithms in this library never materialize dummy nodes, so no operation
 * ever requires T to be default-constructible.
 */
template <class T, class Extension = no_extension>
struct heap_node {
  using value_type = T;
  using extension_type = Extension;

  heap_node* prev_or_parent = nullptr;
  heap_node* next_sibling = nullptr;
  heap_node* first_child = nullptr;
  [[no_unique_address]] Extension extension{};
  T value;

  template <class... Args>
    requires std::constructible_from<T, Args...>
  explicit constexpr heap_node(Args&&... args) : value(std::forward<Args>(args)...) {}
};

// ============================================================================
// Link primitives
//
// Pure pointer surgery: every function here is noexcept and O(1). Policies
// compose heap algorithms from these; the exception-safety story of the
// whole heap layer rests on that split (see heap-policy-design.md):
// comparisons may throw, links may not, and policies only compare while
// every node is still reachable from the forest.
// ============================================================================

/**
 * @brief Unlink @p node from whatever sibling list it occupies.
 *
 * Works uniformly for all three positions a node can be in — this is the
 * payoff of the overloaded back edge:
 *
 *   - forest head:        root advances to the next tree,
 *   - first child:        the parent's first_child advances,
 *   - any later sibling:  the previous sibling's next_sibling advances,
 *
 * and in every case the successor's back edge is repaired with the same
 * single assignment, because "what my predecessor was to me" is exactly
 * what it becomes to my successor. @p node keeps its children.
 */
template <class T, class E>
constexpr void unlink_from_list(heap_node<T, E>* node, heap_node<T, E>*& root) noexcept {
  heap_node<T, E>* prev = node->prev_or_parent;
  heap_node<T, E>* next = node->next_sibling;
  if (prev == nullptr) {
    root = next;
  } else if (prev->first_child == node) {
    prev->first_child = next;
  } else {
    prev->next_sibling = next;
  }
  if (next != nullptr) {
    next->prev_or_parent = prev;
  }
  node->prev_or_parent = nullptr;
  node->next_sibling = nullptr;
}

/**
 * @brief Unlink @p node from a list it is known not to head.
 *
 * Overload for callers working inside a child list (the node provably has a
 * predecessor), where no root pointer is in scope.
 *
 * @pre node->prev_or_parent != nullptr
 */
template <class T, class E>
constexpr void unlink_from_list(heap_node<T, E>* node) noexcept {
  heap_node<T, E>* prev = node->prev_or_parent;
  heap_node<T, E>* next = node->next_sibling;
  if (prev->first_child == node) {
    prev->first_child = next;
  } else {
    prev->next_sibling = next;
  }
  if (next != nullptr) {
    next->prev_or_parent = prev;
  }
  node->prev_or_parent = nullptr;
  node->next_sibling = nullptr;
}

/**
 * @brief Prepend detached @p child to @p parent's child list.
 */
template <class T, class E>
constexpr void link_first_child(heap_node<T, E>* child, heap_node<T, E>* parent) noexcept {
  child->prev_or_parent = parent;
  child->next_sibling = parent->first_child;
  if (parent->first_child != nullptr) {
    parent->first_child->prev_or_parent = child;
  }
  parent->first_child = child;
}

/**
 * @brief Link detached tree @p node at the front of the forest's root list.
 */
template <class T, class E>
constexpr void link_root_front(heap_node<T, E>* node, heap_node<T, E>*& root) noexcept {
  node->prev_or_parent = nullptr;
  node->next_sibling = root;
  if (root != nullptr) {
    root->prev_or_parent = node;
  }
  root = node;
}

/**
 * @brief Let detached @p replacement take over @p node's exact list position.
 *
 * Afterwards @p node is fully unlinked (children untouched).
 */
template <class T, class E>
constexpr void replace_in_list(heap_node<T, E>* node, heap_node<T, E>* replacement, heap_node<T, E>*& root) noexcept {
  heap_node<T, E>* prev = node->prev_or_parent;
  heap_node<T, E>* next = node->next_sibling;
  replacement->prev_or_parent = prev;
  replacement->next_sibling = next;
  if (prev == nullptr) {
    root = replacement;
  } else if (prev->first_child == node) {
    prev->first_child = replacement;
  } else {
    prev->next_sibling = replacement;
  }
  if (next != nullptr) {
    next->prev_or_parent = replacement;
  }
  node->prev_or_parent = nullptr;
  node->next_sibling = nullptr;
}

// ============================================================================
// Forest traversal
//
// Heap trees are not balanced: a pairing heap that received ascending pushes
// is a single depth-n chain. Recursive traversal would overflow the stack on
// large heaps, so everything below is iterative with O(1) extra space — the
// back edges double as the traversal stack.
// ============================================================================

/**
 * @brief Parent of @p n, or nullptr when @p n sits in the root list.
 *
 * Walks the back edges to the front of @p n's sibling list; the front
 * element's back edge is the parent. O(position in list), and O(1) amortized
 * across a full traversal, because each list is only ever walked off once.
 */
template <class Node>
[[nodiscard]] constexpr Node* parent_of(Node* n) noexcept {
  Node* p = n->prev_or_parent;
  while (p != nullptr && p->first_child != n) {
    n = p;
    p = n->prev_or_parent;
  }
  return p;
}

/**
 * @brief Visit every node of the forest exactly once, in preorder.
 *
 * Iterative, O(n) time, O(1) space; @p visit receives a reference to each
 * node. Const-correct via deduction: pass a `const Node*` root to visit
 * const nodes. The visitor must not modify links.
 */
template <class Node, class Visitor>
constexpr void for_each_node(Node* root, Visitor&& visit) {
  Node* n = root;
  while (n != nullptr) {
    visit(*n);
    if (n->first_child != nullptr) {
      n = n->first_child;
      continue;
    }
    while (n != nullptr && n->next_sibling == nullptr) {
      n = parent_of(n);
    }
    if (n != nullptr) {
      n = n->next_sibling;
    }
  }
}

/**
 * @brief Check the doubly-linked invariant of every list in the forest.
 *
 * Shared structural half of every policy's verify(): each forward edge must
 * be mirrored by the successor's back edge, and the forest head must have a
 * null back edge. Policies add their own order/shape checks on top.
 */
template <class Node>
[[nodiscard]] constexpr bool links_consistent(const Node* root) noexcept {
  if (root != nullptr && root->prev_or_parent != nullptr) {
    return false;
  }
  bool ok = true;
  for_each_node(root, [&](const Node& n) {
    if (n.first_child != nullptr && n.first_child->prev_or_parent != &n) {
      ok = false;
    }
    if (n.next_sibling != nullptr && n.next_sibling->prev_or_parent != &n) {
      ok = false;
    }
  });
  return ok;
}

/// Number of nodes in the forest. O(n); for verification only.
template <class Node>
[[nodiscard]] constexpr std::size_t count_nodes(const Node* root) noexcept {
  std::size_t n = 0;
  for_each_node(root, [&](const auto&) {
    ++n;
  });
  return n;
}

// ============================================================================
// Point iterators
//
// A point iterator is a stable *handle* to one element, the pb_ds concept of
// the same name: it stays valid until that element is erased, no matter what
// else happens to the heap. It is not a range iterator — there is no ++,
// because a heap has no useful element order to walk.
//
// Dereferencing always yields a const reference, even through the mutable
// handle: heap order owns the stored values, so in-place mutation must go
// through priority_queue::modify(), which re-establishes the invariant.
// The mutable/const distinction only controls whether the handle can be
// passed to modify()/erase().
// ============================================================================

template <class Node>
class heap_point_iterator {
 public:
  using value_type = std::remove_const_t<typename Node::value_type>;
  using reference = const value_type&;
  using pointer = const value_type*;

  constexpr heap_point_iterator() noexcept = default;
  explicit constexpr heap_point_iterator(Node* node) noexcept : node_(node) {}

  /// Mutable-to-const handle conversion (mirrors iterator → const_iterator).
  template <class MutableNode>
    requires std::same_as<Node, const MutableNode>
  constexpr heap_point_iterator(heap_point_iterator<MutableNode> other) noexcept : node_(other.node()) {}

  [[nodiscard]] constexpr reference operator*() const noexcept { return node_->value; }
  [[nodiscard]] constexpr pointer operator->() const noexcept { return std::addressof(node_->value); }

  [[nodiscard]] constexpr Node* node() const noexcept { return node_; }

  [[nodiscard]] friend constexpr bool operator==(heap_point_iterator, heap_point_iterator) noexcept = default;

 private:
  Node* node_ = nullptr;
};

}  // namespace chaistl::detail::heap
