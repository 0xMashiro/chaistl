// SPDX-License-Identifier: Apache-2.0

#pragma once

// Shared test-only policies for the ring_buffer suites.

#include <chaistl/experimental/containers/ring_buffer.hpp>

#include <utility>

namespace theirs {

// A third-party overwrite-style policy implemented outside chaistl through
// the public core_access protocol:
// a new policy must work with zero changes to ring_buffer.
struct evict_oldest_policy {
  using capacity_behavior = chaistl::experimental::detail::ring_buffer::fixed_capacity_tag;

  template <class Core, class T>
  static constexpr bool push_back_full(Core& core, T&& value) {
    if (core.capacity() == 0) {
      return false;
    }
    using access = typename Core::core_access;
    access::replace_at_logical(core, 0, std::forward<T>(value));
    access::advance_begin(core);
    return true;
  }

  template <class Core, class T>
  static constexpr bool push_front_full(Core& core, T&& value) {
    if (core.capacity() == 0) {
      return false;
    }
    using access = typename Core::core_access;
    access::retreat_begin(core);
    access::replace_at_logical(core, 0, std::forward<T>(value));
    return true;
  }

  template <class Core, class T>
  static constexpr bool insert_full(Core& core, typename Core::iterator pos, T&& value) {
    if (pos == core.begin()) {
      return false;
    }
    using access = typename Core::core_access;
    access::shift_tail_and_insert(core, pos, std::forward<T>(value));
    return true;
  }
};

}  // namespace theirs
