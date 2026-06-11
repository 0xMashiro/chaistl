// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <concepts>
#include <new>
#include <type_traits>
#include <utility>

namespace chaistl::concepts {

// ============================================================================
// Project-specific helpers (no std equivalent)
// ============================================================================

// Exposition-only helper used by iterator concepts.
// No std:: equivalent; std::indirectly_readable requires more than referenceability.
template <class T>
concept referenceable = requires { typename std::type_identity<T&>::type; };

// ============================================================================
// Project-specific concepts (no std equivalent)
// ============================================================================

// Reference:
// - https://en.cppreference.com/w/cpp/container#Transparent_function_objects
//
// A comparator is "transparent" when it defines the nested type
// is_transparent, enabling heterogeneous lookup in associative containers.
// This is not a named requirement and has no std:: concept equivalent.
template <class Compare>
concept transparent_compare = requires { typename Compare::is_transparent; };

// ============================================================================
// Named Requirement: DefaultConstructible
// https://en.cppreference.com/w/cpp/named_req/DefaultConstructible
//
// Expressed as a C++20 concept. This models the named requirement, which
// differs from std::default_initializable:
//   - std::default_initializable: T u; T u{}; T(); T{}
//   - DefaultConstructible named req: additionally requires placement-new
//     (::new (static_cast<void*>(nullptr)) T) for allocator-aware containers
//
// std:: equivalent:
//   std::default_initializable<T> — weaker, no placement-new requirement
// ============================================================================
template <class T>
concept default_constructible = std::destructible<T> && std::default_initializable<T> && requires {
  T{};
  T();
  ::new (static_cast<void*>(nullptr)) T;
};

// ============================================================================
// Named Requirement: MoveConstructible
// https://en.cppreference.com/w/cpp/named_req/MoveConstructible
//
// Expressed as a C++20 concept. The named requirement includes semantic
// postconditions (value equivalence) that concepts cannot express; those
// are tested on concrete types.
//
// Compared to std::move_constructible:
//   - std::move_constructible: syntactic only (can call T(T&&))
//   - MoveConstructible named req: additionally requires Destructible and
//     specific expression forms (T u = rv; T(rv))
//
// std:: equivalent:
//   std::move_constructible<T> — weaker, only syntax
// ============================================================================
template <class T>
concept move_constructible = std::destructible<T> && std::move_constructible<T> && requires(T&& value) {
  T{std::move(value)};
  T(std::move(value));
};

// ============================================================================
// Named Requirement: CopyConstructible
// https://en.cppreference.com/w/cpp/named_req/CopyConstructible
//
// Expressed as a C++20 concept. This models the named requirement, which is
// NOT identical to std::copy_constructible:
//   - std::copy_constructible: syntactic only (can call T(const T&))
//   - CopyConstructible named req: additionally requires MoveConstructible
//     and specific expression forms for lvalue and const rvalue
//
// See cppreference Notes for the counter-example: a type with only a copy
// constructor (no move constructor) satisfies std::copy_constructible but
// NOT the CopyConstructible named requirement.
//
// std:: equivalents:
//   std::copy_constructible<T>  — weaker, only syntax
//   std::move_constructible<T>  — prerequisite
// ============================================================================
template <class T>
concept copy_constructible =
    move_constructible<T> && std::convertible_to<T&, T> && std::convertible_to<const T&, T> &&
    std::convertible_to<const T&&, T> && requires(T& value, const T& const_value, const T&& const_rvalue) {
      T{value};
      T{const_value};
      T{std::move(const_rvalue)};
      T(value);
      T(const_value);
      T(std::move(const_rvalue));
    };

// ============================================================================
// Named Requirement: MoveAssignable
// https://en.cppreference.com/w/cpp/named_req/MoveAssignable
//
// Expressed as a C++20 concept. The named requirement includes semantic
// postconditions (value equivalence, rv state unspecified) that concepts
// cannot express.
//
// Compared to std::assignable_from<T&, T>:
//   - std::assignable_from: syntactic only (t = rv is well-formed, returns T&)
//   - MoveAssignable named req: additionally requires specific expression
//     forms and semantic postconditions
//
// std:: equivalent:
//   std::assignable_from<T&, T> — weaker, only syntax
// ============================================================================
template <class T>
concept move_assignable = std::assignable_from<T&, T> && requires(T& target, T&& source) {
  { target = std::move(source) } -> std::same_as<T&>;
};

// ============================================================================
// Named Requirement: CopyAssignable
// https://en.cppreference.com/w/cpp/named_req/CopyAssignable
//
// Expressed as a C++20 concept. The named requirement includes semantic
// postconditions (value equivalence, source unchanged) that concepts cannot
// express.
//
// Compared to std::assignable_from<T&, const T&>:
//   - std::assignable_from: syntactic only
//   - CopyAssignable named req: additionally requires MoveAssignable and
//     specific expression forms for lvalue, const lvalue, and const rvalue
//
// std:: equivalent:
//   std::assignable_from<T&, const T&> — weaker, only syntax
// ============================================================================
template <class T>
concept copy_assignable =
    move_assignable<T> && requires(T& target, T& source, const T& const_source, const T&& const_rvalue) {
      { target = source } -> std::same_as<T&>;
      { target = const_source } -> std::same_as<T&>;
      { target = std::move(const_rvalue) } -> std::same_as<T&>;
    };

}  // namespace chaistl::concepts
