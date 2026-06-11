// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// span - Non-owning contiguous view
// ============================================================================
//
// Architecture:
//   - Stores a pointer plus either a static extent in the type or a dynamic
//     extent value in the object.
//   - Does not own elements; it is a range view over memory owned by arrays,
//     vectors, C APIs, or other contiguous ranges.
//   - Iterators are raw pointers because span's contract is exactly contiguous
//     traversal over an existing element sequence.
//
// Standardization archaeology:
//   - std::span arrived in C++20 from the GSL/span lineage as the standard
//     vocabulary type for "pointer plus length".
//   - Its main design purpose is to make bounds explicit at interfaces without
//     taking ownership, replacing ad hoc pairs such as (T*, size_t).
//   - The static-extent form lets APIs encode fixed sizes in the type, while
//     dynamic_extent preserves the common runtime-sized case.
//
// Non-standard extensions:
//   - Hardened element access reports contract violations through
//     CHAI_HARDENED.
//
// References:
//   - C++ Draft: https://eel.is/c++draft/views.span
//   - cppreference: https://en.cppreference.com/w/cpp/container/span
//   - GSL span: https://github.com/microsoft/GSL

#include <chaistl/containers/sequence/array.hpp>
#include <chaistl/iterator/adapter/reverse.hpp>
#include <chaistl/utility/hardening.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <limits>
#include <ranges>
#include <type_traits>
#include <utility>

namespace chaistl {

// ======================================================================
// Constants
// ======================================================================

inline constexpr std::size_t dynamic_extent = std::numeric_limits<std::size_t>::max();

// ======================================================================
// Forward declaration
// ======================================================================

template <class ElementType, std::size_t Extent = dynamic_extent>
class span;

namespace detail {

// ----------------------------------------------------------------------
// Type traits
// ----------------------------------------------------------------------

template <class T>
struct is_span : std::false_type {};

template <class T, std::size_t N>
struct is_span<span<T, N>> : std::true_type {};

template <class T>
inline constexpr bool is_span_v = is_span<T>::value;

template <class T>
struct is_std_array : std::false_type {};

template <class T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <class T>
inline constexpr bool is_std_array_v = is_std_array<T>::value;

template <class T>
struct is_chaistl_array : std::false_type {};

template <class T, std::size_t N>
struct is_chaistl_array<array<T, N>> : std::true_type {};

template <class T>
inline constexpr bool is_chaistl_array_v = is_chaistl_array<T>::value;

template <class T>
inline constexpr bool is_any_array_v = is_std_array_v<T> || is_chaistl_array_v<T>;

// ----------------------------------------------------------------------
// Empty type for [[no_unique_address]] optimization
// ----------------------------------------------------------------------

struct empty {};

// ----------------------------------------------------------------------
// Span-compatible iterator concept
// ----------------------------------------------------------------------

template <class It, class T>
concept span_compatible_iterator =
    std::contiguous_iterator<It> &&
    std::convertible_to<std::remove_reference_t<std::iter_reference_t<It>> (*)[], T (*)[]>;

// ----------------------------------------------------------------------
// Span-compatible sentinel concept
// ----------------------------------------------------------------------

template <class Sentinel, class It>
concept span_compatible_sentinel_for =
    std::sized_sentinel_for<Sentinel, It> && !std::convertible_to<Sentinel, std::size_t>;

// ----------------------------------------------------------------------
// Span-compatible range concept
// ----------------------------------------------------------------------

template <class Range, class ElementType>
concept span_compatible_range =
    !is_span_v<std::remove_cvref_t<Range>> && !is_any_array_v<std::remove_cvref_t<Range>> &&
    !std::is_array_v<std::remove_cvref_t<Range>> && std::ranges::contiguous_range<Range> &&
    std::ranges::sized_range<Range> && (std::ranges::borrowed_range<Range> || std::is_const_v<ElementType>) &&
    std::convertible_to<std::remove_reference_t<std::ranges::range_reference_t<Range>> (*)[], ElementType (*)[]>;

// ----------------------------------------------------------------------
// Array-convertible concept (for cv-qualified element conversions)
// ----------------------------------------------------------------------

template <class From, class To>
concept span_array_convertible = std::convertible_to<From (*)[], To (*)[]>;

// ----------------------------------------------------------------------
// Subspan extent computation
// ----------------------------------------------------------------------

template <std::size_t Extent, std::size_t Offset, std::size_t Count>
inline constexpr std::size_t subspan_extent_v = Count != dynamic_extent    ? Count
                                                : Extent != dynamic_extent ? Extent - Offset
                                                                           : dynamic_extent;

}  // namespace detail

// ======================================================================
// span class template
// ======================================================================

template <class ElementType, std::size_t Extent>
class span {
 public:
  // ------------------------------------------------------------------
  // Member types
  // ------------------------------------------------------------------
  using element_type = ElementType;
  using value_type = std::remove_cv_t<ElementType>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = ElementType*;
  using const_pointer = const ElementType*;
  using reference = ElementType&;
  using const_reference = const ElementType&;
  using iterator = pointer;
  using reverse_iterator = chaistl::reverse_iterator<iterator>;

