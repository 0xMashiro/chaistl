// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// ring_buffer - Fixed-capacity circular buffer with overflow policy
// ============================================================================
//
// Architecture:
//   - Owns raw storage for a fixed capacity and maps logical positions onto
//     physical slots with modular arithmetic.
//   - Separates core circular-buffer mechanics from overflow behavior through
//     a policy: overwrite_policy and reject_policy model different production
//     use cases without changing the container layout.
//   - Iterators traverse logical order, not physical slot order.
//
// Standardization archaeology:
//   - There is still no std::ring_buffer. SG14's P0059 explored ring_span and
//     ring vocabulary, but the proposal line stalled because real users want
//     different ownership, overflow, and concurrency semantics.
//   - Boost.CircularBuffer and EASTL-style fixed/ring containers show the
//     industrial pattern: the data structure is simple, but the policy surface
//     is where requirements diverge.
//
// Non-standard extensions:
//   - The whole container is experimental and non-standard.
//   - Overflow behavior is explicit in the Policy parameter rather than baked
//     into the container.
//
// References:
//   - P0059R4: https://wg21.link/p0059r4
//   - Boost.CircularBuffer: https://www.boost.org/doc/libs/release/doc/html/circular_buffer.html
//   - EASTL: https://github.com/electronicarts/EASTL

#include <chaistl/algorithm/comparison.hpp>
#include <chaistl/concepts/allocator.hpp>
#include <chaistl/concepts/container_compatible_range.hpp>
#include <chaistl/concepts/container_element.hpp>
#include <chaistl/experimental/containers/sequence/ring_buffer/policies.hpp>
#include <chaistl/iterator/adapter/reverse.hpp>
#include <chaistl/iterator/interface.hpp>
#include <chaistl/memory/allocator.hpp>
#include <chaistl/memory/detail/lifetime/allocator_uninitialized.hpp>
#include <chaistl/memory/detail/lifetime/exception_guard.hpp>
#include <chaistl/meta/type_traits.hpp>
#include <chaistl/utility/hardening.hpp>

#include <algorithm>
#include <compare>
#include <concepts>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

namespace chaistl::experimental {

namespace detail::ring_buffer {

// ============================================================================
// Policy concept
// ============================================================================

// Checked at the class template head, where the container type is still
// incomplete. The full protocol is checked by full_behavior_policy below.
template <class P>
concept overflow_policy = std::semiregular<P> && requires { typename P::capacity_behavior; };

template <class P, class Core, class T>
concept full_behavior_policy =
    overflow_policy<P> && requires(P policy, Core& core, const T& value, typename Core::iterator pos) {
      { policy.push_back_full(core, value) } -> std::same_as<bool>;
      { policy.push_front_full(core, value) } -> std::same_as<bool>;
      { policy.insert_full(core, pos, value) } -> std::same_as<bool>;
    };

}  // namespace detail::ring_buffer

// ============================================================================
// ring_buffer container
// ============================================================================

/**
 * @brief Fixed-capacity circular buffer with pluggable overflow policy.
 *
 * @ingroup experimental_containers_sequence
 *
 * A ring_buffer stores elements in a circular array. When full, the behavior
 * is determined by the Policy template parameter:
 *
 * - @c overwrite_policy (default): Overwrite the oldest elements. Suitable
 *   for logs, audio buffers, rolling windows.
 * - @c reject_policy: Reject new elements and count drops. Suitable for
 *   telemetry, network packets, critical data.
 *
 * @par Historical Context
 *
 * P0059 (SG14, 2015-2018) proposed std::ring_span and std::ring but was
 * abandoned because "everyone who wants ringbuffers wants something slightly
 * different" [O'Dwyer 2023]. This implementation targets the owning,
 * fixed-capacity, single-threaded subset.
 *
 * @par References
 * - [P0059R4] Guy Davidson, Arthur O'Dwyer.
 *   https://wg21.link/p0059r4
 * - [Boost.CB] Jan Gaspar.
 *   https://www.boost.org/doc/libs/release/doc/html/circular_buffer.html
 * - [EASTL] Electronic Arts.
 *   https://github.com/electronicarts/EASTL
 * - [Hubble 2026] "How to Implement a Ring Buffer for Embedded Logging
 *   and Telemetry."
 * - [O'Dwyer 2023] StackOverflow.
 */
template <chaistl::concepts::container_element T,
          chaistl::concepts::allocator_for<T> Allocator = chaistl::allocator<T>,
          detail::ring_buffer::overflow_policy Policy = overwrite_policy>
class ring_buffer {
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
  using policy_type = Policy;

