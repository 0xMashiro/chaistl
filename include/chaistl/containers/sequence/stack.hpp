// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// stack - LIFO container adaptor
// ============================================================================
//
// Architecture:
//   - Owns an underlying sequence container and exposes only last-in-first-out
//     operations: push/emplace/pop/top at the back.
//   - Defaults to chaistl::deque, but the constraints allow any compatible
//     sequence container with back(), push_back(), and pop_back().
//
// Standardization archaeology:
//   - stack was standardized in C++98 as one of the original container
//     adaptors. It exists to name a behavioral restriction, not to prescribe a
//     storage layout.
//   - Because the backend is protected state in the standard adaptor model,
//     comparisons and allocator behavior are phrased in terms of the contained
//     sequence.
//   - C++23 added push_range to the standard adaptor family; chaistl keeps the
//     same direction where its backend concepts make the operation meaningful.
//
// References:
//   - C++ Draft: https://eel.is/c++draft/stack
//   - cppreference: https://en.cppreference.com/w/cpp/container/stack

#include <chaistl/concepts/container.hpp>
#include <chaistl/concepts/container_element.hpp>
#include <chaistl/containers/sequence/deque.hpp>
#include <chaistl/memory/detail/utility/forward_like.hpp>
#include <chaistl/utility/hardening.hpp>

#include <compare>
#include <concepts>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

namespace chaistl {

/**
 * @brief LIFO (last-in, first-out) container adapter.
 *
 * @ingroup containers_sequence
 *
 * A stack is a LIFO sequence container. By default the underlying
 * container is deque, but any sequence container that supports
 * `back`, `push_back`, and `pop_back` can be used.
 *
 * Deliberate differences from `std::stack`:
 *
 * - Uses `chaistl::deque` as the default container instead of `std::deque`.
 *
 * References:
 *
 * - C++ Draft Standard: https://eel.is/c++draft/stack
 * - cppreference: https://en.cppreference.com/w/cpp/container/stack
 */
template <concepts::container_element T, class Container = deque<T>>
  requires concepts::sequence_container<Container> && requires(Container c, const T& value, T&& rvalue) {
    { c.back() } -> std::same_as<typename Container::reference>;
    { c.push_back(value) } -> std::same_as<void>;
    { c.push_back(std::move(rvalue)) } -> std::same_as<void>;
    { c.pop_back() } -> std::same_as<void>;
  }
class stack {
 public:
  // =======================================================================
  // Member types
  // =======================================================================
  using value_type = typename Container::value_type;
  using reference = typename Container::reference;
  using const_reference = typename Container::const_reference;
  using size_type = typename Container::size_type;
  using container_type = Container;

  // =======================================================================
  // Constructors and assignment
  // =======================================================================
  constexpr stack() noexcept(std::is_nothrow_default_constructible_v<Container>) : c_() {}

  explicit constexpr stack(const Container& container) noexcept(std::is_nothrow_copy_constructible_v<Container>)
      : c_(container) {}

  explicit constexpr stack(Container&& container) noexcept(std::is_nothrow_move_constructible_v<Container>)
      : c_(std::move(container)) {}

  constexpr stack(const stack& other) noexcept(std::is_nothrow_copy_constructible_v<Container>) : c_(other.c_) {}

  constexpr stack(stack&& other) noexcept(std::is_nothrow_move_constructible_v<Container>) : c_(std::move(other.c_)) {}

  template <std::input_iterator InputIt>
  constexpr stack(InputIt first, InputIt last) : c_(first, last) {}

  constexpr stack(std::initializer_list<T> init) : c_(init) {}

  constexpr ~stack() = default;

  constexpr stack& operator=(const stack& other) {
    c_ = other.c_;
    return *this;
  }

  constexpr stack& operator=(stack&& other) noexcept(std::is_nothrow_move_assignable_v<Container>) {
    c_ = std::move(other.c_);
    return *this;
  }

  // =======================================================================
  // Element access
  // =======================================================================
  [[nodiscard]] constexpr auto&& top(this auto&& self) noexcept {
    CHAI_HARDENED(!self.empty(), "chaistl::stack::top: empty container");
    return detail::forward_like<decltype(self)>(self.c_.back());
  }

  // =======================================================================
  // Capacity
  // =======================================================================
  [[nodiscard]] constexpr bool empty() const noexcept { return c_.empty(); }
  [[nodiscard]] constexpr size_type size() const noexcept { return c_.size(); }

  // =======================================================================
  // Modifiers
  // =======================================================================
  constexpr void push(const value_type& value) { c_.push_back(value); }
  constexpr void push(value_type&& value) { c_.push_back(std::move(value)); }

  template <class... Args>
  constexpr decltype(auto) emplace(Args&&... args) {
    return c_.emplace_back(std::forward<Args>(args)...);
  }

  constexpr void pop() {
    CHAI_HARDENED(!empty(), "chaistl::stack::pop: empty container");
    c_.pop_back();
  }

  constexpr void swap(stack& other) noexcept(std::is_nothrow_swappable_v<Container>) {
    using std::swap;
    swap(c_, other.c_);
  }

  // =======================================================================
  // Protected data member (exposed for comparison operators)
  // =======================================================================
 protected:
  Container c_;

  // Grant non-member operators access to c_.
  template <class T2, class Container2>
  friend constexpr bool operator==(const stack<T2, Container2>& lhs, const stack<T2, Container2>& rhs);

  template <class T2, class Container2>
    requires std::three_way_comparable<Container2>
  friend constexpr auto operator<=>(const stack<T2, Container2>& lhs, const stack<T2, Container2>& rhs);
};

// ======================================================================
// Non-member comparison operators
// ======================================================================

template <class T, class Container>
[[nodiscard]] constexpr bool operator==(const stack<T, Container>& lhs, const stack<T, Container>& rhs) {
  return lhs.c_ == rhs.c_;
}

template <class T, class Container>
  requires std::three_way_comparable<Container>
[[nodiscard]] constexpr auto operator<=>(const stack<T, Container>& lhs, const stack<T, Container>& rhs) {
  return lhs.c_ <=> rhs.c_;
}

// ======================================================================
// Non-member swap
// ======================================================================

template <class T, class Container>
  requires std::swappable<Container>
constexpr void swap(stack<T, Container>& lhs, stack<T, Container>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

}  // namespace chaistl