  static constexpr size_type extent = Extent;

  // ------------------------------------------------------------------
  // Storage: use [[no_unique_address]] to eliminate size_ for static extent
  // ------------------------------------------------------------------
 private:
  pointer data_ = nullptr;
  [[no_unique_address]] std::conditional_t<Extent == dynamic_extent, size_type, detail::empty> size_or_empty_;

  // Helper to access size
  [[nodiscard]] constexpr size_type stored_size() const noexcept {
    if constexpr (Extent == dynamic_extent) {
      return size_or_empty_;
    } else {
      return Extent;
    }
  }

 public:
  // ------------------------------------------------------------------
  // Constructors, copy, assignment, destructor
  // ------------------------------------------------------------------

  // Default constructor
  template <std::size_t E = Extent>
    requires(E == dynamic_extent || E == 0)
  constexpr span() noexcept {
    if constexpr (E == dynamic_extent) {
      size_or_empty_ = 0;
    }
  }

  // Iterator + count
  template <detail::span_compatible_iterator<element_type> It>
  constexpr explicit(Extent != dynamic_extent) span(It first, size_type count) : data_(std::to_address(first)) {
    if constexpr (Extent != dynamic_extent) {
      // Hardened check in debug; the assumption stays as an
      // optimization hint when hardening is disabled.
      CHAI_HARDENED(count == Extent, "span(first, count): count != Extent");
      [[assume(count == Extent)]];
    } else {
      size_or_empty_ = count;
    }
  }

  // Iterator + sentinel
  template <detail::span_compatible_iterator<element_type> It, detail::span_compatible_sentinel_for<It> End>
  constexpr explicit(Extent != dynamic_extent) span(It first, End last) : data_(std::to_address(first)) {
    const auto count = static_cast<size_type>(last - first);
    if constexpr (Extent != dynamic_extent) {
      CHAI_HARDENED(count == Extent, "span(first, last): size != Extent");
      [[assume(count == Extent)]];
    } else {
      size_or_empty_ = count;
    }
  }

  // C array (extent is always known)
  template <std::size_t N>
    requires(Extent == dynamic_extent || N == Extent)
  constexpr span(std::type_identity_t<element_type> (&arr)[N]) noexcept : data_(arr) {
    if constexpr (Extent == dynamic_extent) {
      size_or_empty_ = N;
    }
  }

  // chaistl::array
  template <detail::span_array_convertible<element_type> OtherElementType, std::size_t N>
    requires(Extent == dynamic_extent || N == Extent)
  constexpr span(array<OtherElementType, N>& arr) noexcept : data_(arr.data()) {
    if constexpr (Extent == dynamic_extent) {
      size_or_empty_ = N;
    }
  }

  // const chaistl::array
  template <class OtherElementType, std::size_t N>
    requires(Extent == dynamic_extent || N == Extent) &&
            detail::span_array_convertible<const OtherElementType, element_type>
  constexpr span(const array<OtherElementType, N>& arr) noexcept : data_(arr.data()) {
    if constexpr (Extent == dynamic_extent) {
      size_or_empty_ = N;
    }
  }

  // std::array
  template <detail::span_array_convertible<element_type> OtherElementType, std::size_t N>
    requires(Extent == dynamic_extent || N == Extent)
  constexpr span(std::array<OtherElementType, N>& arr) noexcept : data_(arr.data()) {
    if constexpr (Extent == dynamic_extent) {
      size_or_empty_ = N;
    }
  }

  // const std::array
  template <class OtherElementType, std::size_t N>
    requires(Extent == dynamic_extent || N == Extent) &&
            detail::span_array_convertible<const OtherElementType, element_type>
  constexpr span(const std::array<OtherElementType, N>& arr) noexcept : data_(arr.data()) {
    if constexpr (Extent == dynamic_extent) {
      size_or_empty_ = N;
    }
  }

  // Range
  template <detail::span_compatible_range<element_type> Range>
  constexpr explicit(Extent != dynamic_extent) span(Range&& range) : data_(std::ranges::data(range)) {
    if constexpr (Extent != dynamic_extent) {
      CHAI_HARDENED(std::ranges::size(range) == Extent, "span(range): size != Extent");
      [[assume(std::ranges::size(range) == Extent)]];
    } else {
      size_or_empty_ = std::ranges::size(range);
    }
  }

