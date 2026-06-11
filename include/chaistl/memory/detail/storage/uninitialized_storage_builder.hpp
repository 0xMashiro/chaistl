// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/memory/detail/lifetime/construction_transaction.hpp>
#include <chaistl/memory/detail/storage/raw_storage_buffer.hpp>

#include <memory>
#include <utility>

namespace chaistl::detail {

/**
 * @brief Owns uninitialized storage while tracking constructed objects.
 *
 * This is the workflow-level API intended for container implementations.
 * It combines raw storage ownership with construction rollback, so callers can
 * express the desired layout without spelling out exception cleanup.
 *
 * The storage is released only after all construction has succeeded. If this
 * object is destroyed before @ref release(), constructed objects are destroyed
 * and the raw storage is deallocated.
 */
template <class T, class Allocator>
class uninitialized_storage_builder {
 public:
  using allocator_type = Allocator;
  using allocator_traits = std::allocator_traits<allocator_type>;
  using pointer = typename allocator_traits::pointer;
  using size_type = typename allocator_traits::size_type;

  constexpr uninitialized_storage_builder(allocator_type& alloc, size_type capacity)
      : storage_(alloc, capacity), transaction_(alloc) {}

  uninitialized_storage_builder(const uninitialized_storage_builder&) = delete;
  uninitialized_storage_builder& operator=(const uninitialized_storage_builder&) = delete;

  [[nodiscard]] constexpr pointer data() const noexcept { return storage_.data(); }
  [[nodiscard]] constexpr size_type capacity() const noexcept { return storage_.capacity(); }

  template <class... Args>
  constexpr void construct_at(pointer pos, Args&&... args) {
    transaction_.construct_at(pos, std::forward<Args>(args)...);
  }

  template <class InputIt>
  constexpr pointer uninitialized_copy(InputIt first, InputIt last, pointer result) {
    return transaction_.uninitialized_copy(first, last, result);
  }

  template <class InputIt>
  constexpr pointer uninitialized_move_if_noexcept(InputIt first, InputIt last, pointer result) {
    return transaction_.uninitialized_move_if_noexcept(first, last, result);
  }

  template <class Size, class Value>
  constexpr pointer uninitialized_fill_n(pointer first, Size count, const Value& value) {
    return transaction_.uninitialized_fill_n(first, count, value);
  }

  template <class Size>
  constexpr pointer uninitialized_default_construct_n(pointer first, Size count) {
    return transaction_.uninitialized_default_construct_n(first, count);
  }

  /**
   * @brief Complete construction rollback and transfer raw storage ownership.
   *
   * After this call the caller owns the returned storage and must eventually
   * destroy the constructed objects and deallocate the storage.
   */
  [[nodiscard]] constexpr pointer release() noexcept {
    transaction_.complete();
    return storage_.release();
  }

 private:
  raw_storage_buffer<T, allocator_type> storage_;
  construction_transaction<allocator_type, pointer, T> transaction_;
};

}  // namespace chaistl::detail
