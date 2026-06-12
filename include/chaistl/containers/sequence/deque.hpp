// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// deque - Double-ended sequence container over segmented storage
// ============================================================================
//
// Architecture:
//   - Stores elements in fixed-size blocks addressed by a map of block
//     pointers. Growing at either end usually allocates or exposes another
//     block instead of relocating all existing elements.
//   - The iterator carries both the current element pointer and the map slot,
//     so random-access traversal can jump across block boundaries.
//   - This file keeps the map/block arithmetic explicit for teaching; it does
//     not hide the segmented layout behind a vector-like facade.
//
// Standardization archaeology:
//   - deque was standardized in C++98 beside vector and list to cover the
//     "grow cheaply at both ends" point in the sequence-container design
//     space.
//   - Its segmented storage explains the enduring semantic split from vector:
//     deque has random-access iterators but not contiguous iterators, so it is
//     fast for indexed access while still unsuitable for C-array interop.
//   - The standard leaves block size and map layout to implementations. Real
//     libraries therefore differ substantially here while preserving the same
//     public complexity and invalidation rules.
//   - C++23 added range construction/insertion to the container family; this
//     implementation mirrors that direction where the local concepts allow it.
//
// References:
//   - C++ Draft: https://eel.is/c++draft/deque
//   - cppreference: https://en.cppreference.com/w/cpp/container/deque

#include <chaistl/containers/sequence/detail/common.hpp>
#include <chaistl/iterator/interface.hpp>
#include <chaistl/memory/detail/lifetime/allocator_uninitialized.hpp>
#include <chaistl/memory/detail/lifetime/exception_guard.hpp>
#include <chaistl/memory/detail/storage/raw_storage_buffer.hpp>
#include <chaistl/utility/hardening.hpp>

#include <compare>
#include <memory_resource>

namespace chaistl {

/**
 * @brief Double-ended sequence container backed by segmented storage.
 *
 * @ingroup containers_sequence
 *
 * Deque stores elements in fixed-size blocks indexed by a central map.
 * This allows efficient insertion and removal at both ends.
 *
 * Storage model (visible for learning):
 * - `map_`           : array of pointers to blocks
 * - `map_capacity_`  : total slots in map_
 * - `first_block_`   : index into map_ of the first active block
 * - `first_offset_`  : index within that block of the first element
 * - `size_`          : number of live elements
 *
 * Logical index 0 is stored at `map_[first_block_] + first_offset_`;
 * later elements continue across fixed-size blocks.
 *
 * References:
 *
 * - C++ Draft Standard: https://eel.is/c++draft/deque
 * - cppreference: https://en.cppreference.com/w/cpp/container/deque
 */
template <concepts::container_element T, concepts::allocator_for<T> Allocator = allocator<T>>
class deque {
 public:
  // ========================================================================
  // Member types
  // ========================================================================
  using value_type = T;
  using allocator_type = Allocator;
  using allocator_traits = std::allocator_traits<allocator_type>;
  using size_type = typename allocator_traits::size_type;
  using difference_type = typename allocator_traits::difference_type;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = typename allocator_traits::pointer;
  using const_pointer = typename allocator_traits::const_pointer;

  // ========================================================================
  // Iterator implementation
  // ========================================================================

  /**
   * @brief Random-access iterator for deque's segmented storage.
   *
   * Stores a pointer into the current block (`current_`) and a pointer to the
   * map slot that owns the block (`map_slot_`). Increment/decrement cross
   * block boundaries automatically.
   */
  template <bool Const>
  class iterator_impl : public chaistl::iterator_interface {
    using allocator_traits = std::allocator_traits<Allocator>;
    using map_slot = typename allocator_traits::pointer*;
    using const_map_slot = const typename allocator_traits::pointer*;

   public:
    using iterator_category = std::random_access_iterator_tag;
    using iterator_concept = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = typename allocator_traits::difference_type;
    using reference = std::conditional_t<Const, const T&, T&>;
    using pointer =
        std::conditional_t<Const, typename allocator_traits::const_pointer, typename allocator_traits::pointer>;

    constexpr iterator_impl() noexcept = default;
    constexpr iterator_impl(const iterator_impl&) noexcept = default;
    constexpr iterator_impl& operator=(const iterator_impl&) noexcept = default;

    constexpr iterator_impl(const iterator_impl<false>& other) noexcept
      requires Const
        : map_slot_(other.map_slot_), current_(other.current_) {}

    [[nodiscard]] constexpr reference operator*() const noexcept { return *current_; }

    constexpr iterator_impl& operator++() noexcept {
      if (++current_ - block_begin() == block_size) {
        ++map_slot_;
        current_ = block_begin();
      }
      return *this;
    }

    constexpr iterator_impl& operator--() noexcept {
      if (current_ == block_begin()) {
        --map_slot_;
        current_ = block_begin() + block_size;
      }
      --current_;
      return *this;
    }

    constexpr iterator_impl& operator+=(difference_type offset) noexcept;

