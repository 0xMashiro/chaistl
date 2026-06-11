// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <type_traits>
#include <utility>

namespace chaistl::detail {

/**
 * @brief Owns a contiguous block of uninitialized storage obtained from an allocator.
 *
 * @ingroup memory
 *
 * Invariant:
 * - data_ == nullptr  <=>  capacity_ == 0 or ownership has been released.
 * - Destructor deallocates raw storage; it does **not** destroy elements.
 *
 * This type intentionally knows nothing about which objects have been constructed.
 * Element lifetime is managed separately, e.g. by @ref construction_transaction.
 *
 * Typical usage pattern:
 * @code
 *   raw_storage_buffer<T, Allocator> storage(allocator, new_capacity);
 *   // ... construct elements into storage.data() ...
 *   pointer p = storage.release();  // caller takes over ownership
 * @endcode
 */
template <class T, class Allocator>
class raw_storage_buffer {
 public:
  using allocator_type = Allocator;
  using allocator_traits = std::allocator_traits<allocator_type>;
  using pointer = typename allocator_traits::pointer;
  using size_type = typename allocator_traits::size_type;

  /**
   * @brief Allocate storage for @p capacity objects.
   *
   * If @p capacity is zero, no allocation occurs and @ref data() returns nullptr.
   */
  constexpr raw_storage_buffer(allocator_type& alloc, size_type capacity)
      : allocator_(std::addressof(alloc)), data_(), capacity_(capacity) {
    if (capacity_ > 0) [[likely]] {
      data_ = allocator_traits::allocate(*allocator_, capacity_);
    }
  }

  raw_storage_buffer(const raw_storage_buffer&) = delete;
  raw_storage_buffer& operator=(const raw_storage_buffer&) = delete;

  constexpr raw_storage_buffer(raw_storage_buffer&& other) noexcept
      : allocator_(other.allocator_), data_(other.data_), capacity_(other.capacity_) {
    other.data_ = pointer();
    other.capacity_ = 0;
  }

  constexpr raw_storage_buffer& operator=(raw_storage_buffer&& other) noexcept {
    if (this != std::addressof(other)) {
      reset();
      allocator_ = other.allocator_;
      data_ = other.data_;
      capacity_ = other.capacity_;
      other.data_ = pointer();
      other.capacity_ = 0;
    }
    return *this;
  }

  /**
   * @brief Deallocate the owned storage.
   *
   * Precondition: no live objects remain in the storage.  The caller must
   * destroy any constructed elements before the destructor runs.
   */
  constexpr ~raw_storage_buffer() { reset(); }

  [[nodiscard]] constexpr pointer data() const noexcept { return data_; }
  [[nodiscard]] constexpr size_type capacity() const noexcept { return capacity_; }

  /**
   * @brief Relinquish ownership of the storage.
   *
   * Postcondition: @ref data() == nullptr, @ref capacity() == 0.
   *
   * @return The previously owned pointer.
   */
  [[nodiscard]] constexpr pointer release() noexcept {
    pointer p = data_;
    data_ = pointer();
    capacity_ = 0;
    return p;
  }

  /**
   * @brief Deallocate the owned storage immediately.
   *
   * Postcondition: @ref data() == nullptr, @ref capacity() == 0.
   */
  constexpr void reset() noexcept {
    if (data_ != pointer()) [[likely]] {
      allocator_traits::deallocate(*allocator_, data_, capacity_);
      data_ = pointer();
      capacity_ = 0;
    }
  }

 private:
  allocator_type* allocator_;
  pointer data_;
  size_type capacity_;
};

}  // namespace chaistl::detail
