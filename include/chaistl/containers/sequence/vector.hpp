// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// vector - Contiguous dynamic array container
// ============================================================================
//
// Architecture:
//   - Owns a single raw storage interval: [first_, capacity_last_).
//   - The live element interval is [first_, last_); iterators are allocator
//     pointers, so the container preserves the standard vector model of
//     contiguous traversal and direct data() access.
//   - Construction and reallocation are delegated to the memory/detail
//     transaction helpers so exception-safety rules stay visible in one place.
//
// Standardization archaeology:
//   - C++98 standardized vector as the general-purpose growable array, but LWG
//     69 later made contiguity a required property instead of an accident of
//     implementation. That decision is why vector can serve as the bridge
//     between C++ containers and C-array APIs.
//   - LWG 464 added data() access for empty vectors, closing a usability hole
//     where clients had no standard way to ask for the underlying pointer.
//   - LWG 96 and WG21/N1211 record the long-running vector<bool> debate: the
//     specialization traded normal element references and contiguity for bit
//     packing. chaistl deliberately does not provide that specialization.
//   - P0784/P1004 moved vector into the C++20 constexpr-container story by
//     allowing transient allocation during constant evaluation.
//   - P1206R7 added the C++23 range construction/insertion family that this
//     file mirrors with from_range, assign_range, insert_range, and append_range.
//
// Non-standard extensions:
//   - resize_and_overwrite mirrors the C++23 basic_string operation; no vector
//     equivalent has been standardized.
//   - Hardened unchecked access reports contract violations through
//     CHAI_HARDENED instead of silently relying on undefined behavior.
//
// References:
//   - C++ Draft: https://eel.is/c++draft/vector
//   - cppreference: https://en.cppreference.com/w/cpp/container/vector
//   - LWG defects: https://www.open-std.org/jtc1/sc22/wg21/docs/lwg-defects.html
//   - LWG 96 / vector<bool>: https://wg21.link/lwg96
//   - N1211 / vector<bool>: https://wg21.link/N1211
//   - P0784 / constexpr containers: https://wg21.link/P0784
//   - P1004 / constexpr vector: https://wg21.link/P1004
//   - P1206R7 / ranges to containers: https://wg21.link/P1206R7

#include <chaistl/containers/sequence/detail/common.hpp>
#include <chaistl/memory/detail/lifetime/allocator_uninitialized.hpp>
#include <chaistl/memory/detail/lifetime/construction_transaction.hpp>
#include <chaistl/memory/detail/lifetime/exception_guard.hpp>
#include <chaistl/memory/detail/storage/uninitialized_storage_builder.hpp>
#include <chaistl/memory/detail/utility/forward_like.hpp>
#include <chaistl/memory_resource.hpp>
#include <chaistl/utility/hardening.hpp>

namespace chaistl {

/**
 * @brief Contiguous dynamic array container.
 *
 * @ingroup containers_sequence
 *
 * Deliberate differences from `std::vector`:
 *
 * - `vector<bool>` is not specialized.
 * - The growth policy is intentionally simple and not standardized.
 * - Compatibility with pre-C++20 implementation techniques is not a goal.
 *
 * References:
 *
 * - C++ Draft Standard: https://eel.is/c++draft/vector
 * - cppreference: https://en.cppreference.com/w/cpp/container/vector
 */
template <concepts::container_element T, concepts::allocator_for<T> Allocator = allocator<T>>
class vector {
 public:
  // ======================================================================
  // Member types
  // ======================================================================
  using value_type = T;
  using allocator_type = Allocator;
  using allocator_traits = std::allocator_traits<allocator_type>;
  using size_type = typename allocator_traits::size_type;
  using difference_type = typename allocator_traits::difference_type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = typename allocator_traits::pointer;
  using const_pointer = typename allocator_traits::const_pointer;
  using iterator = pointer;
  using const_iterator = const_pointer;
  using reverse_iterator = chaistl::reverse_iterator<iterator>;
  using const_reverse_iterator = chaistl::reverse_iterator<const_iterator>;

  // ======================================================================
  // Constructors and assignment
  // ======================================================================
  constexpr vector() noexcept(noexcept(allocator_type{})) = default;
  explicit constexpr vector(const allocator_type& alloc) noexcept;
  explicit constexpr vector(size_type count, const allocator_type& alloc = allocator_type{});
  constexpr vector(size_type count, const T& value, const allocator_type& alloc = allocator_type{});

  // This overload participates in overload resolution only if InputIt
  // satisfies LegacyInputIterator (cppreference).
  // Type requirements:
  // - T must be EmplaceConstructible from *first.
  // - If InputIt is not a forward iterator, T must be MoveInsertable.
  template <std::input_iterator InputIt>
  constexpr vector(InputIt first, InputIt last, const allocator_type& alloc = allocator_type{});

  // Equivalent to vector(init.begin(), init.end(), alloc).
  constexpr vector(std::initializer_list<T> init, const allocator_type& alloc = allocator_type{});
  constexpr vector(const vector& other);
  constexpr vector(vector&& other) noexcept;
  constexpr vector(const vector& other, const std::type_identity_t<Allocator>& alloc);
  constexpr vector(vector&& other, const std::type_identity_t<Allocator>& alloc);

