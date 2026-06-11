// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <type_traits>

namespace chaistl::meta {

// ============================================================================
// Traits with no std:: equivalent (proposed or absent from the standard)
// ============================================================================

/**
 * @brief Detects whether a type can be relocated via bitwise copy.
 *
 * A type is trivially relocatable if its lifetime can be ended at one address
 * and restarted at another via a bitwise copy (e.g. memmove). This is a
 * stronger property than movability but weaker than trivial copyability.
 *
 * The default implementation conservatively returns true only for trivially
 * copyable types. Users may specialize this variable template for their own
 * types that are relocatable but not trivially copyable (e.g. std::unique_ptr).
 *
 * P1144 proposes std::is_trivially_relocatable for a future standard.
 * Until then, this trait fills the gap.
 *
 * @warning Specializing this trait for a type that is not actually trivially
 * relocatable is undefined behavior.
 */
template <class T>
inline constexpr bool is_trivially_relocatable_v = std::is_trivially_copyable_v<T>;

template <class T>
inline constexpr bool is_trivially_relocatable_v<T[]> = false;

template <class T, std::size_t N>
inline constexpr bool is_trivially_relocatable_v<T[N]> = is_trivially_relocatable_v<T>;

/**
 * @brief Detects whether a type is a specialization of a class template.
 *
 * For type-only template parameters.
 *
 * No std:: equivalent exists (C++26 may add std::is_specialization_of).
 */
template <class T, template <class...> class Template>
struct is_specialization_of : std::false_type {};

template <template <class...> class Template, class... Args>
struct is_specialization_of<Template<Args...>, Template> : std::true_type {};

template <class T, template <class...> class Template>
inline constexpr bool is_specialization_of_v = is_specialization_of<T, Template>::value;

// ============================================================================
// Project-specific detection traits
// ============================================================================

/**
 * @brief Detects whether a type has the syntactic surface of an allocator.
 *
 * This is a lightweight type predicate used during CTAD to disambiguate
 * allocator arguments from comparator arguments. It intentionally does not
 * require a particular return type from allocate() and does not require
 * default constructibility, so it can be evaluated on incomplete or abstract
 * allocator-like types.
 *
 * No std:: equivalent exists.
 */
template <class T>
inline constexpr bool is_allocator_v = requires(T& a) {
  typename T::value_type;
  { a.allocate(std::size_t{}) };
};

// ============================================================================
// Thin wrappers: shorthand for frequently-used std::allocator_traits members
//
// These avoid repeating the long std::allocator_traits<Alloc>::xxx::value
// expression throughout container code.
// ============================================================================

/**
 * @brief Shorthand for std::allocator_traits<Alloc>::is_always_equal::value
 */
template <class Alloc>
inline constexpr bool allocator_is_always_equal_v = std::allocator_traits<Alloc>::is_always_equal::value;

/**
 * @brief Shorthand for std::allocator_traits<Alloc>::propagate_on_container_copy_assignment::value
 */
template <class Alloc>
inline constexpr bool propagate_on_container_copy_assignment_v =
    std::allocator_traits<Alloc>::propagate_on_container_copy_assignment::value;

/**
 * @brief Shorthand for std::allocator_traits<Alloc>::propagate_on_container_move_assignment::value
 */
template <class Alloc>
inline constexpr bool propagate_on_container_move_assignment_v =
    std::allocator_traits<Alloc>::propagate_on_container_move_assignment::value;

/**
 * @brief Shorthand for std::allocator_traits<Alloc>::propagate_on_container_swap::value
 */
template <class Alloc>
inline constexpr bool propagate_on_container_swap_v = std::allocator_traits<Alloc>::propagate_on_container_swap::value;

// ============================================================================
// Project-specific composite traits
//
// These combine multiple std:: traits and chaistl::meta wrappers into
// single predicates used by container noexcept specifications.
// ============================================================================

/**
 * @brief True when container move assignment is guaranteed noexcept.
 *
 * A container's move-assignment operator is noexcept unless:
 *   - the allocator does NOT propagate on move assignment (POCMA is false),
 *   - allocators do NOT always compare equal (IAE is false), AND
 *   - the element type's move constructor can throw.
 *
 * In that one case, elements must be copied instead of moved, and the
 * copy may throw. This trait captures that three-condition logic so
 * every container does not have to repeat it.
 *
 * Matches the de-facto noexcept specification used by both libc++ and
 * libstdc++ (which add is_nothrow_move_constructible to the two-condition
 * form required by the C++ standard).
 *
 * @tparam Alloc  The allocator type.
 * @tparam T      The container's value_type.
 */
template <class Alloc, class T>
inline constexpr bool is_nothrow_container_move_assignable_v =
    propagate_on_container_move_assignment_v<Alloc> || allocator_is_always_equal_v<Alloc> ||
    std::is_nothrow_move_constructible_v<T>;

/**
 * @brief True when container swap is guaranteed noexcept.
 *
 * Swap exchanges internal pointers and (when POCS is true) allocators.
 * It never allocates or constructs elements, so the noexcept condition
 * depends only on whether the two allocators can safely be swapped.
 *
 * @tparam Alloc  The allocator type.
 */
template <class Alloc>
inline constexpr bool is_nothrow_container_swappable_v =
    propagate_on_container_swap_v<Alloc> || allocator_is_always_equal_v<Alloc>;

// ============================================================================
// Type transforms with no std:: equivalent
// ============================================================================

/**
 * @brief Copies cv-qualifiers from From to To.
 *
 * No std:: equivalent exists.
 */
template <class From, class To>
struct copy_cv {
  using type = To;
};

template <class From, class To>
struct copy_cv<const From, To> {
  using type = std::add_const_t<To>;
};

template <class From, class To>
struct copy_cv<volatile From, To> {
  using type = std::add_volatile_t<To>;
};

template <class From, class To>
struct copy_cv<const volatile From, To> {
  using type = std::add_cv_t<To>;
};

template <class From, class To>
using copy_cv_t = typename copy_cv<From, To>::type;

/**
 * @brief Copies cv-qualifiers and reference category from From to To.
 *
 * If From is an rvalue reference, the result is an rvalue reference to
 * a cv-qualified version of To. If From is an lvalue reference (or not
 * a reference at all), the result is an lvalue reference.
 *
 * No std:: equivalent exists.
 */
template <class From, class To>
using copy_cvref_t = std::conditional_t<
    std::is_rvalue_reference_v<From>,
    std::add_rvalue_reference_t<copy_cv_t<std::remove_reference_t<From>, std::remove_reference_t<To>>>,
    std::add_lvalue_reference_t<copy_cv_t<std::remove_reference_t<From>, std::remove_reference_t<To>>>>;

// ============================================================================
// Reserved extension points
// ============================================================================
//
// When future container categories are added, define corresponding traits
// here.  The libc++ / libstdc++ equivalents are noted for each.
//
// Grouped properties use a struct (like allocator_traits).  Standalone
// type properties use a _v variable template.
//
// Flat containers (libc++ __container_traits):
//   template <class Container>
//   struct container_traits {
//       static constexpr bool reservable      = false;  // has reserve()?
//       static constexpr bool strong_emplace  = false;  // insert/emplace
//                                                        // has strong
//                                                        // exception safety
//   };
//
// String / text (libstdc++ __is_char / __is_byte):
//   is_char_like_type_v<T>               — narrow character + std::byte
//                                          detection for memchr optimisation
//
// Algorithm optimisations (libstdc++ __memcpyable / __is_memcmp_ordered):
//   is_memcmp_ordered_v<T>               — unsigned integers on big-endian,
//                                          narrow chars + std::byte on LE
//   is_memcpyable_v<From, To>            — safe to memcpy between these
//                                          types (guards volatile, bool,
//                                          padding bits)
//
// Storage optimisation (libc++ __datasizeof):
//   datasizeof_v<T>                      — sizeof(T) excluding tail padding

}  // namespace chaistl::meta