    constexpr iterator_impl& operator-=(difference_type offset) noexcept { return *this += -offset; }

    using iterator_interface::operator++;
    using iterator_interface::operator--;

    [[nodiscard]] constexpr iterator_impl operator+(difference_type offset) const noexcept {
      auto copy = *this;
      copy += offset;
      return copy;
    }

    [[nodiscard]] constexpr iterator_impl operator-(difference_type offset) const noexcept {
      auto copy = *this;
      copy -= offset;
      return copy;
    }

    [[nodiscard]] constexpr reference operator[](difference_type offset) const noexcept { return *(*this + offset); }

    template <bool OtherConst>
    [[nodiscard]] friend constexpr difference_type operator-(const iterator_impl& lhs,
                                                             const iterator_impl<OtherConst>& rhs) noexcept {
      if (lhs == rhs) return 0;
      return (lhs.map_slot_ - rhs.map_slot_) * block_size + (lhs.current_ - lhs.block_begin()) -
             (rhs.current_ - rhs.block_begin());
    }

    template <bool OtherConst>
    [[nodiscard]] friend constexpr bool operator==(const iterator_impl& lhs,
                                                   const iterator_impl<OtherConst>& rhs) noexcept {
      return lhs.current_ == rhs.current_;
    }

    template <bool OtherConst>
    [[nodiscard]] friend constexpr std::strong_ordering operator<=>(const iterator_impl& lhs,
                                                                    const iterator_impl<OtherConst>& rhs) noexcept {
      if (lhs.map_slot_ < rhs.map_slot_) return std::strong_ordering::less;
      if (rhs.map_slot_ < lhs.map_slot_) return std::strong_ordering::greater;
      if (lhs.current_ < rhs.current_) return std::strong_ordering::less;
      if (rhs.current_ < lhs.current_) return std::strong_ordering::greater;
      return std::strong_ordering::equal;
    }

    [[nodiscard]] friend constexpr iterator_impl operator+(difference_type offset,
                                                           const iterator_impl& iterator) noexcept {
      return iterator + offset;
    }

   private:
    template <bool>
    friend class iterator_impl;

    using stored_map_slot = std::conditional_t<Const, const_map_slot, map_slot>;

    static constexpr difference_type block_size = sizeof(T) < 256 ? 4096 / sizeof(T) : 16;

    [[nodiscard]] constexpr pointer block_begin() const noexcept { return *map_slot_; }

    stored_map_slot map_slot_{};
    pointer current_{};

   public:
    constexpr iterator_impl(stored_map_slot slot, pointer current) noexcept : map_slot_(slot), current_(current) {}
  };

  // ========================================================================
  // Iterator aliases
  // ========================================================================
  using iterator = iterator_impl<false>;
  using const_iterator = iterator_impl<true>;
  using reverse_iterator = chaistl::reverse_iterator<iterator>;
  using const_reverse_iterator = chaistl::reverse_iterator<const_iterator>;

  // ========================================================================
  // Private implementation
  // ========================================================================
 private:
  using map_allocator_type = typename allocator_traits::template rebind_alloc<pointer>;
  using map_allocator_traits = std::allocator_traits<map_allocator_type>;
  using map_pointer = pointer*;

  struct front_slot {
    size_type block{};
    size_type offset{};
    pointer value{};
  };

  static constexpr size_type block_size = sizeof(T) < 256 ? 4096 / sizeof(T) : 16;
  static constexpr size_type initial_map_capacity = 8;

  // ========================================================================
  // Storage model: map / block / offset
  // ========================================================================

  // Central map: an array of pointers to fixed-size blocks.
  map_pointer map_{};
  size_type map_capacity_{};

  // Position of the first element within the block array.
  size_type first_block_{};
  size_type first_offset_{};

  // Number of live elements.
  size_type size_{};

  [[no_unique_address]] allocator_type allocator_{};
  [[no_unique_address]] map_allocator_type map_allocator_{allocator_};

  // ========================================================================
  // Index calculation
  // ========================================================================

  /**
   * @brief Compute the physical address of the element at logical offset.
   *
   * The formula is the heart of the deque storage model:
   *   physical = first_offset_ + offset
   *   block    = first_block_ + physical / block_size
   *   address  = map_[block] + physical % block_size
   */
  [[nodiscard]] constexpr pointer element_pointer(size_type offset) noexcept {
    const size_type physical = first_offset_ + offset;
    return map_[first_block_ + physical / block_size] + physical % block_size;
  }

  [[nodiscard]] constexpr const_pointer element_pointer(size_type offset) const noexcept {
    const size_type physical = first_offset_ + offset;
    return map_[first_block_ + physical / block_size] + physical % block_size;
  }

  [[nodiscard]] constexpr iterator iterator_at(size_type offset) noexcept {
    if (map_ == nullptr) {
      return iterator{};
    }
    const size_type physical = first_offset_ + offset;
    const size_type block_index = first_block_ + physical / block_size;
    return iterator(map_ + block_index, map_[block_index] + physical % block_size);
  }