  // C++23 from-range construction.
  template <concepts::container_compatible_range<T> R>
  constexpr vector(std::from_range_t, R&& range, const allocator_type& alloc = allocator_type{});
  constexpr ~vector();

  constexpr vector& operator=(const vector& other);
  constexpr vector& operator=(vector&& other) noexcept(meta::is_nothrow_container_move_assignable_v<allocator_type, T>);
  constexpr vector& operator=(std::initializer_list<T> init);

  // This overload participates in overload resolution only if InputIt
  // satisfies LegacyInputIterator (cppreference).
  template <std::input_iterator InputIt>
  constexpr void assign(InputIt first, InputIt last);

  // C++23 ranges assign.
  template <concepts::container_compatible_range<T> R>
  constexpr void assign_range(R&& range);
  constexpr void assign(size_type count, const T& value);
  constexpr void assign(std::initializer_list<T> init);

  // ======================================================================
  // Element access
  // ======================================================================
  [[nodiscard]] constexpr auto&& at(this auto&& self, size_type pos) {
    if (pos >= self.size()) {
      throw std::out_of_range("chaistl::vector::at");
    }
    return self[pos];
  }

  [[nodiscard]] constexpr auto&& operator[](this auto&& self, size_type pos) noexcept {
    CHAI_HARDENED(pos < self.size(), "chaistl::vector::operator[]: out of bounds");
    return detail::forward_like<decltype(self)>(self.first_[pos]);
  }

  [[nodiscard]] constexpr auto&& front(this auto&& self) noexcept {
    CHAI_HARDENED(!self.empty(), "chaistl::vector::front: empty container");
    return detail::forward_like<decltype(self)>(*self.first_);
  }

  [[nodiscard]] constexpr auto&& back(this auto&& self) noexcept {
    CHAI_HARDENED(!self.empty(), "chaistl::vector::back: empty container");
    return detail::forward_like<decltype(self)>(*(self.last_ - 1));
  }

  [[nodiscard]] constexpr pointer data() noexcept { return std::to_address(first_); }
  [[nodiscard]] constexpr const_pointer data() const noexcept { return std::to_address(first_); }

  // ======================================================================
  // Iterators
  // ======================================================================
  [[nodiscard]] constexpr iterator begin() noexcept { return first_; }
  [[nodiscard]] constexpr const_iterator begin() const noexcept { return first_; }
  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }

  [[nodiscard]] constexpr iterator end() noexcept { return last_; }
  [[nodiscard]] constexpr const_iterator end() const noexcept { return last_; }
  [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }

