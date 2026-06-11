// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/hash/node.hpp>
#include <chaistl/meta/type_traits.hpp>

#include <concepts>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace chaistl::detail::hash {

/**
 * @brief Owning handle for a detached hash table node.
 *
 * Mirrors @c detail::tree::bst_node_handle: owns a single node (or none) and
 * carries the allocator used to create it, so the node can be destroyed or
 * re-adopted by a container with an equal allocator.
 *
 * The node's @c cached_hash field is preserved by layout but is semantically
 * stale outside a table: re-insertion always recomputes the hash with the
 * destination's hash function, which may be a different stateful instance
 * (ADR: Hash Table Design).
 *
 * Reference:
 * - C++ Draft: https://eel.is/c++draft/container.node
 * - cppreference: https://en.cppreference.com/w/cpp/container/node_handle
 */
template <class Value, class Allocator>
class hash_node_handle {
 public:
  // ========================================================================
  // Member types
  // ========================================================================

  using value_type = Value;
  using allocator_type = Allocator;

  using node_type = hash_node<value_type>;
  using node_allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<node_type>;
  using node_allocator_traits = std::allocator_traits<node_allocator_type>;

  // ========================================================================
  // Construction / destruction
  // ========================================================================

  constexpr hash_node_handle() noexcept = default;

  constexpr hash_node_handle(hash_node_handle&& other) noexcept
      : ptr_(std::exchange(other.ptr_, nullptr)), alloc_(std::move(other.alloc_)) {}

  constexpr hash_node_handle& operator=(hash_node_handle&& other) noexcept {
    if (this != &other) {
      reset();
      ptr_ = std::exchange(other.ptr_, nullptr);
      alloc_ = std::move(other.alloc_);
    }
    return *this;
  }

  constexpr ~hash_node_handle() { reset(); }

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
   * Returns a mutable reference: the point of extract() is that a key can be
   * updated while the element is outside the container
   * ([container.node.overview]). The pair member is declared const, so this
   * is the same const_cast that mainstream implementations perform on the
   * extracted node (and the tree-layer handle takes the same stance).
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

  /// Access the mapped value of a map node (value_type must provide .second).
  [[nodiscard]] constexpr auto& mapped() const noexcept
    requires requires(value_type& v) { v.second; }
  {
    return ptr_->value.second;
  }

  // ========================================================================
  // Modifiers
  // ========================================================================

  constexpr void swap(hash_node_handle& other) noexcept(meta::is_nothrow_container_swappable_v<node_allocator_type>) {
    using std::swap;
    if constexpr (meta::propagate_on_container_swap_v<node_allocator_type>) {
      swap(alloc_, other.alloc_);
    }
    swap(ptr_, other.ptr_);
  }

  /// Destroy the owned node, if any.
  constexpr void reset() noexcept {
    if (ptr_ != nullptr) {
      node_allocator_traits::destroy(alloc_, std::addressof(ptr_->value));
      std::destroy_at(ptr_);
      node_allocator_traits::deallocate(
          alloc_, std::pointer_traits<typename node_allocator_traits::pointer>::pointer_to(*ptr_), 1);
      ptr_ = nullptr;
    }
  }

  // Internal constructor used by hash_table::extract.
  constexpr hash_node_handle(node_type* ptr, const node_allocator_type& alloc) noexcept : ptr_(ptr), alloc_(alloc) {}

  [[nodiscard]] constexpr node_type* ptr() const noexcept { return ptr_; }

  /// Release ownership of the node without destroying it.
  [[nodiscard]] constexpr node_type* release() noexcept { return std::exchange(ptr_, nullptr); }

 private:
  node_type* ptr_ = nullptr;
  [[no_unique_address]] node_allocator_type alloc_;
};

// ============================================================================
// Non-member swap / comparison
// ============================================================================

template <class V, class A>
  requires std::swappable<hash_node_handle<V, A>>
constexpr void swap(hash_node_handle<V, A>& lhs, hash_node_handle<V, A>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

}  // namespace chaistl::detail::hash
