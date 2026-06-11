// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace chaistl {

// ============================================================================
// chaistl::hash — a constexpr, inspectable hash function object.
//
// Why this exists (ADR: Hash Table Design):
//
//   - std::hash is deliberately NOT constexpr. Cpp17Hash only guarantees
//     consistent results within one program execution, so salted/randomized
//     implementations are conforming, and P3372 (constexpr containers, C++26)
//     explicitly leaves std::hash alone. Constant-evaluated use of the
//     unordered containers therefore requires a user-supplied hasher — this
//     is that hasher.
//   - A teaching library should not present hashing as a black box. FNV-1a
//     is small enough to read in one sitting and good enough to exercise a
//     prime-bucket table.
//
// chaistl::hash is NOT the default Hash of any chaistl container — the
// standard surface keeps std::hash<Key>. Unlike a conforming std::hash, the
// FNV-1a result is deterministic across program runs; do not rely on that
// for anything but tests and teaching.
//
// Scope is deliberately minimal (decision 2026-06-11): integral, enumeration,
// and pointer keys plus char-based strings. No hash_combine, no range
// hashing, no floating-point support until the table itself is proven.
// ============================================================================

namespace detail::fnv {

// 64-bit FNV-1a parameters, with the 32-bit pair for narrow size_t targets.
inline constexpr std::size_t offset_basis = sizeof(std::size_t) == 8 ? 0xcbf29ce484222325ull : 0x811c9dc5ull;
inline constexpr std::size_t prime = sizeof(std::size_t) == 8 ? 0x100000001b3ull : 0x01000193ull;

[[nodiscard]] constexpr std::size_t append_byte(std::size_t state, unsigned char byte) noexcept {
  return (state ^ byte) * prime;
}

/// Hashes the object representation of a trivially copyable, non-pointer value.
template <class T>
  requires std::is_trivially_copyable_v<T> && (!std::is_pointer_v<T>)
[[nodiscard]] constexpr std::size_t hash_representation(const T& value) noexcept {
  const auto bytes = std::bit_cast<std::array<unsigned char, sizeof(T)>>(value);
  std::size_t state = offset_basis;
  for (unsigned char byte : bytes) {
    state = append_byte(state, byte);
  }
  return state;
}

}  // namespace detail::fnv

/**
 * @brief Primary template: declared but not defined.
 *
 * Like the disabled std::hash specializations, using chaistl::hash with an
 * unsupported key type is a compile-time error.
 */
template <class T>
struct hash;

/// Integral keys (including bool and character types).
template <std::integral T>
struct hash<T> {
  [[nodiscard]] constexpr std::size_t operator()(T value) const noexcept {
    return detail::fnv::hash_representation(value);
  }
};

/// Enumeration keys hash as their underlying type.
template <class T>
  requires std::is_enum_v<T>
struct hash<T> {
  [[nodiscard]] constexpr std::size_t operator()(T value) const noexcept {
    return detail::fnv::hash_representation(std::to_underlying(value));
  }
};

/**
 * @brief Pointer keys hash by address.
 *
 * Not usable in constant evaluation: an object's address is not a constant,
 * and bit-casting pointers to integers is excluded from constexpr — the same
 * wall std::hash<T*> faces. This is the one chaistl::hash specialization
 * without the constexpr advertisement.
 */
template <class T>
struct hash<T*> {
  [[nodiscard]] std::size_t operator()(T* pointer) const noexcept {
    return detail::fnv::hash_representation(reinterpret_cast<std::uintptr_t>(pointer));
  }
};

/// Char-based string content (the natural transparent partner of string keys).
template <>
struct hash<std::string_view> {
  [[nodiscard]] constexpr std::size_t operator()(std::string_view value) const noexcept {
    std::size_t state = detail::fnv::offset_basis;
    for (char character : value) {
      state = detail::fnv::append_byte(state, static_cast<unsigned char>(character));
    }
    return state;
  }
};

/// std::string hashes as its content, consistently with hash<std::string_view>.
template <>
struct hash<std::string> {
  [[nodiscard]] constexpr std::size_t operator()(const std::string& value) const noexcept {
    return hash<std::string_view>{}(std::string_view(value));
  }
};

}  // namespace chaistl
