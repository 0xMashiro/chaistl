// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>

// ============================================================================
// inplace_vector — placeholder for P0843R14 (C++26)
// ============================================================================
//
// Proposal:  P0843R14 — inplace_vector
// Status:    Adopted for C++26; libstdc++ implemented July 2025 (PR119137)
// Reference: https://wg21.link/P0843
//
// inplace_vector<T, N> is a sequence container with fixed capacity N,
// storing elements inline (no dynamic allocation). It is essentially a
// vector with a statically-allocated backing array.
//
// Key design challenges for implementation:
//   1. constexpr support requires careful lifetime management of the
//      inline storage. libstdc++ uses an anonymous union with an array
//      and std::start_lifetime_as_array (C++23) for runtime paths.
//   2. For compile-time evaluation, trivial types must be explicitly
//      initialized (e.g. _M_elems[__i] = _Tp()) because constexpr
//      cannot leave objects uninitialized.
//   3. A zero-capacity specialization (N == 0) must be empty and
//      trivially copyable regardless of T.
//   4. Size type can be shrunk to save space: unsigned char/short/int
//      when alignment allows and N fits.
//   5. Exception safety: since there's no allocation, strong exception
//      guarantees are simpler but still require care during element
//      relocation (insert/erase in the middle).
//
// When implemented, this should go under chaistl::experimental::inplace_vector
// and follow the same patterns as vector.hpp (concepts, ranges, constexpr).
//
// Dependencies to reuse from chaistl:
//   - detail::uninitialized_storage_builder (for exception-safe construction)
//   - detail::constructed_range_guard
//   - concepts::container_element, concepts::allocator_for
//   - std::from_range_t, ranges support
//
// Note: P3074R7 (trivial unions) would further simplify constexpr
// implementation but is not yet adopted at time of writing.
//
// TODO: Implement inplace_vector<T, N> primary template and
//       inplace_vector<T, 0> specialization.
//
namespace chaistl {
namespace experimental {

// Placeholder — implementation pending.

template <class T, std::size_t N>
class inplace_vector;

}  // namespace experimental
}  // namespace chaistl