  [[nodiscard]] constexpr const_iterator iterator_at(size_type offset) const noexcept {
    if (map_ == nullptr) {
      return const_iterator{};
    }
    const size_type physical = first_offset_ + offset;
    const size_type block_index = first_block_ + physical / block_size;
    return const_iterator(map_ + block_index, map_[block_index] + physical % block_size);
  }

  // ========================================================================
  // Front / back slot preparation
  // ========================================================================

  [[nodiscard]] constexpr pointer prepare_back_slot() {
    ensure_back_slot();
    return element_pointer(size_);
  }

  [[nodiscard]] constexpr front_slot prepare_front_slot() {
    // The common push_front stays inside the current first block. That slot is
    // already allocated, so avoid the map bookkeeping that only block-boundary
    // pushes need.
    if (size_ != 0 && first_offset_ != 0) {
      const size_type offset = first_offset_ - 1;
      return {first_block_, offset, map_[first_block_] + offset};
    }

    ensure_map();

    if (size_ == 0) {
      allocate_block(first_block_);
      return {first_block_, first_offset_, map_[first_block_] + first_offset_};
    }

    front_slot slot{first_block_, first_offset_, nullptr};
    retreat_front_slot(slot);
    allocate_block(slot.block);
    slot.value = map_[slot.block] + slot.offset;
    return slot;
  }

  constexpr void commit_front_slot(front_slot slot) noexcept {
    first_block_ = slot.block;
    first_offset_ = slot.offset;
  }

  constexpr void retreat_front_slot(front_slot& slot) {
    if (slot.offset == 0) {
      if (slot.block == 0) {
        reallocate_map(1, 0);
        slot.block = first_block_;
      }
      --slot.block;
      slot.offset = block_size;
    }
    --slot.offset;
  }

  // ========================================================================
  // Map / block allocation policy
  // ========================================================================

  constexpr void ensure_map() {
    if (map_ != nullptr) {
      return;
    }

    detail::raw_storage_buffer<pointer, map_allocator_type> map_storage(map_allocator_, initial_map_capacity);
    map_pointer new_map = map_storage.data();
    for (size_type index = 0; index < initial_map_capacity; ++index) {
      new_map[index] = nullptr;
    }
    map_ = map_storage.release();
    map_capacity_ = initial_map_capacity;
    first_block_ = map_capacity_ / 2;
    first_offset_ = block_size / 2;
  }

  constexpr void ensure_back_slot() {
    const size_type physical = first_offset_ + size_;
    // The common push_back writes an interior slot of the current last block.
    // At a block boundary we also allocate the following block so end() can be
    // represented as a valid block pointer plus offset, matching this iterator
    // layout's invariant.
    if (size_ != 0 && physical % block_size != 0 && (physical + 1) % block_size != 0) {
      return;
    }
    ensure_map();
    const size_type block_index = ensure_physical_block(physical);
    allocate_block(block_index);
    if ((physical + 1) % block_size == 0) {
      allocate_block(ensure_physical_block(physical + 1));
    }
  }

  [[nodiscard]] constexpr size_type physical_block(size_type physical) const noexcept {
    return first_block_ + physical / block_size;
  }

  constexpr size_type ensure_physical_block(size_type physical) {
    size_type block_index = physical_block(physical);
    if (block_index >= map_capacity_) {
      reallocate_map(0, 1);
      block_index = physical_block(physical);
    }
    return block_index;
  }

  /**
   * @brief Grow the map and recenter existing block pointers.
   *
   * New capacity is at least double the old, plus any requested spare
   * room at the front or back.
   */
  constexpr void reallocate_map(size_type front_spare, size_type back_spare) {
    const size_type new_capacity =
        std::max(map_capacity_ * 2, map_capacity_ + front_spare + back_spare + initial_map_capacity);
    detail::raw_storage_buffer<pointer, map_allocator_type> map_storage(map_allocator_, new_capacity);
    map_pointer new_map = map_storage.data();
    for (size_type index = 0; index < new_capacity; ++index) {
      new_map[index] = nullptr;
    }

    const size_type shift = (new_capacity - map_capacity_) / 2 + front_spare;
    for (size_type index = 0; index < map_capacity_; ++index) {
      new_map[index + shift] = map_[index];
    }
    map_allocator_traits::deallocate(map_allocator_, map_, map_capacity_);
    map_ = map_storage.release();
    map_capacity_ = new_capacity;
    first_block_ += shift;
  }

  constexpr void allocate_block(size_type block_index) {
    if (map_[block_index] == nullptr) {
      detail::raw_storage_buffer<T, allocator_type> block_storage(allocator_, block_size);
      map_[block_index] = block_storage.release();
    }
  }

  // ========================================================================
  // Element construction / destruction
  // ========================================================================

  template <class... Args>
  constexpr void construct_at(pointer slot, Args&&... args) {
    allocator_traits::construct(allocator_, std::to_address(slot), std::forward<Args>(args)...);
  }

