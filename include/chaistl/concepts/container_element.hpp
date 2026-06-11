// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace chaistl::concepts {

// ============================================================================
// Project-specific helper (no std equivalent)
// ============================================================================

// The Allocator::value_type gate must come first and short-circuit:
// std::allocator_traits' primary template is not SFINAE-friendly, so
// naming allocator_traits<NotAnAllocator>::value_type hard-errors inside
// the trait instead of failing this constraint. Constraint conjunctions
// are checked left to right with short-circuiting ([temp.constr.op]), so
// non-allocators (e.g. a comparator deduced into an Allocator slot by a
// CTAD synthesized guide) fail cleanly on the first atom.
template <class Allocator, class T>
concept allocator_value_for = requires { typename Allocator::value_type; } && requires {
  typename std::allocator_traits<Allocator>::value_type;
} && std::same_as<typename std::allocator_traits<Allocator>::value_type, T>;

// ============================================================================
// Project-specific concept
//
// Container value types are generally required to be Erasable. Individual
// operations add stricter requirements such as DefaultInsertable,
// MoveInsertable, or CopyInsertable.
//
// This is a project-specific gate, not a named requirement. It combines
// std::is_object_v (excludes references, void, functions) with the
// Destructible named requirement.
// ============================================================================
template <class T>
concept container_element = std::is_object_v<T> && std::destructible<T>;

// ============================================================================
// Named Requirement: Erasable
// https://en.cppreference.com/w/cpp/named_req/Erasable
//
// Expressed as a C++20 concept. No std:: equivalent exists.
//
// Semantic boundary: this checks that allocator_traits can name a destroy
// expression. It cannot prove that the pointer denotes a live object in storage
// owned by the container.
// ============================================================================
template <class T, class Allocator = std::allocator<T>>
concept erasable =
    container_element<T> && allocator_value_for<Allocator, T> &&
    requires(Allocator allocator, T* pointer) { std::allocator_traits<Allocator>::destroy(allocator, pointer); };

// ============================================================================
// Named Requirement: EmplaceConstructible
// https://en.cppreference.com/w/cpp/named_req/EmplaceConstructible
//
// Expressed as a C++20 concept. No std:: equivalent exists.
//
// Semantic boundary: cppreference phrases this through allocator_traits::construct.
// We also require std::constructible_from<T, Args...> because common
// allocator_traits declarations can be too permissive inside requires-expressions.
// Value equivalence after construction remains a behavioral test concern.
// ============================================================================
template <class T, class Allocator = std::allocator<T>, class... Args>
concept emplace_constructible =
    allocator_value_for<Allocator, T> && std::constructible_from<T, Args...> &&
    requires(Allocator allocator, T* pointer, Args&&... args) {
      std::allocator_traits<Allocator>::construct(allocator, pointer, std::forward<Args>(args)...);
    };

// ============================================================================
// Named Requirement: DefaultInsertable
// https://en.cppreference.com/w/cpp/named_req/DefaultInsertable
//
// Expressed as a C++20 concept. No std:: equivalent exists.
// ============================================================================
template <class T, class Allocator = std::allocator<T>>
concept default_insertable = emplace_constructible<T, Allocator>;

// ============================================================================
// Named Requirement: MoveInsertable
// https://en.cppreference.com/w/cpp/named_req/MoveInsertable
//
// Expressed as a C++20 concept. No std:: equivalent exists.
// ============================================================================
template <class T, class Allocator = std::allocator<T>>
concept move_insertable = emplace_constructible<T, Allocator, T&&>;

// ============================================================================
// Named Requirement: CopyInsertable
// https://en.cppreference.com/w/cpp/named_req/CopyInsertable
//
// Expressed as a C++20 concept. No std:: equivalent exists.
// ============================================================================
template <class T, class Allocator = std::allocator<T>>
concept copy_insertable = move_insertable<T, Allocator> && emplace_constructible<T, Allocator, const T&>;

}  // namespace chaistl::concepts