  // ========================================================================
  // Iterator implementation (nested class)
  // ========================================================================

  template <bool Const>
  class iterator_impl : public chaistl::iterator_interface {
   public:
    using iterator_category = std::random_access_iterator_tag;
    using iterator_concept = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = typename allocator_traits::difference_type;
    using reference = std::conditional_t<Const, const T&, T&>;
    using pointer =
        std::conditional_t<Const, typename allocator_traits::const_pointer, typename allocator_traits::pointer>;

    // Default constructor
    constexpr iterator_impl() noexcept = default;

    // Core primitives required by iterator_interface
    [[nodiscard]] constexpr reference operator*() const noexcept { return *buffer_->element_at_logical(index_); }

    constexpr iterator_impl& operator++() noexcept {
      ++index_;
      return *this;
    }

    constexpr iterator_impl& operator--() noexcept {
      --index_;
      return *this;
    }

    constexpr iterator_impl& operator+=(difference_type n) noexcept {
      index_ += static_cast<size_type>(n);
      return *this;
    }

    constexpr iterator_impl& operator-=(difference_type n) noexcept {
      index_ -= static_cast<size_type>(n);
      return *this;
    }

    [[nodiscard]] constexpr difference_type operator-(const iterator_impl& other) const noexcept {
      return static_cast<difference_type>(index_) - static_cast<difference_type>(other.index_);
    }

    // Hidden friends for comparison
    [[nodiscard]] friend constexpr bool operator==(const iterator_impl& lhs, const iterator_impl& rhs) noexcept {
      return lhs.buffer_ == rhs.buffer_ && lhs.index_ == rhs.index_;
    }

    [[nodiscard]] friend constexpr std::strong_ordering operator<=>(const iterator_impl& lhs,
                                                                    const iterator_impl& rhs) noexcept {
      if (lhs.buffer_ != rhs.buffer_) {
        return lhs.buffer_ <=> rhs.buffer_;
      }
      return lhs.index_ <=> rhs.index_;
    }

    // The derived operator++/--/- declarations hide the base class overloads
    // of the same name (standard name hiding, not a compiler bug), so the
    // postfix ++/-- and iterator-n subtraction from iterator_interface must
    // be re-exposed with using-declarations.
    using chaistl::iterator_interface::operator++;
    using chaistl::iterator_interface::operator--;
    using chaistl::iterator_interface::operator-;

    // Non-member operator+ for symmetric addition
    [[nodiscard]] friend constexpr iterator_impl operator+(difference_type n, const iterator_impl& it) noexcept {
      return it + n;
    }

    // Conversion from non-const to const iterator
    template <bool OtherConst>
      requires(Const && !OtherConst)
    constexpr iterator_impl(const iterator_impl<OtherConst>& other) noexcept
        : buffer_(other.buffer_), index_(other.index_) {}

   private:
    using buffer_pointer = std::conditional_t<Const, const ring_buffer*, ring_buffer*>;

    buffer_pointer buffer_;
    size_type index_;  // logical index [0, buffer_->size()]

    constexpr iterator_impl(buffer_pointer buffer, size_type index) noexcept : buffer_(buffer), index_(index) {}

    friend class ring_buffer;
    template <bool C>
    friend class iterator_impl;
  };

  using iterator = iterator_impl<false>;
  using const_iterator = iterator_impl<true>;
  using reverse_iterator = chaistl::reverse_iterator<iterator>;
  using const_reverse_iterator = chaistl::reverse_iterator<const_iterator>;

  // ========================================================================
  // Policy protocol surface (boost::iterator_core_access pattern)
  //
  // Policies mutate the container exclusively through these forwarders, so
  // third-party policies need no friendship with ring_buffer. Not intended
  // for general use — calling these directly can break container invariants.
  // ========================================================================

  struct core_access {
    template <class U>
    static constexpr void replace_at_logical(ring_buffer& rb, size_type logical_index, U&& value) {
      *rb.element_at_logical(logical_index) = std::forward<U>(value);
    }

    static constexpr void advance_begin(ring_buffer& rb) noexcept { rb.advance_begin(); }

