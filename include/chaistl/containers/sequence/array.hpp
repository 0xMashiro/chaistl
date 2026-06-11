// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// array - Fixed-size contiguous aggregate container
// ============================================================================
//
// Architecture:
//   - Stores exactly N elements inline, with no allocator and no separate size
//     member. The object itself is the storage.
//   - A small storage trait handles N == 0, where the standard still requires
//     a well-formed container even though element access is a precondition
//     violation.
//   - Iterators are raw pointers because array's entire contract is fixed-size
//     contiguous storage.
//
// Standardization archaeology:
//   - std::array arrived with C++11 as the standard-library wrapper around
//     built-in arrays: aggregate initialization and zero overhead, but with the
//     container interface needed by generic algorithms.
//   - The zero-size case is intentionally specified: begin() == end(), size()
//     is zero, and front()/back() are invalid. This is one of the clearest
//     examples of the standard separating a container's range from its element
//     access preconditions.
//   - C++17 class template argument deduction and C++20 constexpr algorithms
//     made array more useful as a compile-time sequence.
//
// Non-standard extensions:
//   - Hardened element access reports out-of-bounds and zero-size access
//     through CHAI_HARDENED.
//
// References:
//   - C++ Draft: https://eel.is/c++draft/array
//   - cppreference: https://en.cppreference.com/w/cpp/container/array

#include <chaistl/algorithm/comparison.hpp>
#include <chaistl/concepts/container_element.hpp>
#include <chaistl/iterator/adapter/reverse.hpp>
#include <chaistl/memory/detail/utility/forward_like.hpp>
#include <chaistl/utility/hardening.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

namespace chaistl {

namespace detail {

template <concepts::container_element T, std::size_t N>
struct array_storage {
  using type = T[N];

  [[nodiscard]] static constexpr T& ref(type& elements, std::size_t index) noexcept { return elements[index]; }

  [[nodiscard]] static constexpr const T& ref(const type& elements, std::size_t index) noexcept {
    return elements[index];
  }

  [[nodiscard]] static constexpr T* ptr(type& elements) noexcept { return elements; }

  [[nodiscard]] static constexpr const T* ptr(const type& elements) noexcept { return elements; }
};

template <concepts::container_element T>
struct array_storage<T, 0> {
  struct type {};

  // Element access on a zero-sized array is always a precondition
  // violation: hardened builds abort with a diagnosable trap, release
  // builds keep the unreachable hint.
  [[nodiscard]] static constexpr T& ref(type&, std::size_t) noexcept {
    CHAI_HARDENED(false, "array<T, 0>: element access on zero-sized array");
    [[assume(false)]];
  }

  [[nodiscard]] static constexpr const T& ref(const type&, std::size_t) noexcept {
    CHAI_HARDENED(false, "array<T, 0>: element access on zero-sized array");
    [[assume(false)]];
  }

  [[nodiscard]] static constexpr T* ptr(type&) noexcept { return nullptr; }

  [[nodiscard]] static constexpr const T* ptr(const type&) noexcept { return nullptr; }
};

}  // namespace detail

/**
 * @brief Fixed-size contiguous array container.
 *
 * References:
 *
 * - C++ Draft Standard: https://eel.is/c++draft/array
 * - cppreference: https://en.cppreference.com/w/cpp/container/array
 */
template <concepts::container_element T, std::size_t N>
struct array {
  // member types
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = chaistl::reverse_iterator<iterator>;
  using const_reverse_iterator = chaistl::reverse_iterator<const_iterator>;

  // Keeps array aggregate-initializable while avoiding a dummy T object for N == 0.
  typename detail::array_storage<T, N>::type elements;

  // element access
  [[nodiscard]] constexpr reference operator[](size_type index) noexcept {
    CHAI_HARDENED(index < N, "chaistl::array::operator[]: out of bounds");
    return detail::array_storage<T, N>::ref(elements, index);
  }

