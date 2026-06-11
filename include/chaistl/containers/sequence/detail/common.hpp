// SPDX-License-Identifier: Apache-2.0

#pragma once

// =============================================================================
// Common includes for sequence container implementations.
//
// This header aggregates the standard library and chaistl headers that
// virtually all sequence containers need. Including it keeps individual
// container headers focused on their unique dependencies only.
//
// Dependencies provided:
//   - Standard library: algorithm, concepts, cstddef, initializer_list,
//     iterator, memory, ranges, stdexcept, type_traits, utility
//   - chaistl concepts: allocator, container_compatible_range, library_wide
//   - chaistl memory: allocator
//   - chaistl meta: allocator propagation traits
//   - chaistl iterators: reverse_iterator adapter
//   - chaistl algorithms: comparison (default_three_way_compare)
//
// Containers that need additional headers (e.g. <compare>, <memory_resource>,
// <tuple>) should include them explicitly after this header.
// =============================================================================

// --- Standard library --------------------------------------------------------
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

// --- chaistl concepts --------------------------------------------------------
#include <chaistl/concepts/allocator.hpp>
#include <chaistl/concepts/container_compatible_range.hpp>
#include <chaistl/concepts/library_wide.hpp>

// --- chaistl memory ----------------------------------------------------------
#include <chaistl/memory/allocator.hpp>

// --- chaistl meta ------------------------------------------------------------
#include <chaistl/meta/type_traits.hpp>

// --- chaistl iterators -------------------------------------------------------
#include <chaistl/iterator/adapter/reverse.hpp>

// --- chaistl algorithms ------------------------------------------------------
#include <chaistl/algorithm/comparison.hpp>