    static constexpr void retreat_begin(ring_buffer& rb) noexcept { rb.retreat_begin(); }

    template <class U>
    static constexpr void shift_tail_and_insert(ring_buffer& rb, iterator pos, U&& value) {
      rb.shift_tail_and_insert(pos, std::forward<U>(value));
    }
  };

  // ========================================================================
  // Constructors / Destructor
  // ========================================================================

  constexpr ring_buffer() noexcept(noexcept(allocator_type{})) = default;

  explicit constexpr ring_buffer(const allocator_type& alloc) noexcept : allocator_(alloc) {}

  explicit constexpr ring_buffer(size_type capacity, const allocator_type& alloc = allocator_type{})
      : allocator_(alloc) {
    if (capacity > 0) {
      data_ = allocator_traits::allocate(allocator_, capacity);
      capacity_ = capacity;
    }
  }

  constexpr ring_buffer(size_type count, const T& value, const allocator_type& alloc = allocator_type{})
      : allocator_(alloc) {
    if (count > 0) {
      data_ = allocator_traits::allocate(allocator_, count);
      capacity_ = count;
      size_ = count;
      chaistl::detail::uninitialized_allocator_fill_n(allocator_, data_, count, value);
    }
  }

  template <std::input_iterator InputIt>
  constexpr ring_buffer(InputIt first, InputIt last, const allocator_type& alloc = allocator_type{})
      : allocator_(alloc) {
    auto distance = static_cast<size_type>(std::distance(first, last));
    if (distance > 0) {
      data_ = allocator_traits::allocate(allocator_, distance);
      capacity_ = distance;
      size_ = distance;
      chaistl::detail::uninitialized_allocator_copy(allocator_, first, last, data_);
    }
  }

  constexpr ring_buffer(std::initializer_list<T> init, const allocator_type& alloc = allocator_type{})
      : ring_buffer(init.begin(), init.end(), alloc) {}

  template <chaistl::concepts::container_compatible_range<T> R>
  constexpr ring_buffer(std::from_range_t, R&& range, const allocator_type& alloc = allocator_type{})
      : allocator_(alloc) {
    if constexpr (std::ranges::sized_range<R>) {
      auto distance = static_cast<size_type>(std::ranges::size(range));
      if (distance > 0) {
        data_ = allocator_traits::allocate(allocator_, distance);
        capacity_ = distance;
        size_ = distance;
        chaistl::detail::uninitialized_allocator_copy(
            allocator_, std::ranges::begin(range), std::ranges::end(range), data_);
      }
    } else {
      for (auto&& elem : range) {
        push_back(std::forward<decltype(elem)>(elem));
      }
    }
  }

  constexpr ring_buffer(const ring_buffer& other)
      : begin_(0),
        size_(other.size_),
        capacity_(other.capacity_),
        allocator_(allocator_traits::select_on_container_copy_construction(other.allocator_)),
        policy_(other.policy_) {
    if (capacity_ > 0) {
      data_ = allocator_traits::allocate(allocator_, capacity_);
      size_type constructed = 0;
      auto guard = chaistl::detail::make_exception_guard([&] {
        for (size_type i = 0; i < constructed; ++i) {
          allocator_traits::destroy(allocator_, std::to_address(data_ + i));
        }
        allocator_traits::deallocate(allocator_, data_, capacity_);
      });
      for (; constructed < size_; ++constructed) {
        allocator_traits::construct(
            allocator_, std::to_address(data_ + constructed), *other.element_at_logical(constructed));
      }
      guard.complete();
    }
  }

  constexpr ring_buffer(const ring_buffer& other, const std::type_identity_t<Allocator>& alloc)
      : begin_(0), size_(other.size_), capacity_(other.capacity_), allocator_(alloc), policy_(other.policy_) {
    if (capacity_ > 0) {
      data_ = allocator_traits::allocate(allocator_, capacity_);
      size_type constructed = 0;
      auto guard = chaistl::detail::make_exception_guard([&] {
        for (size_type i = 0; i < constructed; ++i) {
          allocator_traits::destroy(allocator_, std::to_address(data_ + i));
        }
        allocator_traits::deallocate(allocator_, data_, capacity_);
      });
      for (; constructed < size_; ++constructed) {
        allocator_traits::construct(
            allocator_, std::to_address(data_ + constructed), *other.element_at_logical(constructed));
      }
      guard.complete();
    }
  }