  [[nodiscard]] constexpr const_reference operator[](size_type index) const noexcept {
    CHAI_HARDENED(index < N, "chaistl::array::operator[]: out of bounds");
    return detail::array_storage<T, N>::ref(elements, index);
  }

  [[nodiscard]] constexpr reference at(size_type index) {
    if constexpr (N == 0) {
      throw std::out_of_range("chaistl::array::at");
    } else {
      if (index >= N) {
        throw std::out_of_range("chaistl::array::at");
      }
      return (*this)[index];
    }
  }

  [[nodiscard]] constexpr const_reference at(size_type index) const {
    if constexpr (N == 0) {
      throw std::out_of_range("chaistl::array::at");
    } else {
      if (index >= N) {
        throw std::out_of_range("chaistl::array::at");
      }
      return (*this)[index];
    }
  }

  [[nodiscard]] constexpr reference front() noexcept {
    CHAI_HARDENED(N > 0, "chaistl::array::front: N == 0");
    if constexpr (N == 0) {
      [[assume(false)]];
    }
    return (*this)[0];
  }

  [[nodiscard]] constexpr const_reference front() const noexcept {
    CHAI_HARDENED(N > 0, "chaistl::array::front: N == 0");
    if constexpr (N == 0) {
      [[assume(false)]];
    }
    return (*this)[0];
  }

  [[nodiscard]] constexpr reference back() noexcept {
    CHAI_HARDENED(N > 0, "chaistl::array::back: N == 0");
    if constexpr (N == 0) {
      [[assume(false)]];
    }
    return (*this)[N - 1];
  }

  [[nodiscard]] constexpr const_reference back() const noexcept {
    CHAI_HARDENED(N > 0, "chaistl::array::back: N == 0");
    if constexpr (N == 0) {
      [[assume(false)]];
    }
    return (*this)[N - 1];
  }

  [[nodiscard]] constexpr pointer data() noexcept { return detail::array_storage<T, N>::ptr(elements); }

  [[nodiscard]] constexpr const_pointer data() const noexcept { return detail::array_storage<T, N>::ptr(elements); }

  // iterators
  [[nodiscard]] constexpr iterator begin() noexcept { return data(); }

  [[nodiscard]] constexpr const_iterator begin() const noexcept { return data(); }

  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }

  [[nodiscard]] constexpr iterator end() noexcept {
    if constexpr (N == 0) {
      return begin();
    }
    return data() + N;
  }

  [[nodiscard]] constexpr const_iterator end() const noexcept {
    if constexpr (N == 0) {
      return begin();
    }
    return data() + N;
  }

  [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }

