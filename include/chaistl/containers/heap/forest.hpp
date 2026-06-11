// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/heap/node.hpp>
#include <chaistl/memory/detail/lifetime/exception_guard.hpp>

#include <memory>
#include <type_traits>
#include <utility>

namespace chaistl::detail::heap {

// ============================================================================
// node_forest — RAII ownership of a heap forest
//
// The heap layer splits three responsibilities across three parties:
//
//   - node_forest (this type) owns MEMORY: node lifetime, the allocator,
//     and whole-forest copy/move/destroy with the usual POCCA/POCMA/POCS
//     allocator choreography.
//   - the policy owns STRUCTURE: how trees are linked and reshaped.
//   - priority_queue owns MEANING: the comparator, the public API, and the
//     choreography between the other two.
//
// Because node_forest is a regular RAII value, priority_queue gets its
// copy/move/destroy special members for free (= default), and constructor
// bodies that push elements are exception-safe with no explicit guards:
// if a push throws, the half-built forest member is destroyed normally.
//
// This is an internal building block (compare libstdc++'s _Rb_tree_impl):
// `root` and `count` are public state, and keeping them coherent during an
// operation is the caller's job. priority_queue is the encapsulation
// boundary that users see.
// ============================================================================

template <class T, class Allocator, class Extension>
class node_forest {
 public:
  using node_type = heap_node<T, Extension>;
  using value_allocator_traits = std::allocator_traits<Allocator>;
  using node_allocator_type = typename value_allocator_traits::template rebind_alloc<node_type>;
  using node_allocator_traits = std::allocator_traits<node_allocator_type>;
  using size_type = typename value_allocator_traits::size_type;

  /// Head of the top-level sibling list of trees. nullptr ⇔ empty.
  node_type* root = nullptr;
  /// Number of nodes in the forest.
  size_type count = 0;

  // ==========================================================================
  // Construction / destruction
  // ==========================================================================

  constexpr node_forest() noexcept(std::is_nothrow_default_constructible_v<node_allocator_type>) = default;

  explicit constexpr node_forest(const Allocator& alloc) noexcept : alloc_(alloc) {}

  constexpr node_forest(const node_forest& other)
      : alloc_(node_allocator_traits::select_on_container_copy_construction(other.alloc_)) {
    adopt_clone_of(other);
  }

  constexpr node_forest(const node_forest& other, const Allocator& alloc) : alloc_(alloc) { adopt_clone_of(other); }

  constexpr node_forest(node_forest&& other) noexcept
      : root(std::exchange(other.root, nullptr)),
        count(std::exchange(other.count, 0)),
        alloc_(std::move(other.alloc_)) {}

  // Allocator-extended move: steal when the allocators can free each other's
  // memory, otherwise rebuild node by node, moving the values across.
  // Either way `other` ends up empty — one postcondition, two costs.
  constexpr node_forest(node_forest&& other, const Allocator& alloc) : alloc_(alloc) {
    if (alloc_ == other.alloc_) {
      root = std::exchange(other.root, nullptr);
      count = std::exchange(other.count, 0);
    } else {
      adopt_clone_of(std::move(other));
      other.clear();
    }
  }

  constexpr ~node_forest() { clear(); }

  constexpr node_forest& operator=(const node_forest& other) {
    if (this == &other) {
      return *this;
    }
    // Free with the old allocator *before* POCCA replaces it — after the
    // swap we could no longer legally deallocate the old nodes.
    clear();
    if constexpr (node_allocator_traits::propagate_on_container_copy_assignment::value) {
      alloc_ = other.alloc_;
    }
    adopt_clone_of(other);
    return *this;
  }

  constexpr node_forest& operator=(node_forest&& other) noexcept(
      node_allocator_traits::propagate_on_container_move_assignment::value ||
      node_allocator_traits::is_always_equal::value) {
    if (this == &other) {
      return *this;
    }
    clear();
    if constexpr (node_allocator_traits::propagate_on_container_move_assignment::value) {
      alloc_ = std::move(other.alloc_);
      steal(other);
    } else if (alloc_ == other.alloc_) {
      steal(other);
    } else {
      adopt_clone_of(std::move(other));
      other.clear();
    }
    return *this;
  }

  constexpr void swap(node_forest& other) noexcept {
    using std::swap;
    swap(root, other.root);
    swap(count, other.count);
    if constexpr (node_allocator_traits::propagate_on_container_swap::value) {
      swap(alloc_, other.alloc_);
    }
    // Otherwise allocators must compare equal ([container.reqmts]); swapping
    // unequal non-propagating allocators is undefined, as for std containers.
  }

