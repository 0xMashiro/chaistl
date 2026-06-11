// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/memory/detail/lifetime/allocator_uninitialized.hpp>
#include <chaistl/utility/config.hpp>

#include <cassert>
#include <cstddef>
#include <exception>
#include <memory>
#include <type_traits>
#include <utility>

namespace chaistl::detail {

/**
 * @brief Tracks successfully-constructed objects and rolls them back on exception.
 *
 * @ingroup memory
 *
 * The class has three compilation tiers selected by element type traits:
 *   1. is_trivially_copyable (bulk copy/move fast path):
 *      zero overhead — range tracking and the destructor are no-ops. Copy/move
 *      can use memmove; default construction still respects non-trivial default
 *      constructors.
 *      NOTE: P1144 proposes std::is_trivially_relocatable, which is a stronger
 *      property. We conservatively use is_trivially_copyable here; when P1144
 *      is adopted this can be refined.
 *   2. !is_trivially_copyable && is_trivially_destructible: element-by-element
 *      construction (non-trivial copy/move) but transaction tracking is a no-op
 *      because the destructor is trivial — no rollback work to do.
 *   3. needs_rollback (non-trivial destructor): full tracking — each construction
 *      range is registered and rolled back in reverse order on exception.
 *
 * This class is intentionally decoupled from @ref raw_storage_buffer.
 * It knows nothing about allocation; it only manages object lifetimes.
 *
 * Typical usage pattern:
 * @code
 *   construction_transaction<Allocator, Pointer> tx(allocator);
 *   tx.construct_at(pos, args...);           // registers [pos, pos+1)
 *   tx.uninitialized_move_if_noexcept(first, last, dest);  // registers moved range
 *   tx.complete();                           // disarm rollback
 * @endcode
 *
 * @tparam Allocator  An allocator type.
 * @tparam Pointer    The allocator's pointer type (must support pointer arithmetic).
 * @tparam ValueType  The element type being constructed (used to detect trivially
 *                    relocatable types and enable zero-overhead fast path).
 */
template <class Allocator, class Pointer, class ValueType>
class construction_transaction {
 public:
  using allocator_type = Allocator;
  using allocator_traits = std::allocator_traits<allocator_type>;
  using pointer = Pointer;
  using value_type = ValueType;

  /// True for types whose copy/move construction can use the bulk memmove path.
  /// NOTE: P1144 proposes a stricter std::is_trivially_relocatable trait.
  /// We conservatively use is_trivially_copyable here; refine when P1144 lands.
  static constexpr bool is_trivially_copyable = std::is_trivially_copyable_v<value_type>;
  /// True when a throwing construction leaves objects that need destruction.
  /// False for trivially copyable types and for types with a trivial destructor
  /// (construction may throw but there is nothing to clean up).
  static constexpr bool needs_rollback = !is_trivially_copyable && !std::is_trivially_destructible_v<value_type>;
  static constexpr std::size_t max_ranges = 4;

  explicit constexpr construction_transaction(allocator_type& alloc) noexcept
      : allocator_(std::addressof(alloc)), range_count_(0), completed_(false) {}
  // ranges_ is default-initialized (indeterminate values for trivially-constructible
  // pointer types). This is safe: range_count_ == 0 ensures no slot is ever read
  // before being written through push_range().

  construction_transaction(const construction_transaction&) = delete;
  construction_transaction& operator=(const construction_transaction&) = delete;

  constexpr ~construction_transaction() {
    if constexpr (needs_rollback) {
      if (!completed_) [[unlikely]] {
        rollback();
      }
    }
  }

  /**
   * @brief Construct a single object at @p pos and register it.
   *
   * For trivially relocatable or trivially destructible types, registration is
   * skipped — no rollback is needed.
   */
  template <class... Args>
  constexpr void construct_at(Pointer pos, Args&&... args) {
    if constexpr (is_trivially_copyable) {
      allocator_traits::construct(*allocator_, std::to_address(pos), std::forward<Args>(args)...);
    } else {
      allocator_traits::construct(*allocator_, std::to_address(pos), std::forward<Args>(args)...);
      if constexpr (needs_rollback) {
        push_range(pos, pos + 1);
      }
    }
  }

  /**
   * @brief Copy-construct objects from [@p first, @p last) into [@p result, ...)
   * and register the constructed range.
   *
   * The is_trivially_copyable branch uses memmove; the else branch uses
   * element-by-element construction. needs_rollback gates range registration.
   */
  template <class InputIt>
  constexpr Pointer uninitialized_copy(InputIt first, InputIt last, Pointer result) {
    if constexpr (is_trivially_copyable) {
      return detail::uninitialized_allocator_copy(*allocator_, first, last, result);
    } else {
      Pointer new_last = detail::uninitialized_allocator_copy(*allocator_, first, last, result);
      if constexpr (needs_rollback) {
        push_range(result, new_last);
      }
      return new_last;
    }
  }

