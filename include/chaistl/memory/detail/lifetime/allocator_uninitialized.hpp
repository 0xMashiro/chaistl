// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/memory/allocator.hpp>
#include <chaistl/memory/detail/lifetime/exception_guard.hpp>

#include <concepts>
#include <cstring>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>

namespace chaistl::detail {

template <class Allocator, class Value>
struct allocator_has_plain_construct_semantics : std::false_type {};

template <class T, class Value>
struct allocator_has_plain_construct_semantics<std::allocator<T>, Value>
    : std::bool_constant<std::same_as<std::remove_cv_t<T>, std::remove_cv_t<Value>>> {};

template <class T, class Value>
struct allocator_has_plain_construct_semantics<chaistl::allocator<T>, Value>
    : std::bool_constant<std::same_as<std::remove_cv_t<T>, std::remove_cv_t<Value>>> {};

template <class Allocator, class Value>
inline constexpr bool allocator_has_plain_construct_semantics_v =
    allocator_has_plain_construct_semantics<std::remove_cvref_t<Allocator>, Value>::value;

template <class Allocator, class InputIt, class Pointer>
inline constexpr bool can_bulk_uninitialized_transfer_v =
    std::contiguous_iterator<InputIt> && std::contiguous_iterator<Pointer> &&
    std::same_as<std::remove_cv_t<std::iter_value_t<InputIt>>,
                 std::remove_cv_t<std::remove_reference_t<decltype(*std::declval<Pointer>())>>> &&
    std::is_trivially_copyable_v<std::remove_reference_t<decltype(*std::declval<Pointer>())>> &&
    allocator_has_plain_construct_semantics_v<Allocator, std::remove_reference_t<decltype(*std::declval<Pointer>())>>;

template <class Allocator, class Pointer>
inline constexpr bool can_bulk_value_initialize_v =
    std::contiguous_iterator<Pointer> &&
    std::is_trivially_copyable_v<std::remove_reference_t<decltype(*std::declval<Pointer>())>> &&
    std::is_trivially_default_constructible_v<std::remove_reference_t<decltype(*std::declval<Pointer>())>> &&
    allocator_has_plain_construct_semantics_v<Allocator, std::remove_reference_t<decltype(*std::declval<Pointer>())>>;

template <class Allocator, class Pointer>
constexpr void allocator_destroy_forward(Allocator& allocator, Pointer first, Pointer last) noexcept {
  using value_type = std::remove_reference_t<decltype(*first)>;
  if constexpr (std::is_trivially_destructible_v<value_type>) {
    return;
  }
  using allocator_traits = std::allocator_traits<Allocator>;
  for (; first != last; ++first) {
    allocator_traits::destroy(allocator, std::to_address(first));
  }
}

template <class Allocator, class Pointer>
constexpr void allocator_destroy_reverse(Allocator& allocator, Pointer first, Pointer last) noexcept {
  using value_type = std::remove_reference_t<decltype(*first)>;
  if constexpr (std::is_trivially_destructible_v<value_type>) {
    return;
  }
  using allocator_traits = std::allocator_traits<Allocator>;
  while (last != first) {
    --last;
    allocator_traits::destroy(allocator, std::to_address(last));
  }
}

template <class Allocator, class InputIt, class Pointer>
constexpr Pointer allocator_uninitialized_copy(Allocator& allocator, InputIt first, InputIt last, Pointer result) {
  using value_type = std::remove_reference_t<decltype(*result)>;

  if constexpr (can_bulk_uninitialized_transfer_v<Allocator, InputIt, Pointer>) {
    if !consteval {  // memmove is not usable in constant evaluation
      const auto count = static_cast<std::size_t>(std::distance(first, last));
      if (count > 0) [[likely]] {
        std::memmove(std::to_address(result), std::to_address(first), count * sizeof(value_type));
      }
      return result + count;
    }
  }

  using allocator_traits = std::allocator_traits<Allocator>;

  if constexpr (std::is_trivially_destructible_v<value_type>) {
    // Element-by-element construction without guard: the destructor is trivial,
    // so partially-constructed elements need no cleanup on exception.
    Pointer current = result;
    for (; first != last; ++first, ++current) {
      allocator_traits::construct(allocator, std::to_address(current), *first);
    }
    return current;
  }

  Pointer current = result;
  auto guard = make_exception_guard([&] {
    allocator_destroy_reverse(allocator, result, current);
  });
  for (; first != last; ++first, ++current) {
    allocator_traits::construct(allocator, std::to_address(current), *first);
  }
  guard.complete();
  return current;
}

template <class Allocator, class InputIt, class Size, class Pointer>
constexpr Pointer allocator_uninitialized_copy_n(Allocator& allocator, InputIt first, Size count, Pointer result) {
  using value_type = std::remove_reference_t<decltype(*result)>;
  using allocator_traits = std::allocator_traits<Allocator>;

  if constexpr (std::is_trivially_destructible_v<value_type>) {
    Pointer current = result;
    for (Size index = 0; index < count; ++index, ++first, ++current) {
      allocator_traits::construct(allocator, std::to_address(current), *first);
    }
    return current;
  }

  Pointer current = result;
  auto guard = make_exception_guard([&] {
    allocator_destroy_reverse(allocator, result, current);
  });
  for (Size index = 0; index < count; ++index, ++first, ++current) {
    allocator_traits::construct(allocator, std::to_address(current), *first);
  }
  guard.complete();
  return current;
}

template <class Allocator, class InputIt, class Pointer>
constexpr Pointer allocator_uninitialized_move(Allocator& allocator, InputIt first, InputIt last, Pointer result) {
  using value_type = std::remove_reference_t<decltype(*result)>;

  if constexpr (can_bulk_uninitialized_transfer_v<Allocator, InputIt, Pointer>) {
    if !consteval {  // memmove is not usable in constant evaluation
      const auto count = static_cast<std::size_t>(std::distance(first, last));
      if (count > 0) [[likely]] {
        std::memmove(std::to_address(result), std::to_address(first), count * sizeof(value_type));
      }
      return result + count;
    }
  }

  using allocator_traits = std::allocator_traits<Allocator>;

  if constexpr (std::is_trivially_destructible_v<value_type>) {
    Pointer current = result;
    for (; first != last; ++first, ++current) {
      allocator_traits::construct(allocator, std::to_address(current), std::move(*first));
    }
    return current;
  }

  Pointer current = result;
  auto guard = make_exception_guard([&] {
    allocator_destroy_reverse(allocator, result, current);
  });
  for (; first != last; ++first, ++current) {
    allocator_traits::construct(allocator, std::to_address(current), std::move(*first));
  }
  guard.complete();
  return current;
}

template <class Allocator, class InputIt, class Pointer>
constexpr Pointer allocator_uninitialized_move_if_noexcept(Allocator& allocator,
                                                           InputIt first,
                                                           InputIt last,
                                                           Pointer result) {
  using value_type = std::remove_reference_t<decltype(*result)>;

  if constexpr (can_bulk_uninitialized_transfer_v<Allocator, InputIt, Pointer>) {
    if !consteval {  // memmove is not usable in constant evaluation
      const auto count = static_cast<std::size_t>(std::distance(first, last));
      if (count > 0) [[likely]] {
        std::memmove(std::to_address(result), std::to_address(first), count * sizeof(value_type));
      }
      return result + count;
    }
  }

  using allocator_traits = std::allocator_traits<Allocator>;

  if constexpr (std::is_trivially_destructible_v<value_type>) {
    Pointer current = result;
    for (; first != last; ++first, ++current) {
      allocator_traits::construct(allocator, std::to_address(current), std::move_if_noexcept(*first));
    }
    return current;
  }

  Pointer current = result;
  auto guard = make_exception_guard([&] {
    allocator_destroy_reverse(allocator, result, current);
  });
  for (; first != last; ++first, ++current) {
    allocator_traits::construct(allocator, std::to_address(current), std::move_if_noexcept(*first));
  }
  guard.complete();
  return current;
}

template <class Allocator, class Pointer, class Size, class T>
constexpr Pointer allocator_uninitialized_fill_n(Allocator& allocator, Pointer first, Size count, const T& value) {
  using value_type = std::remove_reference_t<decltype(*first)>;

  using allocator_traits = std::allocator_traits<Allocator>;

  if constexpr (std::is_trivially_copyable_v<value_type>) {
    if (count > 0) [[likely]] {
      for (Size index = 0; index < count; ++index) {
        allocator_traits::construct(allocator, std::to_address(first) + index, value);
      }
    }
    return first + count;
  }

  if constexpr (std::is_trivially_destructible_v<value_type>) {
    Pointer current = first;
    for (Size index = 0; index < count; ++index, ++current) {
      allocator_traits::construct(allocator, std::to_address(current), value);
    }
    return current;
  }

  Pointer current = first;
  auto guard = make_exception_guard([&] {
    allocator_destroy_reverse(allocator, first, current);
  });
  for (Size index = 0; index < count; ++index, ++current) {
    allocator_traits::construct(allocator, std::to_address(current), value);
  }
  guard.complete();
  return current;
}

template <class Allocator, class Pointer, class Size>
constexpr Pointer allocator_uninitialized_default_construct_n(Allocator& allocator, Pointer first, Size count) {
  using value_type = std::remove_reference_t<decltype(*first)>;

  // Allocator-aware default insertion: construct each element as if by
  // allocator_traits::construct(a, p), not by the raw-memory algorithm
  // std::uninitialized_default_construct_n's placement-new `T` expression.
  // With std::allocator's fallback this value-initializes scalars.
  if constexpr (can_bulk_value_initialize_v<Allocator, Pointer>) {
    if !consteval {  // memset is not usable in constant evaluation
      if (count > 0) [[likely]] {
        // value-initialization via allocator_traits::construct(a, p) fallback:
        //   construct_at(p) → ::new(p) T() → zero-initialize for trivial types
        std::memset(std::to_address(first), 0, count * sizeof(value_type));
      }
      return first + count;
    }
  }

  using allocator_traits = std::allocator_traits<Allocator>;

  if constexpr (std::is_trivially_destructible_v<value_type>) {
    Pointer current = first;
    for (Size index = 0; index < count; ++index, ++current) {
      allocator_traits::construct(allocator, std::to_address(current));
    }
    return current;
  }

  Pointer current = first;
  auto guard = make_exception_guard([&] {
    allocator_destroy_reverse(allocator, first, current);
  });
  for (Size index = 0; index < count; ++index, ++current) {
    allocator_traits::construct(allocator, std::to_address(current));
  }
  guard.complete();
  return current;
}

}  // namespace chaistl::detail