  // P2447R6 (C++26) proposed an initializer_list constructor for span.
  // However, P4144R1 (March 2026) showed this causes silent breaking
  // changes in overload resolution (e.g. span<const bool>{ptr, size}
  // resolves to the il constructor instead of the (ptr, count) one).
  // LEWG voted 16/14/2/0/0 to REMOVE the initializer_list constructor
  // entirely for C++26. We intentionally do NOT provide one here.
  //
  // If a future proposal (e.g. C++29) adds a constrained version that
  // avoids the ambiguity, we can revisit.

  // Copy constructor
  constexpr span(const span&) noexcept = default;

  // Conversion from other span
  template <detail::span_array_convertible<element_type> OtherElementType, std::size_t OtherExtent>
    requires(Extent == dynamic_extent || OtherExtent == dynamic_extent || Extent == OtherExtent)
  constexpr explicit(Extent != dynamic_extent && OtherExtent == dynamic_extent)
      span(const span<OtherElementType, OtherExtent>& other) noexcept
      : data_(other.data()) {
    if constexpr (Extent == dynamic_extent) {
      size_or_empty_ = other.size();
    }
  }

  // Copy assignment
  constexpr span& operator=(const span&) noexcept = default;

  // Destructor
  constexpr ~span() = default;

  // ------------------------------------------------------------------
  // Subviews
  // ------------------------------------------------------------------

  template <std::size_t Count>
  [[nodiscard]] constexpr span<element_type, Count> first() const noexcept {
    if constexpr (Extent != dynamic_extent) {
      static_assert(Count <= Extent, "span::first<Count>(): Count out of range");
    }
    CHAI_HARDENED(Count <= stored_size(), "chaistl::span::first: Count out of range");
    return span<element_type, Count>{data_, Count};
  }

  [[nodiscard]] constexpr span<element_type, dynamic_extent> first(size_type count) const noexcept {
    CHAI_HARDENED(count <= stored_size(), "chaistl::span::first: count out of range");
    return span<element_type, dynamic_extent>{data_, count};
  }

  template <std::size_t Count>
  [[nodiscard]] constexpr span<element_type, Count> last() const noexcept {
    if constexpr (Extent != dynamic_extent) {
      static_assert(Count <= Extent, "span::last<Count>(): Count out of range");
    }
    CHAI_HARDENED(Count <= stored_size(), "chaistl::span::last: Count out of range");
    return span<element_type, Count>{data_ + (stored_size() - Count), Count};
  }

  [[nodiscard]] constexpr span<element_type, dynamic_extent> last(size_type count) const noexcept {
    CHAI_HARDENED(count <= stored_size(), "chaistl::span::last: count out of range");
    return span<element_type, dynamic_extent>{data_ + (stored_size() - count), count};
  }

  template <std::size_t Offset, std::size_t Count = dynamic_extent>
  [[nodiscard]] constexpr span<element_type, detail::subspan_extent_v<Extent, Offset, Count>> subspan() const noexcept {
    if constexpr (Extent != dynamic_extent) {
      static_assert(Offset <= Extent, "span::subspan<Offset>(): Offset out of range");
      static_assert(Count == dynamic_extent || Count <= Extent - Offset,
                    "span::subspan<Offset, Count>(): Offset + Count out of range");
    }
    CHAI_HARDENED(Offset <= stored_size(), "chaistl::span::subspan: Offset out of range");
    if constexpr (Count != dynamic_extent) {
      CHAI_HARDENED(Offset + Count <= stored_size(), "chaistl::span::subspan: Offset + Count out of range");
    }
    constexpr std::size_t NewExtent = detail::subspan_extent_v<Extent, Offset, Count>;
    return span<element_type, NewExtent>{data_ + Offset, Count == dynamic_extent ? stored_size() - Offset : Count};
  }

  [[nodiscard]] constexpr span<element_type, dynamic_extent> subspan(size_type offset,
                                                                     size_type count = dynamic_extent) const noexcept {
    CHAI_HARDENED(offset <= stored_size(), "chaistl::span::subspan: offset out of range");
    CHAI_HARDENED(count == dynamic_extent || offset + count <= stored_size(),
                  "chaistl::span::subspan: offset + count out of range");
    return span<element_type, dynamic_extent>{data_ + offset, count == dynamic_extent ? stored_size() - offset : count};
  }

  // ------------------------------------------------------------------
  // Observers
  // ------------------------------------------------------------------

  [[nodiscard]] constexpr size_type size() const noexcept { return stored_size(); }

  [[nodiscard]] constexpr size_type size_bytes() const noexcept { return stored_size() * sizeof(element_type); }

