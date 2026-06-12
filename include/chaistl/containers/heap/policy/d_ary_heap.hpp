// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <span>
#include <utility>

namespace chaistl::heap_policy {

// ============================================================================
// d_ary_heap_policy — implicit array heap with configurable branching factor
//
// A d-ary heap generalizes the binary heap: children of i occupy
// [d*i + 1, d*i + d]. Larger arity shortens the height, often reducing cache
// misses on push/pop, at the cost of scanning more children per sift-down.
//
//   push O(log_d n), pop O(d log_d n), make O(n), top O(1)
//
// Arity must be at least 2. d_ary_heap_policy<2> is semantically equivalent
// to binary_heap_policy, but remains a separate policy for benchmarking.
// ============================================================================

template <std::size_t Arity>
struct d_ary_heap_policy {
  static_assert(Arity >= 2, "chaistl::heap_policy::d_ary_heap_policy requires Arity >= 2");

  template <class T, class Compare>
  static constexpr void push(std::span<T> heap, const Compare& cmp) {
    sift_up(heap, heap.size() - 1, cmp);
  }

  template <class T, class Compare>
  static constexpr void pop(std::span<T> heap, const Compare& cmp) {
    if (heap.size() <= 1) {
      return;
    }
    T displaced = std::move(heap.back());
    heap.back() = std::move(heap.front());
    sift_down(heap.first(heap.size() - 1), 0, std::move(displaced), cmp);
  }

  template <class T, class Compare>
  static constexpr void make(std::span<T> heap, const Compare& cmp) {
    if (heap.size() <= 1) {
      return;
    }
    std::size_t i = parent(heap.size() - 1);
    while (true) {
      sift_down(heap, i, std::move(heap[i]), cmp);
      if (i == 0) {
        return;
      }
      --i;
    }
  }

  template <class T, class Compare>
  [[nodiscard]] static constexpr bool verify(std::span<const T> heap, const Compare& cmp) {
    for (std::size_t i = 1; i < heap.size(); ++i) {
      if (cmp(heap[parent(i)], heap[i])) {
        return false;
      }
    }
    return true;
  }

 private:
  [[nodiscard]] static constexpr std::size_t parent(std::size_t i) noexcept { return (i - 1) / Arity; }

  [[nodiscard]] static constexpr std::size_t first_child(std::size_t i) noexcept { return Arity * i + 1; }

  template <class T, class Compare>
  static constexpr void sift_up(std::span<T> heap, std::size_t index, const Compare& cmp) {
    T value = std::move(heap[index]);
    while (index > 0 && cmp(heap[parent(index)], value)) {
      heap[index] = std::move(heap[parent(index)]);
      index = parent(index);
    }
    heap[index] = std::move(value);
  }

  template <class T, class Compare>
  static constexpr void sift_down(std::span<T> heap, std::size_t index, T value, const Compare& cmp) {
    const std::size_t size = heap.size();
    while (true) {
      const std::size_t first = first_child(index);
      if (first >= size) {
        break;
      }
      std::size_t best = first;
      const std::size_t last = first + Arity < size ? first + Arity : size;
      for (std::size_t child = first + 1; child < last; ++child) {
        if (cmp(heap[best], heap[child])) {
          best = child;
        }
      }
      // Keep the stop test in the d-ary heap. Floyd's bounce helps d=2 by
      // skipping one value-vs-child comparison per level; here each overshot
      // level costs a full scan of up to Arity children.
      if (!cmp(value, heap[best])) {
        break;
      }
      heap[index] = std::move(heap[best]);
      index = best;
    }
    heap[index] = std::move(value);
  }
};

}  // namespace chaistl::heap_policy