  template <class... Args>
  constexpr reference construct_front(Args&&... args) {
    front_slot slot = prepare_front_slot();
    construct_at(slot.value, std::forward<Args>(args)...);
    commit_front_slot(slot);
    ++size_;
    return front();
  }

  template <class... Args>
  constexpr reference construct_back(Args&&... args) {
    pointer slot = prepare_back_slot();
    construct_at(slot, std::forward<Args>(args)...);
    ++size_;
    return back();
  }

  // ========================================================================
  // Storage ownership helpers
  // ========================================================================

  constexpr void destroy_and_deallocate_storage() noexcept {
    clear();
    if (map_ == nullptr) {
      return;
    }
    for (size_type index = 0; index < map_capacity_; ++index) {
      if (map_[index] != nullptr) {
        allocator_traits::deallocate(allocator_, map_[index], block_size);
      }
    }
    map_allocator_traits::deallocate(map_allocator_, map_, map_capacity_);
    map_ = nullptr;
    map_capacity_ = 0;
    first_block_ = 0;
    first_offset_ = 0;
  }

  constexpr void take_storage_from(deque& other) noexcept {
    map_ = std::exchange(other.map_, nullptr);
    map_capacity_ = std::exchange(other.map_capacity_, 0);
    first_block_ = std::exchange(other.first_block_, 0);
    first_offset_ = std::exchange(other.first_offset_, 0);
    size_ = std::exchange(other.size_, 0);
  }

  constexpr void replace_storage_from(deque& other) noexcept {
    destroy_and_deallocate_storage();
    take_storage_from(other);
  }

  [[nodiscard]] constexpr bool storage_compatible_with(const deque& other) const {
    return allocator_ == other.allocator_;
  }

  constexpr void copy_allocator_from(const deque& other) {
    allocator_ = other.allocator_;
    map_allocator_ = map_allocator_type(allocator_);
  }

  constexpr void move_allocator_from(deque& other) {
    allocator_ = std::move(other.allocator_);
    map_allocator_ = map_allocator_type(allocator_);
  }

  constexpr void pop_back_until(size_type count) noexcept {
    while (size_ > count) {
      pop_back();
    }
  }

  constexpr void check_size(size_type new_size) const {
    if (new_size > max_size()) {
      throw std::length_error("chaistl::deque exceeds max_size");
    }
  }

  // ========================================================================
  // Rebuild-based insertion helpers
  // ========================================================================

  constexpr void insert_fill_with_rebuild(size_type offset, size_type count, const T& value);
  constexpr void insert_range_buffer(const_iterator position, deque& inserted);
  template <class InsertFn>
  constexpr void rebuild_with_insertion(size_type offset, InsertFn&& insert_fn);
  constexpr void append_default(size_type count);
  constexpr void append_fill(size_type count, const T& value);
  template <std::input_iterator InputIt>
  constexpr void append_iterator_range(InputIt first, InputIt last);

 public:
  // ========================================================================
  // Constructors and assignment
  // ========================================================================

  constexpr deque() noexcept(noexcept(allocator_type{})) = default;

  explicit constexpr deque(const allocator_type& alloc) noexcept : allocator_(alloc), map_allocator_(allocator_) {}

