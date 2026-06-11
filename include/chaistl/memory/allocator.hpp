// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <concepts>
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>

namespace chaistl {

template <class Pointer>
struct allocation_result {
  Pointer ptr;
  std::size_t count;
};

// References:
// - Allocator named requirement:
//   https://en.cppreference.com/w/cpp/named_req/Allocator
// - std::allocator:
//   https://en.cppreference.com/w/cpp/memory/allocator
//
// chaistl::allocator is intentionally small and stateless. It exposes the
// minimum allocator surface needed by allocator_traits and containers, while
// leaving construction/destruction to allocator_traits fallback paths.
//
// Allocation and object lifetime are separate steps:
//
//   allocate(3)
//      |
//      v
//   [ raw ][ raw ][ raw ]       storage is only bytes with T's alignment
//
//   construct(p + 1, value)
//      |
//      v
//   [ raw ][  T  ][ raw ]       only the middle slot is a live T object
//
//   destroy(p + 1)
//      |
//      v
//   [ raw ][ raw ][ raw ]       the bytes remain, but the T lifetime ended
//
//   deallocate(p, 3)
//      |
//      v
//   storage returned to operator delete
template <class T>
class allocator {
 public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using propagate_on_container_move_assignment = std::true_type;
  using is_always_equal = std::true_type;

  template <class U>
  struct rebind {
    using other = allocator<U>;
  };

  constexpr allocator() noexcept = default;

  template <class U>
  constexpr allocator(const allocator<U>&) noexcept {}

  [[nodiscard]] constexpr T* allocate(size_type count) {
    if (count > max_size()) [[unlikely]] {
      throw std::bad_array_new_length();
    }
    if (count == 0) {
      return nullptr;
    }
    if consteval {
      // Aligned operator new is not usable in constant evaluation; delegate
      // to std::allocator, whose allocations are constexpr-blessed.
      return std::allocator<T>{}.allocate(count);
    }
    return static_cast<T*>(::operator new(count * sizeof(T), std::align_val_t{alignof(T)}));
  }

  constexpr void deallocate(T* pointer, size_type count) noexcept {
    if (pointer == nullptr) [[unlikely]] {
      return;
    }
    if consteval {
      std::allocator<T>{}.deallocate(pointer, count);
      return;
    }
    ::operator delete(pointer, count * sizeof(T), std::align_val_t{alignof(T)});
  }

  [[nodiscard]] constexpr allocation_result<T*> allocate_at_least(size_type count) { return {allocate(count), count}; }

  [[nodiscard]] static constexpr size_type max_size() noexcept {
    return std::numeric_limits<size_type>::max() / sizeof(T);
  }
};

template <class T, class U>
[[nodiscard]] constexpr bool operator==(const allocator<T>&, const allocator<U>&) noexcept {
  return true;
}

}  // namespace chaistl
