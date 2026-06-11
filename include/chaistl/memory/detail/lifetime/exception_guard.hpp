// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <utility>

namespace chaistl::detail {

/**
 * @brief RAII helper for strong exception safety.
 *
 * Stores a rollback function that is called if the guard is destroyed
 * without being explicitly completed. Once @ref complete() is called,
 * the rollback is disabled.
 *
 * Typical usage pattern:
 * @code
 *   auto guard = make_exception_guard([&] { rollback(); });
 *   // ... operations that might throw ...
 *   guard.complete();
 * @endcode
 *
 * @ingroup detail
 *
 * Reference:
 * - libc++ __exception_guard:
 *   https://github.com/llvm/llvm-project/blob/main/libcxx/include/__utility/exception_guard.h
 */
template <class Func>
class exception_guard {
 public:
  explicit constexpr exception_guard(Func func) : func_(std::move(func)) {}

  exception_guard(const exception_guard&) = delete;
  exception_guard& operator=(const exception_guard&) = delete;

  constexpr exception_guard(exception_guard&& other) noexcept
      : func_(std::move(other.func_)), completed_(other.completed_) {
    other.completed_ = true;
  }

  /**
   * @brief Mark the guarded operation as successfully completed.
   *
   * After this call, the rollback function will not be invoked
   * when the guard is destroyed.
   */
  constexpr void complete() noexcept { completed_ = true; }

  /**
   * @brief Invoke the rollback function unless complete() was called.
   */
  constexpr ~exception_guard() {
    if (!completed_) {
      func_();
    }
  }

 private:
  Func func_;
  bool completed_ = false;
};

/**
 * @brief Factory for @ref exception_guard.
 */
template <class Func>
[[nodiscard]] constexpr auto make_exception_guard(Func&& func) {
  return exception_guard<std::decay_t<Func>>(std::forward<Func>(func));
}

}  // namespace chaistl::detail