  [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
  [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
  [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }

  [[nodiscard]] constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
  [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
  [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return rend(); }

  // ======================================================================
  // Capacity
  // ======================================================================
  [[nodiscard]] constexpr bool empty() const noexcept { return first_ == last_; }
  [[nodiscard]] constexpr size_type size() const noexcept {
    if (first_ == nullptr) return 0;
    return static_cast<size_type>(last_ - first_);
  }
  [[nodiscard]] constexpr size_type max_size() const noexcept { return allocator_traits::max_size(allocator_); }
  [[nodiscard]] constexpr size_type capacity() const noexcept {
    if (first_ == nullptr) return 0;
    return static_cast<size_type>(capacity_last_ - first_);
  }
  constexpr void reserve(size_type new_capacity);
  constexpr void shrink_to_fit();
  constexpr void resize(size_type count);
  constexpr void resize(size_type count, const T& value);

  // Non-standard extension: C++23 added resize_and_overwrite to
  // basic_string only (P1072); no vector equivalent has been adopted.
  // Modeled after the string interface.
  template <class Operation>
  constexpr void resize_and_overwrite(size_type count, Operation op);

  // ======================================================================
  // Modifiers
  // ======================================================================
  constexpr void clear() noexcept { destroy_elements(); }

  template <class... Args>
  constexpr reference emplace_back(Args&&... args) {
    if (last_ == capacity_last_) {
      return *reallocate_and_emplace_back(std::forward<Args>(args)...);
    }
    construct_at_end(std::forward<Args>(args)...);
    return back();
  }

  constexpr void push_back(const T& value) { emplace_back(value); }
  constexpr void push_back(T&& value) { emplace_back(std::move(value)); }

  // C++23 ranges append.
  template <concepts::container_compatible_range<T> R>
  constexpr void append_range(R&& range) {
    if constexpr (std::ranges::forward_range<R> || std::ranges::sized_range<R>) {
      append_counted_range(std::ranges::begin(range), static_cast<size_type>(std::ranges::distance(range)));
    } else {
      append_uncounted_range(std::ranges::begin(range), std::ranges::end(range));
    }
  }
  constexpr void pop_back() noexcept {
    CHAI_HARDENED(!empty(), "chaistl::vector::pop_back: empty container");
    --last_;
    allocator_traits::destroy(allocator_, std::to_address(last_));
  }

  template <class... Args>
  constexpr iterator emplace(const_iterator position, Args&&... args);
  constexpr iterator insert(const_iterator position, const T& value);
  constexpr iterator insert(const_iterator position, T&& value);
  constexpr iterator insert(const_iterator position, size_type count, const T& value);

  // This overload participates in overload resolution only if InputIt
  // satisfies LegacyInputIterator (cppreference).
  // Type requirements:
  // - T must be EmplaceConstructible from *first.
  // - Since C++17: T must meet Swappable, MoveAssignable, MoveConstructible
  //   and MoveInsertable.
  template <std::input_iterator InputIt>
  constexpr iterator insert(const_iterator position, InputIt first, InputIt last);

  // C++23 ranges insert.
  template <concepts::container_compatible_range<T> R>
  constexpr iterator insert_range(const_iterator position, R&& range);

  constexpr iterator insert(const_iterator position, std::initializer_list<T> init);
  constexpr iterator erase(const_iterator position);
  constexpr iterator erase(const_iterator first, const_iterator last);
  constexpr void swap(vector& other) noexcept(meta::is_nothrow_container_swappable_v<allocator_type>);

  [[nodiscard]] constexpr allocator_type get_allocator() const noexcept;

  // ======================================================================
  // Private implementation
  // ======================================================================
 private:
  // ------------------------------------------------------------------------
  // Data members
  // ------------------------------------------------------------------------
  pointer first_{};
  pointer last_{};
  pointer capacity_last_{};
  [[no_unique_address]] allocator_type allocator_{};

  // ------------------------------------------------------------------------
  // Internal helpers (ordered by lifecycle: allocate → construct → destroy)
  // ------------------------------------------------------------------------
  [[nodiscard]] constexpr size_type recommended_capacity(size_type required) const;
  constexpr void check_capacity(size_type count) const;
  constexpr void reallocate(size_type new_capacity);

  template <class... Args>
  constexpr pointer reallocate_and_emplace_back(Args&&... args);
  template <class... Args>
  constexpr iterator reallocate_and_emplace_at(size_type offset, Args&&... args);

  // Element lifetime.
  template <class... Args>
  constexpr void construct_at_end(Args&&... args);
  constexpr void destroy_elements() noexcept;
  constexpr void destroy_at_end(pointer new_last) noexcept;

  // In-place insertion.
  template <class... Args>
  constexpr iterator emplace_in_spare_capacity(size_type offset, Args&&... args);
  template <std::input_iterator InputIt>
  constexpr void append_counted_range(InputIt first, size_type count);
  template <std::input_iterator InputIt, class Sentinel>
  constexpr void append_uncounted_range(InputIt first, Sentinel last);
  template <std::input_iterator InputIt>
  constexpr pointer move_assign_if_noexcept(pointer target, InputIt first, InputIt last);
  constexpr iterator insert_fill_with_reallocation(size_type offset, size_type count, const T& value);
  constexpr iterator insert_fill_with_spare_capacity(size_type offset, size_type count, const T& value);
  constexpr iterator insert_range_buffer(size_type offset, vector& inserted);
  constexpr iterator insert_range_buffer_with_reallocation(size_type offset, vector& inserted);
  constexpr iterator insert_range_buffer_with_spare_capacity(size_type offset, vector& inserted);

  // Storage ownership.
  constexpr void destroy_and_deallocate_storage() noexcept;
  constexpr void take_storage_from(vector& other) noexcept;

  // Position helpers.
  [[nodiscard]] constexpr size_type offset_of(const_iterator position) const noexcept;
};

// ======================================================================
// Vector Constructors / Destructor
// ======================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr vector<T, Allocator>::vector(const allocator_type& alloc) noexcept : allocator_(alloc) {}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr vector<T, Allocator>::vector(size_type count, const allocator_type& alloc) : allocator_(alloc) {
  check_capacity(count);
  if (count == 0) return;

  detail::uninitialized_storage_builder<T, allocator_type> storage(allocator_, count);

  pointer new_first = storage.data();
  pointer new_last = storage.uninitialized_default_construct_n(new_first, count);

  first_ = storage.release();
  last_ = new_last;
  capacity_last_ = first_ + count;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr vector<T, Allocator>::vector(size_type count, const T& value, const allocator_type& alloc)
    : allocator_(alloc) {
  check_capacity(count);
  if (count == 0) return;

  detail::uninitialized_storage_builder<T, allocator_type> storage(allocator_, count);

  pointer new_first = storage.data();
  pointer new_last = storage.uninitialized_fill_n(new_first, count, value);

  first_ = storage.release();
  last_ = new_last;
  capacity_last_ = first_ + count;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <std::input_iterator InputIt>
constexpr vector<T, Allocator>::vector(InputIt first, InputIt last, const allocator_type& alloc) : allocator_(alloc) {
  auto guard = detail::make_exception_guard([&] {
    destroy_and_deallocate_storage();
  });
  if constexpr (std::forward_iterator<InputIt>) {
    const auto count = static_cast<size_type>(std::distance(first, last));
    if (count == 0) {
      guard.complete();
      return;
    }
    detail::uninitialized_storage_builder<T, allocator_type> storage(allocator_, count);
    pointer new_first = storage.data();
    pointer new_last = storage.uninitialized_copy(first, last, new_first);
    first_ = storage.release();
    last_ = new_last;
    capacity_last_ = first_ + count;
  } else {
    for (; first != last; ++first) {
      emplace_back(*first);
    }
  }
  guard.complete();
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr vector<T, Allocator>::vector(std::initializer_list<T> init, const allocator_type& alloc)
    : vector(init.begin(), init.end(), alloc) {}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr vector<T, Allocator>::vector(const vector& other)
    : allocator_(allocator_traits::select_on_container_copy_construction(other.allocator_)) {
  if (other.empty()) return;

  detail::uninitialized_storage_builder<T, allocator_type> storage(allocator_, other.size());

  pointer new_first = storage.data();
  pointer new_last = storage.uninitialized_copy(other.first_, other.last_, new_first);

  first_ = storage.release();
  last_ = new_last;
  capacity_last_ = first_ + other.size();
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr vector<T, Allocator>::vector(vector&& other) noexcept
    : first_(std::exchange(other.first_, nullptr)),
      last_(std::exchange(other.last_, nullptr)),
      capacity_last_(std::exchange(other.capacity_last_, nullptr)),
      allocator_(std::move(other.allocator_)) {}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr vector<T, Allocator>::vector(const vector& other, const std::type_identity_t<Allocator>& alloc)
    : allocator_(alloc) {
  if (other.empty()) return;

  detail::uninitialized_storage_builder<T, allocator_type> storage(allocator_, other.size());

  pointer new_first = storage.data();
  pointer new_last = storage.uninitialized_copy(other.first_, other.last_, new_first);

  first_ = storage.release();
  last_ = new_last;
  capacity_last_ = first_ + other.size();
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr vector<T, Allocator>::vector(vector&& other, const std::type_identity_t<Allocator>& alloc)
    : allocator_(alloc) {
  if (allocator_ == other.allocator_) {
    first_ = std::exchange(other.first_, nullptr);
    last_ = std::exchange(other.last_, nullptr);
    capacity_last_ = std::exchange(other.capacity_last_, nullptr);
  } else {
    if (other.empty()) return;

    detail::uninitialized_storage_builder<T, allocator_type> storage(allocator_, other.size());

    pointer new_first = storage.data();
    pointer new_last = storage.uninitialized_move_if_noexcept(other.first_, other.last_, new_first);

    first_ = storage.release();
    last_ = new_last;
    capacity_last_ = first_ + other.size();
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <concepts::container_compatible_range<T> R>
constexpr vector<T, Allocator>::vector(std::from_range_t, R&& range, const allocator_type& alloc) : allocator_(alloc) {
  auto guard = detail::make_exception_guard([&] {
    destroy_and_deallocate_storage();
  });
  append_range(std::forward<R>(range));
  guard.complete();
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr vector<T, Allocator>::~vector() {
  destroy_and_deallocate_storage();
}

// ======================================================================
// Vector Assignment / Assign
// ======================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr vector<T, Allocator>& vector<T, Allocator>::operator=(const vector& other) {
  if (this == &other) return *this;

  if constexpr (meta::propagate_on_container_copy_assignment_v<allocator_type>) {
    vector copy(other, other.allocator_);
    destroy_and_deallocate_storage();
    allocator_ = other.allocator_;
    take_storage_from(copy);
  } else {
    vector copy(other, allocator_);
    destroy_and_deallocate_storage();
    take_storage_from(copy);
  }
  return *this;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr vector<T, Allocator>& vector<T, Allocator>::operator=(vector&& other) noexcept(
    meta::is_nothrow_container_move_assignable_v<allocator_type, T>) {
  if (this == &other) return *this;

  if constexpr (meta::propagate_on_container_move_assignment_v<allocator_type>) {
    destroy_and_deallocate_storage();
    allocator_ = std::move(other.allocator_);
    take_storage_from(other);
  } else if (allocator_ == other.allocator_) {
    destroy_and_deallocate_storage();
    take_storage_from(other);
  } else {
    vector moved(std::move(other), allocator_);
    destroy_and_deallocate_storage();
    take_storage_from(moved);
  }
  return *this;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr vector<T, Allocator>& vector<T, Allocator>::operator=(std::initializer_list<T> init) {
  vector copy(init, allocator_);
  swap(copy);
  return *this;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <std::input_iterator InputIt>
constexpr void vector<T, Allocator>::assign(InputIt first, InputIt last) {
  vector copy(first, last, allocator_);
  swap(copy);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <concepts::container_compatible_range<T> R>
constexpr void vector<T, Allocator>::assign_range(R&& range) {
  vector copy(std::from_range, std::forward<R>(range), allocator_);
  swap(copy);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void vector<T, Allocator>::assign(size_type count, const T& value) {
  vector copy(count, value, allocator_);
  swap(copy);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void vector<T, Allocator>::assign(std::initializer_list<T> init) {
  assign(init.begin(), init.end());
}

// ======================================================================
// ======================================================================
// Vector Capacity Handling
// ======================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void vector<T, Allocator>::reserve(size_type new_capacity) {
  if (new_capacity <= capacity()) return;

  check_capacity(new_capacity);
  reallocate(new_capacity);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void vector<T, Allocator>::shrink_to_fit() {
  if (size() == capacity()) return;
  if (empty()) {
    destroy_and_deallocate_storage();
    return;
  }
  reallocate(size());
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void vector<T, Allocator>::resize(size_type count) {
  if (count < size()) {
    destroy_at_end(first_ + count);
    return;
  }

  reserve(count);
  last_ = detail::allocator_uninitialized_default_construct_n(allocator_, last_, count - size());
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void vector<T, Allocator>::resize(size_type count, const T& value) {
  if (count < size()) {
    destroy_at_end(first_ + count);
    return;
  }

  reserve(count);
  last_ = detail::allocator_uninitialized_fill_n(allocator_, last_, count - size(), value);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class Operation>
constexpr void vector<T, Allocator>::resize_and_overwrite(size_type count, Operation op) {
  const size_type old_size = size();
  if (count <= old_size) {
    destroy_at_end(first_ + count);
    return;
  }

  reserve(count);
  // Mirrors basic_string::resize_and_overwrite ([string.capacity]): if op
  // throws, behavior is undefined. The caller takes responsibility for
  // exception safety of the operation.
  op(std::to_address(first_), old_size, count);
  last_ = first_ + count;
}

// ======================================================================
// Vector Modifier Logic
// ======================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class... Args>
constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::emplace(const_iterator position,
                                                                                Args&&... args) {
  const size_type offset = offset_of(position);
  if (last_ == capacity_last_) {
    return reallocate_and_emplace_at(offset, std::forward<Args>(args)...);
  }
  return emplace_in_spare_capacity(offset, std::forward<Args>(args)...);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::insert(const_iterator position,
                                                                               const T& value) {
  return emplace(position, value);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::insert(const_iterator position, T&& value) {
  return emplace(position, std::move(value));
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::insert(const_iterator position,
                                                                               size_type count,
                                                                               const T& value) {
  const size_type offset = offset_of(position);
  if (count == 0) {
    return begin() + static_cast<difference_type>(offset);
  }

  check_capacity(size() + count);
  if (size() + count > capacity()) {
    return insert_fill_with_reallocation(offset, count, value);
  }
  return insert_fill_with_spare_capacity(offset, count, value);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <std::input_iterator InputIt>
constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::insert(const_iterator position,
                                                                               InputIt first,
                                                                               InputIt last) {
  vector inserted(first, last, allocator_);
  return insert_range(position, inserted);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <concepts::container_compatible_range<T> R>
constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::insert_range(const_iterator position,
                                                                                     R&& range) {
  const size_type offset = offset_of(position);
  if (offset == size()) {
    append_range(std::forward<R>(range));
    return begin() + static_cast<difference_type>(offset);
  }

  vector inserted(std::from_range, std::forward<R>(range), allocator_);
  return insert_range_buffer(offset, inserted);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::insert(const_iterator position,
                                                                               std::initializer_list<T> init) {
  return insert(position, init.begin(), init.end());
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::erase(const_iterator position) {
  return erase(position, position + 1);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::erase(const_iterator first,
                                                                              const_iterator last) {
  const size_type first_offset = offset_of(first);
  const size_type last_offset = offset_of(last);

  if (first_offset == last_offset) {
    return begin() + static_cast<difference_type>(first_offset);
  }

  pointer erase_first = first_ + first_offset;
  pointer erase_last = first_ + last_offset;

  // P4102R0: Container insertion and erasure should be allowed to relocate.
  // Current standard wording requires that erase() move-assign the suffix
  // over the erased range. For types that are trivially relocatable, this
  // is equivalent to memmove (which handles overlapping ranges). When P4102
  // is adopted, implementations may use relocation instead of assignment,
  // providing order-of-magnitude speedups for large erasures of POD types.
  //
  // Potential future optimization:
  //   if constexpr (is_trivially_relocatable_v<T>) {
  //     std::memmove(std::to_address(erase_first),
  //                  std::to_address(erase_last),
  //                  (last_ - erase_last) * sizeof(T));
  //     new_last = erase_first + (last_ - erase_last);
  //   } else {
  //     new_last = std::move(erase_last, last_, erase_first);
  //   }
  pointer new_last = std::move(erase_last, last_, erase_first);
  destroy_at_end(new_last);
  return begin() + static_cast<difference_type>(first_offset);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void vector<T, Allocator>::swap(vector& other) noexcept(
    meta::is_nothrow_container_swappable_v<allocator_type>) {
  using std::swap;
  swap(first_, other.first_);
  swap(last_, other.last_);
  swap(capacity_last_, other.capacity_last_);
  if constexpr (meta::propagate_on_container_swap_v<allocator_type>) {
    swap(allocator_, other.allocator_);
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename vector<T, Allocator>::allocator_type vector<T, Allocator>::get_allocator() const noexcept {
  return allocator_;
}

// ======================================================================
// Vector Internal Storage Helpers
// ======================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename vector<T, Allocator>::size_type vector<T, Allocator>::recommended_capacity(
    size_type required) const {
  if (required <= capacity()) return capacity();

  check_capacity(required);
  const size_type current = capacity();
  if (current > max_size() / 2) return max_size();
  return std::max(current == 0 ? size_type{1} : current * 2, required);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void vector<T, Allocator>::check_capacity(size_type count) const {
  if (count > max_size()) {
    throw std::length_error("chaistl::vector capacity exceeds max_size()");
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void vector<T, Allocator>::reallocate(size_type new_capacity) {
  detail::uninitialized_storage_builder<T, allocator_type> storage(allocator_, new_capacity);

  pointer new_first = storage.data();

  // P2786R9 / P4102R0: Trivial relocation optimization.
  // For types that are trivially relocatable (e.g. all scalar types,
  // std::vector, std::string on most implementations), the standard
  // currently requires move-assignment followed by destruction. This
  // is semantically equivalent to memcpy for relocatable types, but
  // the standard does not yet permit it.
  //
  // When P4102 is adopted, we can replace the loop below with:
  //   if constexpr (is_trivially_relocatable_v<T>) {
  //     std::memcpy(std::to_address(new_first),
  //                 std::to_address(first_),
  //                 size() * sizeof(T));
  //     new_last = new_first + size();
  //   } else {
  //     new_last = storage.uninitialized_move_if_noexcept(...);
  //   }
  //
  // libc++ and libstdc++ are already experimenting with this. The
  // trait is expected to be std::is_trivially_relocatable (or similar)
  // in a future standard version.
  pointer new_last = storage.uninitialized_move_if_noexcept(first_, last_, new_first);

  destroy_and_deallocate_storage();
  first_ = storage.release();
  last_ = new_last;
  capacity_last_ = first_ + new_capacity;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class... Args>
constexpr typename vector<T, Allocator>::pointer vector<T, Allocator>::reallocate_and_emplace_back(Args&&... args) {
  const size_type old_size = size();
  const size_type new_capacity = recommended_capacity(old_size + 1);

  detail::uninitialized_storage_builder<T, allocator_type> storage(allocator_, new_capacity);

  pointer new_first = storage.data();

  // Construct the new element first (before moving old elements),
  // so that args referencing existing elements are not invalidated.
  // If construction throws, storage builder's transaction rolls back
  // the new element and deallocates the new storage; old storage is untouched.
  pointer new_tail = new_first + old_size;
  storage.construct_at(new_tail, std::forward<Args>(args)...);

  // P2786R9 / P4102R0: See reallocate() for trivial relocation notes.
  // When the trait becomes available, the move below can be replaced
  // with std::memcpy for trivially relocatable types, giving order-of-
  // magnitude performance improvements for large vectors of PODs.
  storage.uninitialized_move_if_noexcept(first_, last_, new_first);

  destroy_and_deallocate_storage();
  first_ = storage.release();
  last_ = new_tail + 1;
  capacity_last_ = first_ + new_capacity;
  return first_ + old_size;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class... Args>
constexpr void vector<T, Allocator>::construct_at_end(Args&&... args) {
  allocator_traits::construct(allocator_, std::to_address(last_), std::forward<Args>(args)...);
  // Intentionally: last_ is incremented after construction succeeds.
  // If construct() throws, the container remains in a valid state
  // (the slot at last_ is still uninitialized).
  ++last_;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <std::input_iterator InputIt>
constexpr void vector<T, Allocator>::append_counted_range(InputIt first, size_type count) {
  if (count == 0) {
    return;
  }

  const size_type old_size = size();
  if (count > max_size() - old_size) {
    throw std::length_error("chaistl::vector capacity exceeds max_size()");
  }

  if (count > capacity() - old_size) {
    const size_type new_capacity = recommended_capacity(old_size + count);
    detail::uninitialized_storage_builder<T, allocator_type> storage(allocator_, new_capacity);
    pointer new_first = storage.data();

    storage.uninitialized_copy_n(first, count, new_first + old_size);
    storage.uninitialized_move_if_noexcept(first_, last_, new_first);

    destroy_and_deallocate_storage();
    first_ = storage.release();
    last_ = first_ + old_size + count;
    capacity_last_ = first_ + new_capacity;
    return;
  }

  pointer constructed_last = last_;
  detail::constructed_range_guard<allocator_type, pointer> guard(allocator_, last_, constructed_last);
  constructed_last = detail::allocator_uninitialized_copy_n(allocator_, first, count, constructed_last);
  guard.complete();
  last_ = constructed_last;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <std::input_iterator InputIt, class Sentinel>
constexpr void vector<T, Allocator>::append_uncounted_range(InputIt first, Sentinel last) {
  for (; first != last; ++first) {
    emplace_back(*first);
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <std::input_iterator InputIt>
constexpr typename vector<T, Allocator>::pointer vector<T, Allocator>::move_assign_if_noexcept(pointer target,
                                                                                               InputIt first,
                                                                                               InputIt last) {
  for (; first != last; ++first, ++target) {
    *target = std::move_if_noexcept(*first);
  }
  return target;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class... Args>
constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::reallocate_and_emplace_at(size_type offset,
                                                                                                  Args&&... args) {
  const size_type old_size = size();
  const size_type new_capacity = recommended_capacity(size() + 1);

  detail::uninitialized_storage_builder<T, allocator_type> storage(allocator_, new_capacity);

  pointer new_first = storage.data();
  pointer inserted = new_first + offset;

  storage.construct_at(inserted, std::forward<Args>(args)...);
  storage.uninitialized_move_if_noexcept(first_, first_ + offset, new_first);
  storage.uninitialized_move_if_noexcept(first_ + offset, last_, inserted + 1);

  destroy_and_deallocate_storage();
  first_ = storage.release();
  last_ = first_ + old_size + 1;
  capacity_last_ = first_ + new_capacity;
  return begin() + static_cast<difference_type>(offset);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class... Args>
constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::emplace_in_spare_capacity(size_type offset,
                                                                                                  Args&&... args) {
  pointer position = first_ + offset;
  if (position == last_) {
    construct_at_end(std::forward<Args>(args)...);
    return begin() + static_cast<difference_type>(offset);
  }

  // Strategy: make room by shifting the suffix one step to the right.
  //   1. Construct a copy of the last element at the end.
  //   2. move_backward shifts [position, last-1) → [position+1, last).
  //   3. Assign the new value at position.
  T value(std::forward<Args>(args)...);
  allocator_traits::construct(allocator_, std::to_address(last_), std::move_if_noexcept(*(last_ - 1)));
  ++last_;
  std::move_backward(position, last_ - 2, last_ - 1);
  *position = std::move(value);
  return begin() + static_cast<difference_type>(offset);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::insert_fill_with_reallocation(size_type offset,
                                                                                                      size_type count,
                                                                                                      const T& value) {
  const size_type old_size = size();
  const size_type new_capacity = recommended_capacity(old_size + count);

  detail::uninitialized_storage_builder<T, allocator_type> storage(allocator_, new_capacity);

  pointer new_first = storage.data();

  storage.uninitialized_copy(first_, first_ + offset, new_first);
  storage.uninitialized_fill_n(new_first + offset, count, value);
  storage.uninitialized_copy(first_ + offset, first_ + old_size, new_first + offset + count);

  destroy_and_deallocate_storage();
  first_ = storage.release();
  last_ = first_ + old_size + count;
  capacity_last_ = first_ + new_capacity;
  return begin() + static_cast<difference_type>(offset);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::insert_fill_with_spare_capacity(
    size_type offset, size_type count, const T& value) {
  T inserted_value(value);
  pointer position = first_ + offset;
  pointer old_last = last_;
  const size_type suffix_count = static_cast<size_type>(old_last - position);

  if (suffix_count == 0) {
    // No suffix to shift: just construct new elements at the end.
    last_ = detail::allocator_uninitialized_fill_n(allocator_, last_, count, inserted_value);
    return begin() + static_cast<difference_type>(offset);
  }

  if (count < suffix_count) {
    // Short insert: only part of the suffix needs to move.
    //   1. Move [old_last - count, old_last) to uninitialized end.
    //   2. move_backward shifts [position, old_last - count) right by count.
    //   3. Fill [position, position + count) with the new value.
    pointer constructed_last = old_last;
    detail::constructed_range_guard<allocator_type, pointer> guard(allocator_, old_last, constructed_last);
    constructed_last =
        detail::allocator_uninitialized_move_if_noexcept(allocator_, old_last - count, old_last, constructed_last);
    guard.complete();
    last_ = constructed_last;
    std::move_backward(position, old_last - count, old_last);
    std::fill_n(position, count, inserted_value);
    return begin() + static_cast<difference_type>(offset);
  }

  // Long insert: the suffix becomes entirely part of the new range.
  //   1. Construct (count - suffix_count) new values at the end.
  //   2. Move [position, old_last) to uninitialized area.
  //   3. Fill [position, old_last) with the new value.
  pointer constructed_last = old_last;
  detail::constructed_range_guard<allocator_type, pointer> guard(allocator_, old_last, constructed_last);
  constructed_last =
      detail::allocator_uninitialized_fill_n(allocator_, constructed_last, count - suffix_count, inserted_value);
  constructed_last = detail::allocator_uninitialized_move_if_noexcept(allocator_, position, old_last, constructed_last);
  guard.complete();
  last_ = constructed_last;
  std::fill(position, old_last, inserted_value);
  return begin() + static_cast<difference_type>(offset);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::insert_range_buffer(size_type offset,
                                                                                            vector& inserted) {
  if (inserted.empty()) {
    return begin() + static_cast<difference_type>(offset);
  }

  check_capacity(size() + inserted.size());
  if (size() + inserted.size() > capacity()) {
    return insert_range_buffer_with_reallocation(offset, inserted);
  }
  return insert_range_buffer_with_spare_capacity(offset, inserted);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::insert_range_buffer_with_reallocation(
    size_type offset, vector& inserted) {
  const size_type old_size = size();
  const size_type insert_count = inserted.size();
  const size_type new_capacity = recommended_capacity(old_size + insert_count);

  detail::uninitialized_storage_builder<T, allocator_type> storage(allocator_, new_capacity);

  pointer new_first = storage.data();

  storage.uninitialized_move_if_noexcept(first_, first_ + offset, new_first);
  storage.uninitialized_move_if_noexcept(inserted.begin(), inserted.end(), new_first + offset);
  storage.uninitialized_move_if_noexcept(first_ + offset, first_ + old_size, new_first + offset + insert_count);

  destroy_and_deallocate_storage();
  first_ = storage.release();
  last_ = first_ + old_size + insert_count;
  capacity_last_ = first_ + new_capacity;
  return begin() + static_cast<difference_type>(offset);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename vector<T, Allocator>::iterator vector<T, Allocator>::insert_range_buffer_with_spare_capacity(
    size_type offset, vector& inserted) {
  pointer position = first_ + offset;
  pointer old_last = last_;
  const size_type insert_count = inserted.size();
  const size_type suffix_count = static_cast<size_type>(old_last - position);

  if (suffix_count == 0) {
    // No suffix: move-insert everything at the end.
    last_ = detail::allocator_uninitialized_move_if_noexcept(allocator_, inserted.begin(), inserted.end(), last_);
    return begin() + static_cast<difference_type>(offset);
  }

  if (insert_count < suffix_count) {
    // Short range: same logic as insert_fill_with_spare_capacity,
    // but assign from the inserted range instead of filling.
    pointer constructed_last = old_last;
    detail::constructed_range_guard<allocator_type, pointer> guard(allocator_, old_last, constructed_last);
    constructed_last = detail::allocator_uninitialized_move_if_noexcept(
        allocator_, old_last - insert_count, old_last, constructed_last);
    guard.complete();
    last_ = constructed_last;
    std::move_backward(position, old_last - insert_count, old_last);
    move_assign_if_noexcept(position, inserted.begin(), inserted.end());
    return begin() + static_cast<difference_type>(offset);
  }

  // Long range: suffix is absorbed into the inserted range.
  //   1. Move the tail of inserted (beyond what overwrites the suffix) to end.
  //   2. Move [position, old_last) to uninitialized area.
  //   3. Assign the head of inserted over [position, old_last).
  pointer constructed_last = old_last;
  auto inserted_tail = inserted.begin() + static_cast<difference_type>(suffix_count);
  detail::constructed_range_guard<allocator_type, pointer> guard(allocator_, old_last, constructed_last);
  constructed_last =
      detail::allocator_uninitialized_move_if_noexcept(allocator_, inserted_tail, inserted.end(), constructed_last);
  constructed_last = detail::allocator_uninitialized_move_if_noexcept(allocator_, position, old_last, constructed_last);
  guard.complete();
  last_ = constructed_last;
  move_assign_if_noexcept(position, inserted.begin(), inserted_tail);
  return begin() + static_cast<difference_type>(offset);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void vector<T, Allocator>::destroy_elements() noexcept {
  detail::allocator_destroy_reverse(allocator_, first_, last_);
  last_ = first_;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void vector<T, Allocator>::destroy_at_end(pointer new_last) noexcept {
  detail::allocator_destroy_reverse(allocator_, new_last, last_);
  last_ = new_last;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void vector<T, Allocator>::destroy_and_deallocate_storage() noexcept {
  const size_type old_capacity = capacity();
  destroy_elements();
  if (first_ != nullptr) {
    allocator_traits::deallocate(allocator_, first_, old_capacity);
  }
  first_ = nullptr;
  last_ = nullptr;
  capacity_last_ = nullptr;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void vector<T, Allocator>::take_storage_from(vector& other) noexcept {
  first_ = std::exchange(other.first_, nullptr);
  last_ = std::exchange(other.last_, nullptr);
  capacity_last_ = std::exchange(other.capacity_last_, nullptr);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename vector<T, Allocator>::size_type vector<T, Allocator>::offset_of(
    const_iterator position) const noexcept {
  return static_cast<size_type>(position - cbegin());
}

// ======================================================================
// Vector Non-member Functions
// ======================================================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr bool operator==(const vector<T, Allocator>& lhs, const vector<T, Allocator>& rhs) {
  return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
[[nodiscard]] constexpr auto operator<=>(const vector<T, Allocator>& lhs, const vector<T, Allocator>& rhs) {
  return chaistl::default_three_way_compare(lhs, rhs);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void swap(vector<T, Allocator>& lhs,
                    vector<T, Allocator>& rhs) noexcept(meta::is_nothrow_container_swappable_v<Allocator>) {
  lhs.swap(rhs);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator, class U = T>
constexpr typename vector<T, Allocator>::size_type erase(vector<T, Allocator>& container, const U& value) {
  auto first = std::remove(container.begin(), container.end(), value);
  auto removed = static_cast<typename vector<T, Allocator>::size_type>(std::distance(first, container.end()));
  container.erase(first, container.end());
  return removed;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator, class Predicate>
constexpr typename vector<T, Allocator>::size_type erase_if(vector<T, Allocator>& container, Predicate predicate) {
  auto first = std::remove_if(container.begin(), container.end(), predicate);
  auto removed = static_cast<typename vector<T, Allocator>::size_type>(std::distance(first, container.end()));
  container.erase(first, container.end());
  return removed;
}

template <std::input_iterator InputIt, class Allocator = allocator<std::iter_value_t<InputIt>>>
vector(InputIt, InputIt, Allocator = Allocator()) -> vector<std::iter_value_t<InputIt>, Allocator>;

template <class R, class Allocator = allocator<std::ranges::range_value_t<R>>>
  requires concepts::container_compatible_range<R, std::ranges::range_value_t<R>>
vector(std::from_range_t, R&&, Allocator = Allocator()) -> vector<std::ranges::range_value_t<R>, Allocator>;

namespace pmr {

template <concepts::container_element T>
using vector = chaistl::vector<T, chaistl::pmr::polymorphic_allocator<T>>;

}  // namespace pmr

}  // namespace chaistl