  // ==========================================================================
  // Node lifetime
  // ==========================================================================

  // Links store raw node_type* (house convention, as in the tree layer), so
  // the allocator's pointer type only appears at the allocate/deallocate
  // boundary: to_address() unwraps it after allocate, pointer_to() rebuilds
  // it before deallocate. For std::allocator both are the identity.
  template <class... Args>
  [[nodiscard]] constexpr node_type* create(Args&&... args) {
    typename node_allocator_traits::pointer p = node_allocator_traits::allocate(alloc_, 1);
    auto guard = detail::make_exception_guard([&] {
      node_allocator_traits::deallocate(alloc_, p, 1);
    });
    node_allocator_traits::construct(alloc_, std::to_address(p), std::forward<Args>(args)...);
    guard.complete();
    return std::to_address(p);
  }

  constexpr void destroy(node_type* node) noexcept {
    node_allocator_traits::destroy(alloc_, node);
    node_allocator_traits::deallocate(
        alloc_, std::pointer_traits<typename node_allocator_traits::pointer>::pointer_to(*node), 1);
  }

  /**
   * @brief Destroy every node. Iterative: the forest is its own stack.
   *
   * Pops the head of a work list seeded with the root list and pushes the
   * head's children before destroying it. Child links are about to die
   * anyway, so next_sibling can be repurposed as the work-list link —
   * O(n) time, O(1) space, immune to chain-shaped trees.
   */
  constexpr void clear() noexcept {
    node_type* todo = root;
    root = nullptr;
    count = 0;
    while (todo != nullptr) {
      node_type* n = todo;
      todo = n->next_sibling;
      node_type* child = n->first_child;
      while (child != nullptr) {
        node_type* next_child = child->next_sibling;
        child->next_sibling = todo;
        todo = child;
        child = next_child;
      }
      destroy(n);
    }
  }

  [[nodiscard]] constexpr Allocator get_allocator() const noexcept { return Allocator(alloc_); }

  [[nodiscard]] constexpr node_allocator_type& node_allocator() noexcept { return alloc_; }
  [[nodiscard]] constexpr const node_allocator_type& node_allocator() const noexcept { return alloc_; }

 private:
  [[no_unique_address]] node_allocator_type alloc_{};

  constexpr void steal(node_forest& other) noexcept {
    root = std::exchange(other.root, nullptr);
    count = std::exchange(other.count, 0);
  }

  // Clone `other`'s forest into *this (which must be empty), preserving the
  // exact link structure and per-node extension state. An lvalue source is
  // copied; an rvalue source has its values moved out (its structure is left
  // intact for the caller to clear()).
  template <class Forest>
  constexpr void adopt_clone_of(Forest&& other) {
    constexpr bool move_values = !std::is_lvalue_reference_v<Forest>;
    using source_node = std::conditional_t<move_values, node_type, const node_type>;

    source_node* src = other.root;
    if (src == nullptr) {
      return;
    }

    // Lock-step preorder walk: `s` traverses the source with O(1) space
    // (back edges, see for_each_node); `d` mirrors every step in the copy,
    // so each new node is linked the moment it is created. The partial
    // clone is a well-formed forest at all times, which makes exception
    // handling trivial: clear() it and rethrow.
    auto guard = detail::make_exception_guard([&] {
      clear();
    });
    auto transfer = [](source_node& n) -> decltype(auto) {
      if constexpr (move_values) {
        return std::move(n.value);
      } else {
        return (n.value);
      }
    };

    root = create(transfer(*src));
    root->extension = src->extension;
    ++count;

    source_node* s = src;
    node_type* d = root;
    while (true) {
      if (s->first_child != nullptr) {
        s = s->first_child;
        node_type* child = create(transfer(*s));
        child->extension = s->extension;
        ++count;
        child->prev_or_parent = d;
        d->first_child = child;
        d = child;
        continue;
      }
      while (s != nullptr && s->next_sibling == nullptr) {
        s = parent_of(s);
        d = parent_of(d);
      }
      if (s == nullptr) {
        break;
      }
      s = s->next_sibling;
      node_type* sibling = create(transfer(*s));
      sibling->extension = s->extension;
      ++count;
      sibling->prev_or_parent = d;
      d->next_sibling = sibling;
      d = sibling;
    }
    guard.complete();
  }
};

}  // namespace chaistl::detail::heap