  [[nodiscard]] constexpr bool empty() const noexcept { return stored_size() == 0; }

  // ------------------------------------------------------------------
  // Element access
  // ------------------------------------------------------------------

  // P2821R5 (C++26): span::at()
  // All other contiguous containers/views (vector, array, deque,
  // string, string_view) offer both operator[] and at(). span was the
  // odd one out. This adds bounds-checked, throwing access.
  // Note: P2833R2 excludes span::at() from the freestanding subset
  // because freestanding environments may not support exceptions.
  [[nodiscard]] constexpr reference at(size_type idx) const {
    if (idx >= size()) {
      throw std::out_of_range("chaistl::span::at");
    }
    return data_[idx];
  }

  [[nodiscard]] constexpr reference operator[](size_type idx) const noexcept {
    CHAI_HARDENED(idx < size(), "chaistl::span::operator[]: out of bounds");
    return data_[idx];
  }

  [[nodiscard]] constexpr reference front() const noexcept {
    CHAI_HARDENED(!empty(), "chaistl::span::front: empty span");
    return data_[0];
  }

  [[nodiscard]] constexpr reference back() const noexcept {
    CHAI_HARDENED(!empty(), "chaistl::span::back: empty span");
    return data_[stored_size() - 1];
  }

  [[nodiscard]] constexpr pointer data() const noexcept { return data_; }

  // ------------------------------------------------------------------
  // Iterators
  // ------------------------------------------------------------------

  [[nodiscard]] constexpr iterator begin() const noexcept { return data_; }

  [[nodiscard]] constexpr iterator end() const noexcept { return data_ + stored_size(); }

  [[nodiscard]] constexpr reverse_iterator rbegin() const noexcept { return reverse_iterator(end()); }

  [[nodiscard]] constexpr reverse_iterator rend() const noexcept { return reverse_iterator(begin()); }
};

// ======================================================================
// Deduction guides
// ======================================================================

template <class It, class EndOrSize>
span(It, EndOrSize) -> span<std::remove_reference_t<std::iter_reference_t<It>>>;

template <class T, std::size_t N>
span(T (&)[N]) -> span<T, N>;

template <class T, std::size_t N>
span(array<T, N>&) -> span<T, N>;

template <class T, std::size_t N>
span(const array<T, N>&) -> span<const T, N>;

template <class T, std::size_t N>
span(std::array<T, N>&) -> span<T, N>;

template <class T, std::size_t N>
span(const std::array<T, N>&) -> span<const T, N>;

template <class Range>
span(Range&&) -> span<std::remove_reference_t<std::ranges::range_reference_t<Range>>>;

// ======================================================================
// Non-member functions
// ======================================================================

// as_bytes / as_writable_bytes
template <class T, std::size_t Extent>
  requires(!std::is_volatile_v<T>)
[[nodiscard]] constexpr auto as_bytes(span<T, Extent> s) noexcept {
  using byte_span = span<const std::byte, Extent == dynamic_extent ? dynamic_extent : Extent * sizeof(T)>;
  return byte_span{reinterpret_cast<const std::byte*>(s.data()), s.size_bytes()};
}

template <class T, std::size_t Extent>
  requires(!std::is_const_v<T> && !std::is_volatile_v<T>)
[[nodiscard]] constexpr auto as_writable_bytes(span<T, Extent> s) noexcept {
  using byte_span = span<std::byte, Extent == dynamic_extent ? dynamic_extent : Extent * sizeof(T)>;
  return byte_span{reinterpret_cast<std::byte*>(s.data()), s.size_bytes()};
}

// Comparison operators
template <class T, std::size_t X, class U, std::size_t Y>
[[nodiscard]] constexpr bool operator==(span<T, X> lhs, span<U, Y> rhs) {
  return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

template <class T, std::size_t X, class U, std::size_t Y>
  requires std::three_way_comparable_with<T, U>
[[nodiscard]] constexpr auto operator<=>(span<T, X> lhs, span<U, Y> rhs) {
  return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
}

// swap
template <class T, std::size_t Extent>
constexpr void swap(span<T, Extent>& lhs, span<T, Extent>& rhs) noexcept {
  auto tmp = lhs;
  lhs = rhs;
  rhs = tmp;
}

}  // namespace chaistl

// ======================================================================
// Ranges opt-in
// ======================================================================

template <class ElementType, std::size_t Extent>
inline constexpr bool std::ranges::enable_borrowed_range<chaistl::span<ElementType, Extent>> = true;

template <class ElementType, std::size_t Extent>
inline constexpr bool std::ranges::enable_view<chaistl::span<ElementType, Extent>> = true;