  constexpr ring_buffer(ring_buffer&& other) noexcept
      : data_(std::exchange(other.data_, pointer{})),
        begin_(std::exchange(other.begin_, size_type{})),
        size_(std::exchange(other.size_, size_type{})),
        capacity_(std::exchange(other.capacity_, size_type{})),
        allocator_(std::move(other.allocator_)),
        policy_(std::exchange(other.policy_, policy_type{})) {}

  constexpr ring_buffer(ring_buffer&& other, const std::type_identity_t<Allocator>& alloc) : allocator_(alloc) {
    if (allocator_ == other.allocator_) {
      data_ = std::exchange(other.data_, pointer{});
      begin_ = std::exchange(other.begin_, size_type{});
      size_ = std::exchange(other.size_, size_type{});
      capacity_ = std::exchange(other.capacity_, size_type{});
      policy_ = std::exchange(other.policy_, policy_type{});
    } else {
      if (other.capacity_ > 0) {
        data_ = allocator_traits::allocate(allocator_, other.capacity_);
        capacity_ = other.capacity_;
        begin_ = other.begin_;
        size_ = other.size_;
        policy_ = other.policy_;
        for (size_type i = 0; i < size_; ++i) {
          allocator_traits::construct(allocator_, std::to_address(element_at_logical(i)), *other.element_at_logical(i));
        }
      }
    }
  }

  constexpr ~ring_buffer() {
    destroy_elements();
    deallocate_storage();
  }

  // ========================================================================
  // Assignment
  // ========================================================================

  constexpr ring_buffer& operator=(const ring_buffer& other) {
    if (this == std::addressof(other)) {
      return *this;
    }

    if constexpr (chaistl::meta::propagate_on_container_copy_assignment_v<allocator_type>) {
      ring_buffer tmp(other, other.allocator_);
      destroy_elements();
      deallocate_storage();
      allocator_ = other.allocator_;
      swap_data(tmp);
    } else {
      ring_buffer tmp(other, allocator_);
      destroy_elements();
      deallocate_storage();
      swap_data(tmp);
    }
    return *this;
  }

  constexpr ring_buffer& operator=(ring_buffer&& other) noexcept(
      chaistl::meta::is_nothrow_container_move_assignable_v<allocator_type, T>) {
    if (this == std::addressof(other)) {
      return *this;
    }

    if constexpr (chaistl::meta::propagate_on_container_move_assignment_v<allocator_type>) {
      destroy_elements();
      deallocate_storage();
      allocator_ = std::move(other.allocator_);
      swap_data(other);
    } else if (allocator_ == other.allocator_) {
      destroy_elements();
      deallocate_storage();
      swap_data(other);
    } else {
      ring_buffer tmp(std::move(other), allocator_);
      destroy_elements();
      deallocate_storage();
      swap_data(tmp);
    }
    return *this;
  }

  // Assignment follows Boost.CircularBuffer semantics: the capacity is reset
  // to the number of source elements (clear()+push_back would silently drop
  // or die on a zero-capacity buffer).
  constexpr ring_buffer& operator=(std::initializer_list<T> init) {
    assign(init.begin(), init.end());
    return *this;
  }

  template <std::input_iterator InputIt>
  constexpr void assign(InputIt first, InputIt last) {
    ring_buffer tmp(first, last, allocator_);
    destroy_elements();
    deallocate_storage();
    swap_data(tmp);
  }

  constexpr void assign(size_type count, const T& value) {
    ring_buffer tmp(count, value, allocator_);
    destroy_elements();
    deallocate_storage();
    swap_data(tmp);
  }

  constexpr void assign(std::initializer_list<T> init) { assign(init.begin(), init.end()); }

  template <chaistl::concepts::container_compatible_range<T> R>
  constexpr void assign_range(R&& range) {
    ring_buffer tmp(std::from_range, std::forward<R>(range), allocator_);
    destroy_elements();
    deallocate_storage();
    swap_data(tmp);
  }

  // ========================================================================
  // Iterators
  // ========================================================================

  [[nodiscard]] constexpr iterator begin() noexcept { return iterator(this, 0); }

  [[nodiscard]] constexpr const_iterator begin() const noexcept { return const_iterator(this, 0); }

  [[nodiscard]] constexpr iterator end() noexcept { return iterator(this, size_); }

