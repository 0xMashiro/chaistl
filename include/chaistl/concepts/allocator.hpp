// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/concepts/basic.hpp>
#include <chaistl/concepts/container_element.hpp>

#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>

namespace chaistl::concepts {

// ============================================================================
// "Qualifies as an allocator" — the deduction-guide gate
// https://eel.is/c++draft/container.reqmts (deduction-guide participation)
//
// Container deduction guides must reject (not hard-error on) a non-allocator
// deduced into an Allocator slot, and reject an allocator deduced into a
// Compare slot. The full allocator_for concept cannot serve here: it goes
// through std::allocator_traits, whose primary template is not
// SFINAE-friendly — naming allocator_traits<std::greater<int>>::value_type
// hard-errors inside the trait instead of failing the constraint. The
// standard therefore defines this deliberately shallow syntactic check:
// A::value_type denotes a type and a.allocate(size_t{}) is well-formed.
// ============================================================================
template <class Allocator>
concept qualifies_as_allocator = requires(Allocator& alloc) {
  typename Allocator::value_type;
  alloc.allocate(std::size_t{});
};

// ============================================================================
// Named Requirement: Allocator
// https://en.cppreference.com/w/cpp/named_req/Allocator
//
// Expressed as a C++20 concept. No std:: equivalent exists.
//
// The standard's Allocator named requirement is phrased in terms of allocator
// objects plus allocator_traits. This concept checks the syntactic surface a
// container needs to acquire and release storage for T. The cross-allocation
// semantic rule ("memory allocated by a1 can be deallocated through a2 if
// a1 == a2") remains a behavioral requirement for allocator tests.
//
// Semantic boundary: this concept cannot prove equality is an equivalence
// relation, allocation/deallocation ownership is respected, or the no-throw
// requirements on allocator copy/move/rebind operations. Those are behavioral
// obligations for concrete allocator tests.
// ============================================================================
template <class Allocator, class T>
concept allocator_for =
    std::is_object_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T> && allocator_value_for<Allocator, T> &&
    copy_constructible<Allocator> && move_constructible<Allocator> && std::equality_comparable<Allocator> &&
    requires(Allocator allocator,
             typename std::allocator_traits<Allocator>::size_type count,
             typename std::allocator_traits<Allocator>::pointer pointer) {
      typename std::allocator_traits<Allocator>::value_type;
      typename std::allocator_traits<Allocator>::pointer;
      typename std::allocator_traits<Allocator>::const_pointer;
      typename std::allocator_traits<Allocator>::void_pointer;
      typename std::allocator_traits<Allocator>::const_void_pointer;
      typename std::allocator_traits<Allocator>::difference_type;
      typename std::allocator_traits<Allocator>::size_type;
      typename std::allocator_traits<Allocator>::template rebind_alloc<T>;

      requires std::same_as<typename std::allocator_traits<Allocator>::template rebind_alloc<T>, Allocator>;
      requires std::contiguous_iterator<typename std::allocator_traits<Allocator>::pointer>;
      requires std::contiguous_iterator<typename std::allocator_traits<Allocator>::const_pointer>;
      requires std::convertible_to<typename std::allocator_traits<Allocator>::pointer,
                                   typename std::allocator_traits<Allocator>::const_pointer>;
      requires std::convertible_to<typename std::allocator_traits<Allocator>::pointer,
                                   typename std::allocator_traits<Allocator>::void_pointer>;
      requires std::convertible_to<typename std::allocator_traits<Allocator>::pointer,
                                   typename std::allocator_traits<Allocator>::const_void_pointer>;
      requires std::convertible_to<typename std::allocator_traits<Allocator>::const_pointer,
                                   typename std::allocator_traits<Allocator>::const_void_pointer>;
      requires std::convertible_to<typename std::allocator_traits<Allocator>::void_pointer,
                                   typename std::allocator_traits<Allocator>::const_void_pointer>;
      requires std::signed_integral<typename std::allocator_traits<Allocator>::difference_type>;
      requires std::unsigned_integral<typename std::allocator_traits<Allocator>::size_type>;

      {
        std::allocator_traits<Allocator>::allocate(allocator, count)
      } -> std::same_as<typename std::allocator_traits<Allocator>::pointer>;
      { std::allocator_traits<Allocator>::deallocate(allocator, pointer, count) } -> std::same_as<void>;
      {
        std::allocator_traits<Allocator>::max_size(allocator)
      } -> std::same_as<typename std::allocator_traits<Allocator>::size_type>;
    };

}  // namespace chaistl::concepts
