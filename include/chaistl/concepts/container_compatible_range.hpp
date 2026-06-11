// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <concepts>
#include <ranges>

namespace chaistl::concepts {

// Reference:
// - https://eel.is/c++draft/container.intro.reqmts
//
// The standard uses container-compatible-range as an exposition-only concept for
// C++23 range-aware container constructors and modifiers.
template <class R, class T>
concept container_compatible_range =
    std::ranges::input_range<R> && std::convertible_to<std::ranges::range_reference_t<R>, T>;

}  // namespace chaistl::concepts