  /**
   * @brief Move-construct objects (move_if_noexcept) from [@p first, @p last)
   * into [@p result, ...) and register the constructed range.
   */
  template <class InputIt>
  constexpr Pointer uninitialized_move_if_noexcept(InputIt first, InputIt last, Pointer result) {
    if constexpr (is_trivially_copyable) {
      return detail::uninitialized_allocator_move_if_noexcept(*allocator_, first, last, result);
    } else {
      Pointer new_last = detail::uninitialized_allocator_move_if_noexcept(*allocator_, first, last, result);
      if constexpr (needs_rollback) {
        push_range(result, new_last);
      }
      return new_last;
    }
  }

  /**
   * @brief Fill-construct @p count copies of @p value starting at @p first
   * and register the constructed range.
   */
  template <class Size, class T>
  constexpr Pointer uninitialized_fill_n(Pointer first, Size count, const T& value) {
    if constexpr (is_trivially_copyable) {
      return detail::uninitialized_allocator_fill_n(*allocator_, first, count, value);
    } else {
      Pointer new_last = detail::uninitialized_allocator_fill_n(*allocator_, first, count, value);
      if constexpr (needs_rollback) {
        push_range(first, new_last);
      }
      return new_last;
    }
  }

  /**
   * @brief Default-construct @p count objects starting at @p first
   * and register the constructed range.
   */
  template <class Size>
  constexpr Pointer uninitialized_default_construct_n(Pointer first, Size count) {
    if constexpr (is_trivially_copyable) {
      return detail::uninitialized_allocator_default_construct_n(*allocator_, first, count);
    } else {
      Pointer new_last = detail::uninitialized_allocator_default_construct_n(*allocator_, first, count);
      if constexpr (needs_rollback) {
        push_range(first, new_last);
      }
      return new_last;
    }
  }

  /**
   * @brief Register a range of already-constructed objects for rollback.
   *
   * No-op when rollback is not needed.
   */
  constexpr void register_constructed_range(Pointer first, Pointer last) noexcept {
    if constexpr (needs_rollback) {
      push_range(first, last);
    }
  }

  /**
   * @brief Mark the transaction as complete; disable rollback on destruction.
   *
   * No-op when rollback is not needed.
   */
  constexpr void complete() noexcept {
    if constexpr (needs_rollback) {
      completed_ = true;
    }
  }

 private:
  struct range {
    Pointer first;
    Pointer last;
  };

  allocator_type* allocator_;
  range ranges_[max_ranges];
  std::size_t range_count_;
  bool completed_;

  CHAI_ALWAYS_INLINE constexpr void push_range(Pointer first, Pointer last) noexcept {
    if (first == last) return;
    assert(range_count_ < max_ranges &&
           "construction_transaction: too many ranges. Increase max_ranges or review usage.");
    if (range_count_ >= max_ranges) [[unlikely]] {
      std::terminate();
    }
    ranges_[range_count_] = {first, last};
    ++range_count_;
  }

  CHAI_COLD constexpr void rollback() noexcept {
    for (std::size_t i = range_count_; i > 0; --i) {
      detail::allocator_destroy_reverse(*allocator_, ranges_[i - 1].first, ranges_[i - 1].last);
    }
  }
};

/**
 * @brief Rolls back an already-owned uninitialized range under construction.
 *
 * Unlike @ref construction_transaction, this guard does not own storage and
 * does not register discrete ranges. It is intended for in-place container
 * growth where new objects are appended into spare capacity. The caller
 * advances @p last as construction succeeds; if the guard is not completed,
 * the constructed range [first, last) is destroyed in reverse order.
 */
template <class Allocator, class Pointer>
class constructed_range_guard {
 public:
  using allocator_type = Allocator;
  using pointer = Pointer;

  constexpr constructed_range_guard(allocator_type& alloc, pointer first, pointer& last) noexcept
      : allocator_(std::addressof(alloc)), first_(first), last_(std::addressof(last)) {}

  constructed_range_guard(const constructed_range_guard&) = delete;
  constructed_range_guard& operator=(const constructed_range_guard&) = delete;

  constexpr ~constructed_range_guard() {
    if (!completed_) {
      allocator_destroy_reverse(*allocator_, first_, *last_);
      *last_ = first_;
    }
  }

  constexpr void complete() noexcept { completed_ = true; }

 private:
  allocator_type* allocator_;
  pointer first_;
  pointer* last_;
  bool completed_ = false;
};

}  // namespace chaistl::detail
