// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// queue - FIFO container adaptor
// ============================================================================
//
// Architecture:
//   - Owns an underlying sequence container and exposes only front insertion
//     order: push/emplace at the back, observe/pop at the front.
//   - Defaults to chaistl::deque, matching std::queue's use of deque as the
//     general-purpose double-ended backend.
//   - The adaptor constraints document the real contract: front(), back(),
//     push_back(), and pop_front().
//
// Standardization archaeology:
//   - queue was standardized in C++98 as a container adaptor: a restricted
//     interface over another container, not a storage strategy of its own.
//   - This separation is the point of the adaptor family. The queue contract is
//     FIFO behavior; deque/list-like storage is an implementation choice.
//   - C++23 added push_range to the standard adaptor; chaistl's range support
//     follows the same direction when the backend supports it.
//
// References:
//   - C++ Draft: https://eel.is/c++draft/queue
//   - cppreference: https://en.cppreference.com/w/cpp/container/queue

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
 * @brief FIFO (first-in, first-out) container adapter.
 *
 * @ingroup containers_sequence
 *
 * A queue is a FIFO sequence container. By default the underlying
 * container is deque, but any sequence container that supports
 * `front`, `back`, `push_back`, and `pop_front` can be used.
 *
 * Deliberate differences from `std::queue`:
 *
 * - Uses `chaistl::deque` as the default container instead of `std::deque`.
 *
 * References:
 *
 * - C++ Draft Standard: https://eel.is/c++draft/queue
 * - cppreference: https://en.cppreference.com/w/cpp/container/queue
 */
template <concepts::container_element T, class Container = deque<T>>
  requires concepts::sequence_container<Container> && requires(Container c, const T& value, T&& rvalue) {
    { c.front() } -> std::same_as<typename Container::reference>;
    { c.push_back(value) } -> std::same_as<void>;
    { c.push_back(std::move(rvalue)) } -> std::same_as<void>;
    { c.pop_front() } -> std::same_as<void>;
  }
class queue {
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
  constexpr queue() noexcept(std::is_nothrow_default_constructible_v<Container>) : c_() {}

  explicit constexpr queue(const Container& container) noexcept(std::is_nothrow_copy_constructible_v<Container>)
      : c_(container) {}

  explicit constexpr queue(Container&& container) noexcept(std::is_nothrow_move_constructible_v<Container>)
      : c_(std::move(container)) {}

  constexpr queue(const queue& other) noexcept(std::is_nothrow_copy_constructible_v<Container>) : c_(other.c_) {}

  constexpr queue(queue&& other) noexcept(std::is_nothrow_move_constructible_v<Container>) : c_(std::move(other.c_)) {}

  template <std::input_iterator InputIt>
  constexpr queue(InputIt first, InputIt last) : c_(first, last) {}

  constexpr queue(std::initializer_list<T> init) : c_(init) {}

  constexpr ~queue() = default;

  constexpr queue& operator=(const queue& other) {
    c_ = other.c_;
    return *this;
  }

  constexpr queue& operator=(queue&& other) noexcept(std::is_nothrow_move_assignable_v<Container>) {
    c_ = std::move(other.c_);
    return *this;
  }

  // =======================================================================
  // Element access
  // =======================================================================
  [[nodiscard]] constexpr auto&& front(this auto&& self) noexcept {
    CHAI_HARDENED(!self.empty(), "chaistl::queue::front: empty container");
    return detail::forward_like<decltype(self)>(self.c_.front());
  }

  [[nodiscard]] constexpr auto&& back(this auto&& self) noexcept {
    CHAI_HARDENED(!self.empty(), "chaistl::queue::back: empty container");
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
    CHAI_HARDENED(!empty(), "chaistl::queue::pop: empty container");
    c_.pop_front();
  }

  constexpr void swap(queue& other) noexcept(std::is_nothrow_swappable_v<Container>) {
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
  friend constexpr bool operator==(const queue<T2, Container2>& lhs, const queue<T2, Container2>& rhs);

  template <class T2, class Container2>
    requires std::three_way_comparable<Container2>
  friend constexpr auto operator<=>(const queue<T2, Container2>& lhs, const queue<T2, Container2>& rhs);
};

// ======================================================================
// Non-member comparison operators
// ======================================================================

template <class T, class Container>
[[nodiscard]] constexpr bool operator==(const queue<T, Container>& lhs, const queue<T, Container>& rhs) {
  return lhs.c_ == rhs.c_;
}

template <class T, class Container>
  requires std::three_way_comparable<Container>
[[nodiscard]] constexpr auto operator<=>(const queue<T, Container>& lhs, const queue<T, Container>& rhs) {
  return lhs.c_ <=> rhs.c_;
}

// ======================================================================
// Non-member swap
// ======================================================================

template <class T, class Container>
  requires std::swappable<Container>
constexpr void swap(queue<T, Container>& lhs, queue<T, Container>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

}  // namespace chaistl
