// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/tree/node.hpp>
#include <chaistl/meta/type_traits.hpp>

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace chaistl::detail::tree {

/**
 * @brief Owning handle for a detached BST node.
 *
 * This is the tree-layer implementation of C++17 node handles. It owns a
 * single node (or no node), carries the allocator used to create it, and
 * exposes the policy-agnostic operations needed by map/set.
 *
 * The handle is parameterized by the stored value type, the balance policy
 * (so it knows the concrete node type), and the allocator type. Map/set
 * expose it via a using-declaration (possibly wrapped to rename value_type
 * for map).
 *
 * Reference:
 * - C++ Draft: https://eel.is/c++draft/container.node
 * - cppreference: https://en.cppreference.com/w/cpp/container/node_handle
 */
template <class Value, class Policy, class Allocator>
class bst_node_handle {
 public:
  // ========================================================================
  // Member types
  // ========================================================================

  using value_type = Value;
  using allocator_type = Allocator;
  using policy_type = Policy;

  using node_type = bst_node<value_type, policy_type>;
  using node_allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<node_type>;
  using node_allocator_traits = std::allocator_traits<node_allocator_type>;

  // ========================================================================
  // Construction / destruction
  // ========================================================================

  constexpr bst_node_handle() noexcept = default;

  constexpr bst_node_handle(bst_node_handle&& other) noexcept
      : ptr_(std::exchange(other.ptr_, nullptr)), alloc_(std::move(other.alloc_)) {}

  constexpr bst_node_handle& operator=(bst_node_handle&& other) noexcept {
    if (this != &other) {
      reset();
      ptr_ = std::exchange(other.ptr_, nullptr);
      alloc_ = std::move(other.alloc_);
    }
    return *this;
  }

  constexpr ~bst_node_handle() { reset(); }

  // ========================================================================
  // Observers
  // ========================================================================

  [[nodiscard]] constexpr bool empty() const noexcept { return ptr_ == nullptr; }

  [[nodiscard]] constexpr explicit operator bool() const noexcept { return ptr_ != nullptr; }

  [[nodiscard]] constexpr allocator_type get_allocator() const noexcept { return allocator_type(alloc_); }

  [[nodiscard]] constexpr value_type& value() const noexcept { return ptr_->value; }

  /**
   * @brief Access the key of a map node (value_type must provide .first).
   *
   * Only available when the stored value is a key-value pair. Returns a
   * mutable reference: the point of extract() is that a key can be updated
   * while the element is outside the container ([container.node.overview]).
   * The pair member is declared const, so this is the same const_cast that
   * mainstream implementations perform on the extracted node.
   */
  [[nodiscard]] constexpr auto& key() const noexcept
    requires requires(value_type& v) {
      typename std::tuple_element_t<0, value_type>;
      v.first;
    }
  {
    using mutable_key = std::remove_const_t<std::tuple_element_t<0, value_type>>;
    return const_cast<mutable_key&>(ptr_->value.first);
  }

  /**
   * @brief Access the mapped value of a map node (value_type must provide .second).
   *
   * Only available when the stored value is a key-value pair.
   */
  [[nodiscard]] constexpr auto& mapped() const noexcept
    requires requires(value_type& v) { v.second; }
  {
    return ptr_->value.second;
  }

  // ========================================================================
  // Modifiers
  // ========================================================================

  /**
   * @brief Swap with another handle of the same type.
   *
   * The standard only requires swap to be well-formed when the allocators
   * compare equal. We propagate on swap when the allocator type says so;
   * otherwise we assume equal allocators (the common case for stateless
   * allocators) and swap directly.
   */
  constexpr void swap(bst_node_handle& other) noexcept(meta::is_nothrow_container_swappable_v<node_allocator_type>) {
    using std::swap;
    if constexpr (meta::propagate_on_container_swap_v<node_allocator_type>) {
      swap(alloc_, other.alloc_);
    }
    swap(ptr_, other.ptr_);
  }

  /**
   * @brief Destroy the owned node, if any.
   */
  constexpr void reset() noexcept {
    if (ptr_ != nullptr) {
      node_allocator_traits::destroy(alloc_, std::addressof(ptr_->value));
      std::destroy_at(ptr_);
      node_allocator_traits::deallocate(alloc_, ptr_, 1);
      ptr_ = nullptr;
    }
  }

  // Internal constructor used by binary_search_tree::extract.
  constexpr bst_node_handle(node_type* ptr, const node_allocator_type& alloc) noexcept : ptr_(ptr), alloc_(alloc) {}

  [[nodiscard]] constexpr node_type* ptr() const noexcept { return ptr_; }

  /**
   * @brief Release ownership of the node without destroying it.
   *
   * Returns the raw node pointer and leaves the handle empty.
   */
  [[nodiscard]] constexpr node_type* release() noexcept { return std::exchange(ptr_, nullptr); }

 private:
  node_type* ptr_ = nullptr;
  [[no_unique_address]] node_allocator_type alloc_;
};

// ============================================================================
// Non-member swap
// ============================================================================

template <class V, class P, class A>
  requires std::swappable<bst_node_handle<V, P, A>>
constexpr void swap(bst_node_handle<V, P, A>& lhs, bst_node_handle<V, P, A>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

// ============================================================================
// Comparison
// ============================================================================

template <class V, class P, class A>
[[nodiscard]] constexpr bool operator==(const bst_node_handle<V, P, A>& lhs, const bst_node_handle<V, P, A>& rhs) {
  return lhs.empty() && rhs.empty();
}

template <class V, class P, class A>
[[nodiscard]] constexpr bool operator!=(const bst_node_handle<V, P, A>& lhs, const bst_node_handle<V, P, A>& rhs) {
  return !(lhs == rhs);
}

}  // namespace chaistl::detail::tree