  explicit constexpr deque(size_type count, const allocator_type& alloc = allocator_type{})
      : allocator_(alloc), map_allocator_(allocator_) {
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_storage();
    });
    append_default(count);
    guard.complete();
  }

  constexpr deque(size_type count, const T& value, const allocator_type& alloc = allocator_type{})
      : allocator_(alloc), map_allocator_(allocator_) {
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_storage();
    });
    append_fill(count, value);
    guard.complete();
  }

  template <std::input_iterator InputIt>
  constexpr deque(InputIt first, InputIt last, const allocator_type& alloc = allocator_type{})
      : allocator_(alloc), map_allocator_(allocator_) {
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_storage();
    });
    append_iterator_range(first, last);
    guard.complete();
  }

  constexpr deque(std::initializer_list<T> init, const allocator_type& alloc = allocator_type{})
      : deque(init.begin(), init.end(), alloc) {}

  constexpr deque(const deque& other)
      : allocator_(allocator_traits::select_on_container_copy_construction(other.allocator_)),
        map_allocator_(allocator_) {
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_storage();
    });
    append_iterator_range(other.begin(), other.end());
    guard.complete();
  }

  constexpr deque(deque&& other) noexcept
      : map_(std::exchange(other.map_, nullptr)),
        map_capacity_(std::exchange(other.map_capacity_, 0)),
        first_block_(std::exchange(other.first_block_, 0)),
        first_offset_(std::exchange(other.first_offset_, 0)),
        size_(std::exchange(other.size_, 0)),
        allocator_(std::move(other.allocator_)),
        map_allocator_(std::move(other.map_allocator_)) {}

  constexpr deque(const deque& other, const std::type_identity_t<Allocator>& alloc)
      : allocator_(alloc), map_allocator_(allocator_) {
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_storage();
    });
    append_iterator_range(other.begin(), other.end());
    guard.complete();
  }

  constexpr deque(deque&& other, const std::type_identity_t<Allocator>& alloc)
      : allocator_(alloc), map_allocator_(allocator_) {
    if (storage_compatible_with(other)) {
      take_storage_from(other);
    } else {
      auto guard = detail::make_exception_guard([&] {
        destroy_and_deallocate_storage();
      });
      append_iterator_range(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
      guard.complete();
    }
  }

  template <concepts::container_compatible_range<T> R>
  constexpr deque(std::from_range_t, R&& range, const allocator_type& alloc = allocator_type{})
      : allocator_(alloc), map_allocator_(allocator_) {
    auto guard = detail::make_exception_guard([&] {
      destroy_and_deallocate_storage();
    });
    append_range(std::forward<R>(range));
    guard.complete();
  }

  constexpr ~deque() { destroy_and_deallocate_storage(); }

  constexpr deque& operator=(const deque& other);

  constexpr deque& operator=(deque&& other) noexcept(meta::is_nothrow_container_move_assignable_v<allocator_type, T>);

  constexpr deque& operator=(std::initializer_list<T> init);

  template <std::input_iterator InputIt>
  constexpr void assign(InputIt first, InputIt last);

  template <concepts::container_compatible_range<T> R>
  constexpr void assign_range(R&& range);

  constexpr void assign(size_type count, const T& value);
  constexpr void assign(std::initializer_list<T> init);

  [[nodiscard]] constexpr allocator_type get_allocator() const noexcept { return allocator_; }

  // ========================================================================
  // Element access
  // ========================================================================

  [[nodiscard]] constexpr reference at(size_type pos) {
    if (pos >= size_) {
      throw std::out_of_range("chaistl::deque::at");
    }
    return (*this)[pos];
  }

  [[nodiscard]] constexpr const_reference at(size_type pos) const {
    if (pos >= size_) {
      throw std::out_of_range("chaistl::deque::at");
    }
    return (*this)[pos];
  }

  [[nodiscard]] constexpr reference operator[](size_type pos) noexcept {
    CHAI_HARDENED(pos < size_, "chaistl::deque::operator[]: out of bounds");
    return *element_pointer(pos);
  }

  [[nodiscard]] constexpr const_reference operator[](size_type pos) const noexcept {
    CHAI_HARDENED(pos < size_, "chaistl::deque::operator[]: out of bounds");
    return *element_pointer(pos);
  }

  [[nodiscard]] constexpr reference front() noexcept {
    CHAI_HARDENED(!empty(), "chaistl::deque::front: empty container");
    return (*this)[0];
  }

  [[nodiscard]] constexpr const_reference front() const noexcept {
    CHAI_HARDENED(!empty(), "chaistl::deque::front: empty container");
    return (*this)[0];
  }

  [[nodiscard]] constexpr reference back() noexcept {
    CHAI_HARDENED(!empty(), "chaistl::deque::back: empty container");
    return (*this)[size_ - 1];
  }

  [[nodiscard]] constexpr const_reference back() const noexcept {
    CHAI_HARDENED(!empty(), "chaistl::deque::back: empty container");
    return (*this)[size_ - 1];
  }

  // ========================================================================
  // Iterators
  // ========================================================================

  [[nodiscard]] constexpr iterator begin() noexcept { return iterator_at(0); }

  [[nodiscard]] constexpr const_iterator begin() const noexcept { return iterator_at(0); }

  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }

  [[nodiscard]] constexpr iterator end() noexcept { return iterator_at(size_); }

  [[nodiscard]] constexpr const_iterator end() const noexcept { return iterator_at(size_); }

  [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }

  [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

  [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

  [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }

  [[nodiscard]] constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

  [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

  [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return rend(); }

  // ========================================================================
  // Capacity
  // ========================================================================

  [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

  [[nodiscard]] constexpr size_type size() const noexcept { return size_; }

  [[nodiscard]] constexpr size_type max_size() const noexcept { return allocator_traits::max_size(allocator_); }

  constexpr void resize(size_type count);
  constexpr void resize(size_type count, const T& value);
  constexpr void shrink_to_fit();

  // ========================================================================
  // Modifiers
  // ========================================================================

  template <class... Args>
  constexpr reference emplace_front(Args&&... args) {
    check_size(size_ + 1);
    return construct_front(std::forward<Args>(args)...);
  }

  template <class... Args>
  constexpr reference emplace_back(Args&&... args) {
    check_size(size_ + 1);
    return construct_back(std::forward<Args>(args)...);
  }

  constexpr void push_front(const T& value);
  constexpr void push_front(T&& value);

  template <concepts::container_compatible_range<T> R>
  constexpr void prepend_range(R&& range);

  constexpr void push_back(const T& value);
  constexpr void push_back(T&& value);

  template <concepts::container_compatible_range<T> R>
  constexpr void append_range(R&& range);

  template <class... Args>
  constexpr iterator emplace(const_iterator position, Args&&... args);
  constexpr iterator insert(const_iterator position, const T& value);
  constexpr iterator insert(const_iterator position, T&& value);
  constexpr iterator insert(const_iterator position, size_type count, const T& value);

  template <std::input_iterator InputIt>
  constexpr iterator insert(const_iterator position, InputIt first, InputIt last);

  template <concepts::container_compatible_range<T> R>
  constexpr iterator insert_range(const_iterator position, R&& range);

  constexpr iterator insert(const_iterator position, std::initializer_list<T> init);

  constexpr void pop_front() noexcept {
    CHAI_HARDENED(!empty(), "chaistl::deque::pop_front: empty container");
    pointer slot = element_pointer(0);
    allocator_traits::destroy(allocator_, std::to_address(slot));
    --size_;
    ++first_offset_;
    if (first_offset_ == block_size) {
      first_offset_ = 0;
      ++first_block_;
    }
    if (size_ == 0) {
      first_offset_ = block_size / 2;
    }
  }

  constexpr void pop_back() noexcept {
    CHAI_HARDENED(!empty(), "chaistl::deque::pop_back: empty container");
    pointer slot = element_pointer(size_ - 1);
    allocator_traits::destroy(allocator_, std::to_address(slot));
    --size_;
  }

  constexpr iterator erase(const_iterator position);
  constexpr iterator erase(const_iterator first, const_iterator last);

  constexpr void swap(deque& other) noexcept(meta::is_nothrow_container_swappable_v<allocator_type>) {
    using std::swap;
    swap(map_, other.map_);
    swap(map_capacity_, other.map_capacity_);
    swap(first_block_, other.first_block_);
    swap(first_offset_, other.first_offset_);
    swap(size_, other.size_);
    if constexpr (meta::propagate_on_container_swap_v<allocator_type>) {
      swap(allocator_, other.allocator_);
      swap(map_allocator_, other.map_allocator_);
    }
  }

  constexpr void clear() noexcept {
    detail::allocator_destroy_forward(allocator_, begin(), end());
    size_ = 0;
  }
};

// ===================================
/*  Deque Assignment / Assign        */
// ===================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr deque<T, Allocator>& deque<T, Allocator>::operator=(const deque& other) {
  if (this == &other) {
    return *this;
  }
  if constexpr (meta::propagate_on_container_copy_assignment_v<allocator_type>) {
    deque copy(other, other.get_allocator());
    destroy_and_deallocate_storage();
    copy_allocator_from(other);
    take_storage_from(copy);
  } else {
    deque copy(other, get_allocator());
    replace_storage_from(copy);
  }
  return *this;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr deque<T, Allocator>& deque<T, Allocator>::operator=(deque&& other) noexcept(
    meta::is_nothrow_container_move_assignable_v<allocator_type, T>) {
  if (this == &other) {
    return *this;
  }

  if constexpr (meta::propagate_on_container_move_assignment_v<allocator_type>) {
    destroy_and_deallocate_storage();
    move_allocator_from(other);
    take_storage_from(other);
  } else if constexpr (meta::allocator_is_always_equal_v<allocator_type>) {
    replace_storage_from(other);
  } else if (storage_compatible_with(other)) {
    replace_storage_from(other);
  } else {
    assign(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
  }
  return *this;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr deque<T, Allocator>& deque<T, Allocator>::operator=(std::initializer_list<T> init) {
  assign(init.begin(), init.end());
  return *this;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <std::input_iterator InputIt>
constexpr void deque<T, Allocator>::assign(InputIt first, InputIt last) {
  deque copy(first, last, get_allocator());
  swap(copy);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <concepts::container_compatible_range<T> R>
constexpr void deque<T, Allocator>::assign_range(R&& range) {
  deque copy(std::from_range, std::forward<R>(range), get_allocator());
  swap(copy);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void deque<T, Allocator>::assign(size_type count, const T& value) {
  deque copy(count, value, get_allocator());
  swap(copy);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void deque<T, Allocator>::assign(std::initializer_list<T> init) {
  assign(init.begin(), init.end());
}

// ===========================
/*  Deque Capacity Handling  */
// ===========================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void deque<T, Allocator>::resize(size_type count) {
  pop_back_until(count);
  append_default(count - size());
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void deque<T, Allocator>::resize(size_type count, const T& value) {
  pop_back_until(count);
  append_fill(count - size(), value);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void deque<T, Allocator>::shrink_to_fit() {
  if (empty()) {
    destroy_and_deallocate_storage();
    return;
  }

  deque compact(get_allocator());
  compact.append_iterator_range(std::make_move_iterator(begin()), std::make_move_iterator(end()));
  swap(compact);
}

// ======================
/*  Deque Modifier Logic */
// ======================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void deque<T, Allocator>::push_front(const T& value) {
  emplace_front(value);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void deque<T, Allocator>::push_front(T&& value) {
  emplace_front(std::move(value));
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <concepts::container_compatible_range<T> R>
constexpr void deque<T, Allocator>::prepend_range(R&& range) {
  deque inserted(std::from_range, std::forward<R>(range), get_allocator());
  for (auto current = inserted.rbegin(); current != inserted.rend(); ++current) {
    emplace_front(std::move(*current));
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void deque<T, Allocator>::push_back(const T& value) {
  emplace_back(value);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void deque<T, Allocator>::push_back(T&& value) {
  emplace_back(std::move(value));
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <concepts::container_compatible_range<T> R>
constexpr void deque<T, Allocator>::append_range(R&& range) {
  for (auto&& value : range) {
    emplace_back(std::forward<decltype(value)>(value));
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class... Args>
constexpr typename deque<T, Allocator>::iterator deque<T, Allocator>::emplace(const_iterator position, Args&&... args) {
  const auto offset = static_cast<size_type>(position - begin());
  if (offset == 0) {
    emplace_front(std::forward<Args>(args)...);
    return begin();
  }
  if (offset == size()) {
    emplace_back(std::forward<Args>(args)...);
    return iterator_at(offset);
  }

  T value(std::forward<Args>(args)...);
  if (offset < size() / 2) {
    emplace_front(std::move(front()));
    std::move(begin() + 2, begin() + static_cast<difference_type>(offset + 1), begin() + 1);
  } else {
    const size_type old_size = size();
    emplace_back(std::move(back()));
    std::move_backward(begin() + static_cast<difference_type>(offset),
                       begin() + static_cast<difference_type>(old_size - 1),
                       begin() + static_cast<difference_type>(old_size));
  }
  (*this)[offset] = std::move(value);
  return iterator_at(offset);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename deque<T, Allocator>::iterator deque<T, Allocator>::insert(const_iterator position, const T& value) {
  return emplace(position, value);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename deque<T, Allocator>::iterator deque<T, Allocator>::insert(const_iterator position, T&& value) {
  return emplace(position, std::move(value));
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename deque<T, Allocator>::iterator deque<T, Allocator>::insert(const_iterator position,
                                                                             size_type count,
                                                                             const T& value) {
  const auto offset = position - begin();
  if (count == 0) {
    return begin() + offset;
  }
  if (offset == 0) {
    std::fill_n(std::front_inserter(*this), count, value);
    return begin();
  }
  if (static_cast<size_type>(offset) == size()) {
    std::fill_n(std::back_inserter(*this), count, value);
    return begin() + offset;
  }
  T copied_value(value);
  insert_fill_with_rebuild(static_cast<size_type>(offset), count, copied_value);
  return begin() + offset;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <std::input_iterator InputIt>
constexpr typename deque<T, Allocator>::iterator deque<T, Allocator>::insert(const_iterator position,
                                                                             InputIt first,
                                                                             InputIt last) {
  const auto offset = position - begin();
  deque inserted(first, last, get_allocator());
  insert_range_buffer(position, inserted);
  return begin() + offset;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <concepts::container_compatible_range<T> R>
constexpr typename deque<T, Allocator>::iterator deque<T, Allocator>::insert_range(const_iterator position, R&& range) {
  const auto offset = position - begin();
  if (position == end()) {
    append_range(std::forward<R>(range));
    return begin() + offset;
  }
  if (position == begin()) {
    prepend_range(std::forward<R>(range));
    return begin();
  }
  deque inserted(std::from_range, std::forward<R>(range), get_allocator());
  insert_range_buffer(begin() + offset, inserted);
  return begin() + offset;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename deque<T, Allocator>::iterator deque<T, Allocator>::insert(const_iterator position,
                                                                             std::initializer_list<T> init) {
  return insert(position, init.begin(), init.end());
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename deque<T, Allocator>::iterator deque<T, Allocator>::erase(const_iterator position) {
  return erase(position, position + 1);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr typename deque<T, Allocator>::iterator deque<T, Allocator>::erase(const_iterator first, const_iterator last) {
  const auto first_offset = static_cast<size_type>(first - begin());
  const auto last_offset = static_cast<size_type>(last - begin());
  if (first_offset == last_offset) {
    return iterator_at(first_offset);
  }

  const size_type count = last_offset - first_offset;

  // P4102R0: Container insertion and erasure should be allowed to relocate.
  // Deque erase currently uses move_backward/move to shift elements, then
  // pops from the nearer end. For trivially relocatable types, the entire
  // shift could be replaced with memmove across block boundaries. However,
  // deque's segmented storage makes this more complex than vector's flat
  // array — each block boundary requires separate memmove calls.
  //
  // When P4102 is adopted and is_trivially_relocatable_v is available,
  // consider a block-aware memmove path for the shift operation.

  if (first_offset < size() - last_offset) {
    std::move_backward(begin(),
                       begin() + static_cast<difference_type>(first_offset),
                       begin() + static_cast<difference_type>(last_offset));
    for (size_type index = 0; index < count; ++index) {
      pop_front();
    }
  } else {
    std::move(begin() + static_cast<difference_type>(last_offset),
              end(),
              begin() + static_cast<difference_type>(first_offset));
    for (size_type index = 0; index < count; ++index) {
      pop_back();
    }
  }
  return iterator_at(first_offset);
}

// =================================
/*  Deque Private Helper Functions */
// =================================

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void deque<T, Allocator>::append_default(size_type count) {
  for (size_type index = 0; index < count; ++index) {
    emplace_back();
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void deque<T, Allocator>::append_fill(size_type count, const T& value) {
  for (size_type index = 0; index < count; ++index) {
    emplace_back(value);
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <std::input_iterator InputIt>
constexpr void deque<T, Allocator>::append_iterator_range(InputIt first, InputIt last) {
  for (; first != last; ++first) {
    emplace_back(*first);
  }
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void deque<T, Allocator>::insert_fill_with_rebuild(size_type offset, size_type count, const T& value) {
  rebuild_with_insertion(offset, [&](deque& result) {
    std::fill_n(std::back_inserter(result), count, value);
  });
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void deque<T, Allocator>::insert_range_buffer(const_iterator position, deque& inserted) {
  const auto offset = position - begin();
  if (inserted.empty()) {
    return;
  }
  if (offset == 0) {
    std::move(inserted.rbegin(), inserted.rend(), std::front_inserter(*this));
    return;
  }
  if (static_cast<size_type>(offset) == size()) {
    std::move(inserted.begin(), inserted.end(), std::back_inserter(*this));
    return;
  }

  rebuild_with_insertion(static_cast<size_type>(offset), [&](deque& result) {
    std::move(inserted.begin(), inserted.end(), std::back_inserter(result));
  });
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <bool Const>
constexpr auto deque<T, Allocator>::iterator_impl<Const>::operator+=(difference_type offset) noexcept
    -> iterator_impl& {
  if (offset == 0) {
    return *this;
  }

  offset += current_ - block_begin();
  if (offset > 0) {
    map_slot_ += offset / block_size;
    current_ = block_begin() + offset % block_size;
  } else {
    const difference_type distance_from_previous_block_end = static_cast<difference_type>(block_size) - 1 - offset;
    map_slot_ -= distance_from_previous_block_end / block_size;
    current_ = block_begin() + (block_size - 1 - distance_from_previous_block_end % block_size);
  }
  return *this;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
template <class InsertFn>
constexpr void deque<T, Allocator>::rebuild_with_insertion(size_type offset, InsertFn&& insert_fn) {
  deque result(get_allocator());
  auto insertion_point = begin() + static_cast<difference_type>(offset);
  std::move(begin(), insertion_point, std::back_inserter(result));
  std::forward<InsertFn>(insert_fn)(result);
  std::move(insertion_point, end(), std::back_inserter(result));
  swap(result);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
[[nodiscard]] constexpr bool operator==(const deque<T, Allocator>& lhs, const deque<T, Allocator>& rhs) {
  return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
[[nodiscard]] constexpr auto operator<=>(const deque<T, Allocator>& lhs, const deque<T, Allocator>& rhs) {
  return chaistl::default_three_way_compare(lhs, rhs);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator>
constexpr void swap(deque<T, Allocator>& lhs,
                    deque<T, Allocator>& rhs) noexcept(meta::is_nothrow_container_swappable_v<Allocator>) {
  lhs.swap(rhs);
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator, class U = T>
constexpr typename deque<T, Allocator>::size_type erase(deque<T, Allocator>& container, const U& value) {
  auto first = std::remove(container.begin(), container.end(), value);
  auto removed = static_cast<typename deque<T, Allocator>::size_type>(std::distance(first, container.end()));
  container.erase(first, container.end());
  return removed;
}

template <concepts::container_element T, concepts::allocator_for<T> Allocator, class Predicate>
constexpr typename deque<T, Allocator>::size_type erase_if(deque<T, Allocator>& container, Predicate predicate) {
  auto first = std::remove_if(container.begin(), container.end(), predicate);
  auto removed = static_cast<typename deque<T, Allocator>::size_type>(std::distance(first, container.end()));
  container.erase(first, container.end());
  return removed;
}

template <std::input_iterator InputIt, class Allocator = allocator<std::iter_value_t<InputIt>>>
deque(InputIt, InputIt, Allocator = Allocator()) -> deque<std::iter_value_t<InputIt>, Allocator>;

template <class R, class Allocator = allocator<std::ranges::range_value_t<R>>>
  requires concepts::container_compatible_range<R, std::ranges::range_value_t<R>>
deque(std::from_range_t, R&&, Allocator = Allocator()) -> deque<std::ranges::range_value_t<R>, Allocator>;

namespace pmr {

template <concepts::container_element T>
using deque = chaistl::deque<T, std::pmr::polymorphic_allocator<T>>;

}  // namespace pmr

}  // namespace chaistl
