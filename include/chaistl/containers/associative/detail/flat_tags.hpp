// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

namespace chaistl {

struct sorted_unique_t {
  explicit sorted_unique_t() = default;
};

inline constexpr sorted_unique_t sorted_unique{};

struct sorted_equivalent_t {
  explicit sorted_equivalent_t() = default;
};

inline constexpr sorted_equivalent_t sorted_equivalent{};

namespace detail {

/// Satisfied when every underlying container of a flat adaptor supports
/// uses-allocator construction with `Alloc`.
template <class Alloc, class... Containers>
concept flat_uses_allocator = (std::uses_allocator_v<Containers, Alloc> && ...);

}  // namespace detail

}  // namespace chaistl