  [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

  [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

  [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }

  [[nodiscard]] constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

  [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

  [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return rend(); }

  // capacity
  //
  // Standard signatures ([array.overview]): const member functions, plain
  // constexpr. The previous `static consteval` spelling rejected the most
  // ordinary use — calling a.size() on a runtime object — because an
  // immediate invocation must be a constant expression, object argument
  // included.
  [[nodiscard]] constexpr size_type size() const noexcept { return N; }

  [[nodiscard]] constexpr size_type max_size() const noexcept { return N; }

  [[nodiscard]] constexpr bool empty() const noexcept { return N == 0; }

  // modifiers
  constexpr void fill(const T& value)
    requires std::assignable_from<T&, const T&>
  {
    std::fill(begin(), end(), value);
  }

  constexpr void swap(array& other) noexcept(std::is_nothrow_swappable_v<T>)
    requires std::swappable<T>
  {
    std::ranges::swap_ranges(*this, other);
  }
};

// deduction guide
template <concepts::container_element T, class... U>
  requires((std::same_as<T, U> && ...) && (concepts::container_element<U> && ...))
array(T, U...) -> array<T, 1 + sizeof...(U)>;

template <concepts::container_element T, std::size_t N>
[[nodiscard]] constexpr T* begin(array<T, N>& arr) noexcept {
  return arr.data();
}

template <concepts::container_element T, std::size_t N>
[[nodiscard]] constexpr const T* begin(const array<T, N>& arr) noexcept {
  return arr.data();
}

template <concepts::container_element T, std::size_t N>
[[nodiscard]] constexpr T* end(array<T, N>& arr) noexcept {
  if constexpr (N == 0) {
    return arr.data();
  }
  return arr.data() + N;
}

template <concepts::container_element T, std::size_t N>
[[nodiscard]] constexpr const T* end(const array<T, N>& arr) noexcept {
  if constexpr (N == 0) {
    return arr.data();
  }
  return arr.data() + N;
}

template <concepts::container_element T, std::size_t N>
[[nodiscard]] constexpr bool operator==(const array<T, N>& lhs, const array<T, N>& rhs) {
  return std::ranges::equal(lhs, rhs);
}

template <concepts::container_element T, std::size_t N>
[[nodiscard]] constexpr auto operator<=>(const array<T, N>& lhs, const array<T, N>& rhs) {
  return chaistl::default_three_way_compare(lhs, rhs);
}

template <std::size_t I, concepts::container_element T, std::size_t N>
  requires(I < N)
[[nodiscard]] constexpr T& get(array<T, N>& value) noexcept {
  return value[I];
}

template <std::size_t I, concepts::container_element T, std::size_t N>
  requires(I < N)
[[nodiscard]] constexpr const T& get(const array<T, N>& value) noexcept {
  return value[I];
}

template <std::size_t I, concepts::container_element T, std::size_t N>
  requires(I < N)
[[nodiscard]] constexpr T&& get(array<T, N>&& value) noexcept {
  return std::move(value[I]);
}

template <std::size_t I, concepts::container_element T, std::size_t N>
  requires(I < N)
[[nodiscard]] constexpr const T&& get(const array<T, N>&& value) noexcept {
  return std::move(value[I]);
}

template <concepts::container_element T, std::size_t N>
constexpr void swap(array<T, N>& lhs, array<T, N>& rhs) noexcept(noexcept(lhs.swap(rhs)))
  requires std::swappable<T>
{
  lhs.swap(rhs);
}

namespace detail {

template <class T, std::size_t N, std::size_t... I>
[[nodiscard]] constexpr array<std::remove_cv_t<T>, N> to_array_copy(T (&source)[N], std::index_sequence<I...>) {
  return {{source[I]...}};
}

template <class T, std::size_t N, std::size_t... I>
[[nodiscard]] constexpr array<std::remove_cv_t<T>, N> to_array_move(T (&&source)[N], std::index_sequence<I...>) {
  return {{std::move(source[I])...}};
}

}  // namespace detail

template <concepts::container_element T, std::size_t N>
[[nodiscard]] constexpr array<std::remove_cv_t<T>, N> to_array(T (&source)[N]) {
  static_assert(!std::is_array_v<T>, "chaistl::to_array does not accept multidimensional arrays");
  static_assert(std::constructible_from<std::remove_cv_t<T>, T&>, "chaistl::to_array requires copy construction");
  return detail::to_array_copy(source, std::make_index_sequence<N>{});
}

template <concepts::container_element T, std::size_t N>
[[nodiscard]] constexpr array<std::remove_cv_t<T>, N> to_array(T (&&source)[N]) {
  static_assert(!std::is_array_v<T>, "chaistl::to_array does not accept multidimensional arrays");
  static_assert(std::constructible_from<std::remove_cv_t<T>, T>, "chaistl::to_array requires move construction");
  return detail::to_array_move(std::move(source), std::make_index_sequence<N>{});
}

}  // namespace chaistl

namespace std {

template <class T, size_t N>
struct tuple_size<chaistl::array<T, N>> : integral_constant<size_t, N> {};

template <size_t I, class T, size_t N>
  requires(I < N)
struct tuple_element<I, chaistl::array<T, N>> {
  using type = T;
};

}  // namespace std
