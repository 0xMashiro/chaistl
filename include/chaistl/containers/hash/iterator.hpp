// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/hash/node.hpp>
#include <chaistl/iterator/interface.hpp>

#include <cstddef>
#include <iterator>
#include <type_traits>

namespace chaistl::detail::hash {

/**
 * @brief Forward iterator over a hash table's global iteration list.
 *
 * Uses the @c iterator_impl<Const> pattern from chaistl::list: a single
 * template parameterized on @c bool Const produces both @c iterator and
 * @c const_iterator.
 *
 * The iterator stores a @c hash_node_base* and walks @c next_in_order, so
 * increment is O(1) regardless of how many buckets are empty — the reason
 * the table keeps a global list at all (ADR: Hash Table Design). It casts to
 * the typed node only on dereference. `end()` is the null iterator.
 *
 * Derives from @c chaistl::iterator_interface to eliminate boilerplate:
 * operator-> and postfix ++ are deduced from the core primitives.
 *
 * @tparam T      The value type stored in the table.
 * @tparam Const  true for const_iterator, false for iterator.
 */
template <class T, bool Const>
class hash_iterator : public chaistl::iterator_interface {
 public:
  using iterator_category = std::forward_iterator_tag;
  using iterator_concept = std::forward_iterator_tag;
  using value_type = T;
  using difference_type = std::ptrdiff_t;
  using reference = std::conditional_t<Const, const T&, T&>;
  using pointer = std::conditional_t<Const, const T*, T*>;

  // ========================================================================
  // Construction
  // ========================================================================

  constexpr hash_iterator() noexcept = default;

  explicit constexpr hash_iterator(hash_node_base* node) noexcept : node_(node) {}

  explicit constexpr hash_iterator(const hash_node_base* node) noexcept
    requires Const
      : node_(const_cast<hash_node_base*>(node)) {}

  // A user-declared copy constructor deprecates the implicit copy
  // assignment operator — declare both explicitly.
  constexpr hash_iterator(const hash_iterator&) noexcept = default;
  constexpr hash_iterator& operator=(const hash_iterator&) noexcept = default;

  constexpr hash_iterator(const hash_iterator<T, false>& other) noexcept
    requires Const
      : node_(other.node_) {}

  // ========================================================================
  // Core primitives
  // ========================================================================

  [[nodiscard]] constexpr reference operator*() const noexcept { return static_cast<hash_node<T>*>(node_)->value; }

  constexpr hash_iterator& operator++() noexcept {
    node_ = node_->next_in_order;
    return *this;
  }

  // Bring in postfix ++ from iterator_interface (name hiding).
  using iterator_interface::operator++;

  // ========================================================================
  // Comparison
  // ========================================================================

  [[nodiscard]] constexpr bool operator==(const hash_iterator& other) const noexcept { return node_ == other.node_; }

  // ========================================================================
  // Internal access (for container use)
  // ========================================================================

  [[nodiscard]] constexpr hash_node_base* base() const noexcept { return node_; }

 private:
  hash_node_base* node_ = nullptr;

  template <class U, bool C>
  friend class hash_iterator;
};

/**
 * @brief Forward iterator over a single bucket's collision chain.
 *
 * Walks @c next_in_bucket and stops at @c nullptr, so it visits exactly the
 * elements of one bucket — the standard's local-iterator contract. It needs
 * no cached hash codes and no bucket array access; that simplicity is a
 * direct consequence of the direct-chain layout.
 *
 * @tparam T      The value type stored in the table.
 * @tparam Const  true for const_local_iterator, false for local_iterator.
 */
template <class T, bool Const>
class hash_local_iterator : public chaistl::iterator_interface {
 public:
  using iterator_category = std::forward_iterator_tag;
  using iterator_concept = std::forward_iterator_tag;
  using value_type = T;
  using difference_type = std::ptrdiff_t;
  using reference = std::conditional_t<Const, const T&, T&>;
  using pointer = std::conditional_t<Const, const T*, T*>;

  constexpr hash_local_iterator() noexcept = default;

  explicit constexpr hash_local_iterator(hash_node_base* node) noexcept : node_(node) {}

  explicit constexpr hash_local_iterator(const hash_node_base* node) noexcept
    requires Const
      : node_(const_cast<hash_node_base*>(node)) {}

  constexpr hash_local_iterator(const hash_local_iterator&) noexcept = default;
  constexpr hash_local_iterator& operator=(const hash_local_iterator&) noexcept = default;

  constexpr hash_local_iterator(const hash_local_iterator<T, false>& other) noexcept
    requires Const
      : node_(other.node_) {}

  [[nodiscard]] constexpr reference operator*() const noexcept { return static_cast<hash_node<T>*>(node_)->value; }

  constexpr hash_local_iterator& operator++() noexcept {
    node_ = node_->next_in_bucket;
    return *this;
  }

  using iterator_interface::operator++;

  [[nodiscard]] constexpr bool operator==(const hash_local_iterator& other) const noexcept {
    return node_ == other.node_;
  }

  [[nodiscard]] constexpr hash_node_base* base() const noexcept { return node_; }

 private:
  hash_node_base* node_ = nullptr;

  template <class U, bool C>
  friend class hash_local_iterator;
};

}  // namespace chaistl::detail::hash