  [[nodiscard]] constexpr const_iterator end() const noexcept { return const_iterator(this, size_); }

  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }

  [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }

  [[nodiscard]] constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

  [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

  [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }

  [[nodiscard]] constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

  [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

  [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

  // ========================================================================
  // Capacity
  // ========================================================================

  [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

  [[nodiscard]] constexpr bool full() const noexcept { return size_ == capacity_; }

  [[nodiscard]] constexpr size_type size() const noexcept { return size_; }

  [[nodiscard]] constexpr size_type max_size() const noexcept { return allocator_traits::max_size(allocator_); }

  [[nodiscard]] constexpr size_type capacity() const noexcept { return capacity_; }

  // Boost.CircularBuffer semantics: growing past the current capacity
  // increases the capacity (the policy only governs push/insert overflow,
  // not explicit resizing).
  constexpr void resize(size_type new_size) {
    if (new_size < size_) {
      for (size_type i = new_size; i < size_; ++i) {
        allocator_traits::destroy(allocator_, std::to_address(element_at_logical(i)));
      }
      size_ = new_size;
    } else if (new_size > size_) {
      if (new_size > capacity_) {
        set_capacity(new_size);
      }
      while (size_ < new_size) {
        allocator_traits::construct(allocator_, std::to_address(element_at_end()), T{});
        ++size_;
      }
    }
  }

  constexpr void resize(size_type new_size, const T& value) {
    if (new_size < size_) {
      for (size_type i = new_size; i < size_; ++i) {
        allocator_traits::destroy(allocator_, std::to_address(element_at_logical(i)));
      }
      size_ = new_size;
    } else if (new_size > size_) {
      if (new_size > capacity_) {
        set_capacity(new_size);
      }
      while (size_ < new_size) {
        allocator_traits::construct(allocator_, std::to_address(element_at_end()), value);
        ++size_;
      }
    }
  }

  constexpr void set_capacity(size_type new_capacity) {
    if (new_capacity == capacity_) {
      return;
    }

    pointer new_data = nullptr;
    if (new_capacity > 0) {
      new_data = allocator_traits::allocate(allocator_, new_capacity);
    }

    size_type new_size = (std::min)(size_, new_capacity);
    size_type constructed = 0;
    auto guard = chaistl::detail::make_exception_guard([&] {
      for (size_type i = 0; i < constructed; ++i) {
        allocator_traits::destroy(allocator_, std::to_address(new_data + i));
      }
      if (new_data != pointer{} && new_capacity > 0) {
        allocator_traits::deallocate(allocator_, new_data, new_capacity);
      }
    });
    if (new_size > 0) {
      if constexpr (std::is_nothrow_move_constructible_v<T>) {
        for (; constructed < new_size; ++constructed) {
          allocator_traits::construct(
              allocator_, std::to_address(new_data + constructed), std::move(*element_at_logical(constructed)));
        }
      } else {
        for (; constructed < new_size; ++constructed) {
          allocator_traits::construct(
              allocator_, std::to_address(new_data + constructed), *element_at_logical(constructed));
        }
      }
    }
    guard.complete();

    destroy_elements();
    deallocate_storage();

    data_ = new_data;
    capacity_ = new_capacity;
    begin_ = 0;
    size_ = new_size;
  }

  // ========================================================================
  // Element access
  // ========================================================================

  [[nodiscard]] constexpr reference operator[](size_type pos) noexcept {
    CHAI_HARDENED(pos < size_, "chaistl::experimental::ring_buffer::operator[]: out of bounds");
    return *element_at_logical(pos);
  }

  [[nodiscard]] constexpr const_reference operator[](size_type pos) const noexcept {
    CHAI_HARDENED(pos < size_, "chaistl::experimental::ring_buffer::operator[]: out of bounds");
    return *element_at_logical(pos);
  }

  [[nodiscard]] constexpr reference at(size_type pos) {
    if (pos >= size_) {
      throw std::out_of_range("chaistl::experimental::ring_buffer::at");
    }
    return (*this)[pos];
  }

  [[nodiscard]] constexpr const_reference at(size_type pos) const {
    if (pos >= size_) {
      throw std::out_of_range("chaistl::experimental::ring_buffer::at");
    }
    return (*this)[pos];
  }

  [[nodiscard]] constexpr reference front() noexcept {
    CHAI_HARDENED(!empty(), "chaistl::experimental::ring_buffer::front: empty container");
    return *element_at_logical(0);
  }

  [[nodiscard]] constexpr const_reference front() const noexcept {
    CHAI_HARDENED(!empty(), "chaistl::experimental::ring_buffer::front: empty container");
    return *element_at_logical(0);
  }

  [[nodiscard]] constexpr reference back() noexcept {
    CHAI_HARDENED(!empty(), "chaistl::experimental::ring_buffer::back: empty container");
    return *element_at_logical(size_ - 1);
  }

  [[nodiscard]] constexpr const_reference back() const noexcept {
    CHAI_HARDENED(!empty(), "chaistl::experimental::ring_buffer::back: empty container");
    return *element_at_logical(size_ - 1);
  }

  // ========================================================================
  // Modifiers
  // ========================================================================

  template <class... Args>
  constexpr reference emplace_back(Args&&... args) {
    static_assert(detail::ring_buffer::full_behavior_policy<Policy, ring_buffer, T>,
                  "Policy does not satisfy the ring_buffer full-behavior protocol");
    CHAI_HARDENED(capacity_ > 0,
                  "chaistl::experimental::ring_buffer::emplace_back: zero-capacity buffer "
                  "(use try_push_back for a total alternative)");
    if (size_ < capacity_) {
      allocator_traits::construct(allocator_, std::to_address(element_at_end()), std::forward<Args>(args)...);
      ++size_;
      return back();
    }

    T temp(std::forward<Args>(args)...);
    policy_.push_back_full(*this, std::move(temp));
    // If the policy rejected the element, this is the pre-existing back().
    return back();
  }

  constexpr void push_back(const T& value) { emplace_back(value); }

  constexpr void push_back(T&& value) { emplace_back(std::move(value)); }

  // Total push: reports per-call success instead of requiring callers to
  // diff dropped_count() around the call. Safe on zero-capacity buffers.
  constexpr bool try_push_back(const T& value) { return try_push_back_impl(value); }

  constexpr bool try_push_back(T&& value) { return try_push_back_impl(std::move(value)); }

  constexpr bool try_push_front(const T& value) { return try_push_front_impl(value); }

  constexpr bool try_push_front(T&& value) { return try_push_front_impl(std::move(value)); }

  template <class... Args>
  constexpr reference emplace_front(Args&&... args) {
    CHAI_HARDENED(capacity_ > 0,
                  "chaistl::experimental::ring_buffer::emplace_front: zero-capacity buffer "
                  "(use try_push_front for a total alternative)");
    if (size_ < capacity_) {
      retreat_begin();
      allocator_traits::construct(allocator_, std::to_address(element_at_logical(0)), std::forward<Args>(args)...);
      ++size_;
      return front();
    }

    {
      T temp(std::forward<Args>(args)...);
      policy_.push_front_full(*this, std::move(temp));
      return front();
    }
  }

  constexpr void push_front(const T& value) { emplace_front(value); }

  constexpr void push_front(T&& value) { emplace_front(std::move(value)); }

  constexpr void pop_back() noexcept {
    CHAI_HARDENED(!empty(), "chaistl::experimental::ring_buffer::pop_back: empty container");
    allocator_traits::destroy(allocator_, std::to_address(element_at_logical(size_ - 1)));
    --size_;
  }

  constexpr void pop_front() noexcept {
    CHAI_HARDENED(!empty(), "chaistl::experimental::ring_buffer::pop_front: empty container");
    allocator_traits::destroy(allocator_, std::to_address(element_at_logical(0)));
    advance_begin();
    --size_;
  }

  constexpr iterator insert(const_iterator pos, const T& value) { return emplace(pos, value); }

  constexpr iterator insert(const_iterator pos, T&& value) { return emplace(pos, std::move(value)); }

  template <class... Args>
  constexpr iterator emplace(const_iterator pos, Args&&... args) {
    CHAI_HARDENED(pos >= begin() && pos <= end(), "chaistl::experimental::ring_buffer::emplace: invalid iterator");

    auto offset = static_cast<size_type>(pos - begin());

    if (size_ < capacity_) {
      for (size_type i = size_; i > offset; --i) {
        if (i == size_) {
          allocator_traits::construct(
              allocator_, std::to_address(element_at_logical(i)), std::move(*element_at_logical(i - 1)));
        } else {
          *element_at_logical(i) = std::move(*element_at_logical(i - 1));
        }
      }
      if (offset < size_) {
        *element_at_logical(offset) = T(std::forward<Args>(args)...);
      } else {
        allocator_traits::construct(
            allocator_, std::to_address(element_at_logical(offset)), std::forward<Args>(args)...);
      }
      ++size_;
      return iterator(this, offset);
    }

    T temp(std::forward<Args>(args)...);
    bool handled = policy_.insert_full(*this, iterator(this, offset), std::move(temp));
    if (!handled) {
      return iterator(this, offset);
    }
    // The oldest element was dropped and begin advanced, so the element
    // written at logical `offset` is now at logical `offset - 1`.
    return iterator(this, offset - 1);
  }

  constexpr iterator erase(const_iterator pos) {
    CHAI_HARDENED(pos >= begin() && pos < end(), "chaistl::experimental::ring_buffer::erase: invalid iterator");

    auto offset = static_cast<size_type>(pos - begin());

    for (size_type i = offset; i + 1 < size_; ++i) {
      *element_at_logical(i) = std::move(*element_at_logical(i + 1));
    }

    allocator_traits::destroy(allocator_, std::to_address(element_at_logical(size_ - 1)));
    --size_;

    return iterator(this, offset);
  }

  constexpr iterator erase(const_iterator first, const_iterator last) {
    CHAI_HARDENED(first >= begin() && last <= end() && first <= last,
                  "chaistl::experimental::ring_buffer::erase: invalid range");

    auto first_offset = static_cast<size_type>(first - begin());
    auto last_offset = static_cast<size_type>(last - begin());
    auto count = last_offset - first_offset;

    if (count == 0) {
      return iterator(this, first_offset);
    }

    for (size_type i = first_offset; i + count < size_; ++i) {
      *element_at_logical(i) = std::move(*element_at_logical(i + count));
    }

    for (size_type i = size_ - count; i < size_; ++i) {
      allocator_traits::destroy(allocator_, std::to_address(element_at_logical(i)));
    }

    size_ -= count;
    return iterator(this, first_offset);
  }

  constexpr void swap(ring_buffer& other) noexcept(chaistl::meta::is_nothrow_container_swappable_v<allocator_type>) {
    using std::swap;
    swap(data_, other.data_);
    swap(begin_, other.begin_);
    swap(size_, other.size_);
    swap(capacity_, other.capacity_);
    if constexpr (chaistl::meta::propagate_on_container_swap_v<allocator_type>) {
      swap(allocator_, other.allocator_);
    }
    swap(policy_, other.policy_);
  }

  constexpr void clear() noexcept {
    destroy_elements();
    size_ = 0;
    begin_ = 0;
  }

  // ========================================================================
  // Policy-state interface (available when the policy provides it)
  // ========================================================================

  [[nodiscard]] constexpr size_type dropped_count() const noexcept
    requires requires(const Policy& p) {
      { p.dropped_count() } -> std::convertible_to<size_type>;
    }
  {
    return static_cast<size_type>(policy_.dropped_count());
  }

  constexpr void reset_dropped_count() noexcept
    requires requires(Policy& p) { p.reset_dropped_count(); }
  {
    policy_.reset_dropped_count();
  }

  // ========================================================================
  // Validation (for testing)
  // ========================================================================

  [[nodiscard]] constexpr bool validate() const noexcept { return begin_ < capacity_ || capacity_ == 0; }

 private:
  // ========================================================================
  // Core data members
  // ========================================================================
  pointer data_ = pointer{};
  size_type begin_ = 0;
  size_type size_ = 0;
  size_type capacity_ = 0;
  [[no_unique_address]] allocator_type allocator_;
  // Policy state (e.g. reject_policy's drop counter) lives in the policy;
  // [[no_unique_address]] makes stateless policies cost zero bytes.
  [[no_unique_address]] policy_type policy_;

  // ========================================================================
  // Core helpers (reached by policies through core_access, and by iterators)
  // ========================================================================

  [[nodiscard]] constexpr pointer element_at_logical(size_type logical_index) const noexcept {
    return data_ + ((begin_ + logical_index) % capacity_);
  }

  [[nodiscard]] constexpr pointer element_at_end() const noexcept { return data_ + ((begin_ + size_) % capacity_); }

  constexpr void advance_begin() noexcept { begin_ = (begin_ + 1) % capacity_; }

  constexpr void retreat_begin() noexcept {
    if (begin_ == 0) {
      begin_ = capacity_ - 1;
    } else {
      --begin_;
    }
  }

  // Insert into a FULL buffer at pos, dropping the oldest element.
  // Shifts the tail [pos, end) one slot toward the back; the slot at
  // element_at_logical(size_) wraps onto the oldest element's storage,
  // which is exactly the element being dropped. After advance_begin() the
  // inserted element sits at logical (pos - begin()) - 1.
  template <class U>
  constexpr void shift_tail_and_insert(iterator pos, U&& value) {
    auto offset = static_cast<size_type>(pos - begin());
    for (size_type i = size_; i > offset; --i) {
      *element_at_logical(i) = std::move(*element_at_logical(i - 1));
    }
    *element_at_logical(offset) = std::forward<U>(value);
    advance_begin();
  }

  template <class U>
  constexpr bool try_push_back_impl(U&& value) {
    if (size_ < capacity_) {
      allocator_traits::construct(allocator_, std::to_address(element_at_end()), std::forward<U>(value));
      ++size_;
      return true;
    }
    return policy_.push_back_full(*this, std::forward<U>(value));
  }

  template <class U>
  constexpr bool try_push_front_impl(U&& value) {
    if (size_ < capacity_) {
      retreat_begin();
      allocator_traits::construct(allocator_, std::to_address(element_at_logical(0)), std::forward<U>(value));
      ++size_;
      return true;
    }
    return policy_.push_front_full(*this, std::forward<U>(value));
  }

  constexpr void destroy_elements() noexcept {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      for (size_type i = 0; i < size_; ++i) {
        allocator_traits::destroy(allocator_, std::to_address(element_at_logical(i)));
      }
    }
  }

  constexpr void deallocate_storage() noexcept {
    if (data_ != pointer{} && capacity_ > 0) {
      allocator_traits::deallocate(allocator_, data_, capacity_);
      data_ = pointer{};
      capacity_ = 0;
    }
  }

  constexpr void swap_data(ring_buffer& other) noexcept {
    using std::swap;
    swap(data_, other.data_);
    swap(begin_, other.begin_);
    swap(size_, other.size_);
    swap(capacity_, other.capacity_);
    swap(policy_, other.policy_);
  }

  // Grant non-member operators access
  template <class T2, class A2, class P2>
  friend constexpr bool operator==(const ring_buffer<T2, A2, P2>& lhs, const ring_buffer<T2, A2, P2>& rhs);

  template <class T2, class A2, class P2>
  friend constexpr auto operator<=>(const ring_buffer<T2, A2, P2>& lhs, const ring_buffer<T2, A2, P2>& rhs);
};

// ============================================================================
// Non-member comparison operators
// ============================================================================

template <class T, class Allocator, class Policy>
[[nodiscard]] constexpr bool operator==(const ring_buffer<T, Allocator, Policy>& lhs,
                                        const ring_buffer<T, Allocator, Policy>& rhs) {
  return lhs.size_ == rhs.size_ && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <class T, class Allocator, class Policy>
[[nodiscard]] constexpr auto operator<=>(const ring_buffer<T, Allocator, Policy>& lhs,
                                         const ring_buffer<T, Allocator, Policy>& rhs) {
  return chaistl::default_three_way_compare(lhs, rhs);
}

// ============================================================================
// Non-member swap
// ============================================================================

template <class T, class Allocator, class Policy>
constexpr void swap(ring_buffer<T, Allocator, Policy>& lhs,
                    ring_buffer<T, Allocator, Policy>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

// ============================================================================
// Deduction guides
// ============================================================================

template <class InputIt,
          class Allocator = chaistl::allocator<std::iter_value_t<InputIt>>,
          class Policy = overwrite_policy>
  requires std::input_iterator<InputIt>
ring_buffer(InputIt, InputIt, Allocator = Allocator(), Policy = Policy())
    -> ring_buffer<std::iter_value_t<InputIt>, Allocator, Policy>;

template <class R, class Allocator = chaistl::allocator<std::ranges::range_value_t<R>>, class Policy = overwrite_policy>
  requires std::ranges::input_range<R>
ring_buffer(std::from_range_t, R&&, Allocator = Allocator(), Policy = Policy())
    -> ring_buffer<std::ranges::range_value_t<R>, Allocator, Policy>;

}  // namespace chaistl::experimental
