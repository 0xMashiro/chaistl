// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/containers/tree/node.hpp>
#include <chaistl/iterator/interface.hpp>

#include <iterator>
#include <memory>
#include <type_traits>

namespace chaistl::detail::tree {

/**
 * @brief Bidirectional iterator for binary search trees.
 *
 * Uses the @c iterator_impl<Const> pattern from chaistl::list:
 * a single template parameterized on @c bool Const produces both
 * @c iterator and @c const_iterator.
 *
 * The iterator stores a @c bst_node_base* and casts to the typed node
 * on dereference. This keeps the iterator independent of the balance policy.
 *
 * Derives from @c chaistl::iterator_interface to eliminate boilerplate:
 * operator->, postfix ++/-- are deduced from the core primitives.
 *
 * @tparam T      The value type stored in the tree.
 * @tparam Const  true for const_iterator, false for iterator.
 */
template <class T, bool Const>
class bst_iterator : public chaistl::iterator_interface {
 public:
  using iterator_category = std::bidirectional_iterator_tag;
  using iterator_concept = std::bidirectional_iterator_tag;
  using value_type = T;
  using difference_type = std::ptrdiff_t;
  using reference = std::conditional_t<Const, const T&, T&>;
  using pointer = std::conditional_t<Const, const T*, T*>;

  // ========================================================================
  // Construction
  // ========================================================================

  constexpr bst_iterator() noexcept = default;

  explicit constexpr bst_iterator(bst_node_base* node) noexcept : node_(node) {}

  explicit constexpr bst_iterator(const bst_node_base* node) noexcept
    requires Const
      : node_(const_cast<bst_node_base*>(node)) {}

  // A user-declared copy constructor deprecates the implicit copy
  // assignment operator — declare both explicitly.
  constexpr bst_iterator(const bst_iterator&) noexcept = default;
  constexpr bst_iterator& operator=(const bst_iterator&) noexcept = default;

  constexpr bst_iterator(const bst_iterator<T, false>& other) noexcept
    requires Const
      : node_(other.node_) {}

  // ========================================================================
  // Core primitives (tree-specific, not deducible by iterator_interface)
  // ========================================================================

  [[nodiscard]] constexpr reference operator*() const noexcept { return static_cast<bst_value_node<T>*>(node_)->value; }

  constexpr bst_iterator& operator++() noexcept {
    node_ = node_successor(node_);
    return *this;
  }

  constexpr bst_iterator& operator--() noexcept {
    node_ = node_predecessor(node_);
    return *this;
  }

  // Bring in postfix ++/-- from iterator_interface (name hiding).
  using iterator_interface::operator++;
  using iterator_interface::operator--;

  // ========================================================================
  // Comparison
  // ========================================================================

  [[nodiscard]] constexpr bool operator==(const bst_iterator& other) const noexcept { return node_ == other.node_; }

  // ========================================================================
  // Internal access (for container use)
  // ========================================================================

  [[nodiscard]] constexpr bst_node_base* base() const noexcept { return node_; }

 private:
  bst_node_base* node_ = nullptr;

  template <class U, bool C>
  friend class bst_iterator;
};

}  // namespace chaistl::detail::tree
