// SPDX-License-Identifier: Apache-2.0

// References:
// - std::span overview and member types:
//   https://en.cppreference.com/w/cpp/container/span
//   https://eel.is/c++draft/views.span
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/concepts.hpp>
#include <chaistl/containers/array.hpp>
#include <chaistl/containers/span.hpp>
#include <chaistl/containers/vector.hpp>

#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <ranges>
#include <string>
#include <type_traits>
#include <vector>

namespace {

// ======================================================================
// Compile-time type trait and concept checks
// ======================================================================

static_assert(std::ranges::contiguous_range<chaistl::span<int>>);
static_assert(std::ranges::sized_range<chaistl::span<int>>);
static_assert(std::ranges::borrowed_range<chaistl::span<int>>);
static_assert(std::ranges::view<chaistl::span<int>>);

// Member types
static_assert(std::same_as<chaistl::span<int>::element_type, int>);
static_assert(std::same_as<chaistl::span<int>::value_type, int>);
static_assert(std::same_as<chaistl::span<int>::size_type, std::size_t>);
static_assert(std::same_as<chaistl::span<int>::difference_type, std::ptrdiff_t>);
static_assert(std::same_as<chaistl::span<int>::pointer, int*>);
static_assert(std::same_as<chaistl::span<int>::reference, int&>);
static_assert(std::same_as<chaistl::span<int>::iterator, int*>);

// extent constant
static_assert(chaistl::span<int, 5>::extent == 5);
static_assert(chaistl::span<int>::extent == chaistl::dynamic_extent);

// ======================================================================
// Storage optimization checks
// ======================================================================

// Static-extent span should be pointer-sized
static_assert(sizeof(chaistl::span<int, 5>) == sizeof(int*));
static_assert(sizeof(chaistl::span<int, 0>) == sizeof(int*));

// Dynamic-extent span should be pointer + size_t
static_assert(sizeof(chaistl::span<int>) == sizeof(int*) + sizeof(std::size_t));
static_assert(sizeof(chaistl::span<double>) == sizeof(double*) + sizeof(std::size_t));

// ======================================================================
// Construction from C arrays
// ======================================================================

TEST(SpanTest, ConstructFromCArray) {
  int arr[] = {1, 2, 3, 4, 5};

  chaistl::span<int> dynamic_span(arr);
  EXPECT_EQ(dynamic_span.size(), 5);
  EXPECT_EQ(dynamic_span.data(), arr);

  chaistl::span<int, 5> static_span(arr);
  EXPECT_EQ(static_span.size(), 5);
  EXPECT_EQ(static_span.data(), arr);
}

TEST(SpanTest, ConstructFromConstCArray) {
  const int arr[] = {10, 20, 30};

  chaistl::span<const int> dynamic_span(arr);
  EXPECT_EQ(dynamic_span.size(), 3);

  chaistl::span<const int, 3> static_span(arr);
  EXPECT_EQ(static_span.size(), 3);
}

// ======================================================================
// Construction from chaistl::array
// ======================================================================

TEST(SpanTest, ConstructFromChaistlArray) {
  chaistl::array<int, 4> arr = {10, 20, 30, 40};

  chaistl::span<int> dynamic_span(arr);
  EXPECT_EQ(dynamic_span.size(), 4);
  EXPECT_EQ(dynamic_span.data(), arr.data());

  chaistl::span<int, 4> static_span(arr);
  EXPECT_EQ(static_span.size(), 4);
}

TEST(SpanTest, ConstructFromConstChaistlArray) {
  const chaistl::array<int, 3> arr = {1, 2, 3};

  chaistl::span<const int> dynamic_span(arr);
  EXPECT_EQ(dynamic_span.size(), 3);

  chaistl::span<const int, 3> static_span(arr);
  EXPECT_EQ(static_span.size(), 3);
}

// ======================================================================
// Construction from std::array
// ======================================================================

TEST(SpanTest, ConstructFromStdArray) {
  std::array<int, 4> arr = {10, 20, 30, 40};

  chaistl::span<int> dynamic_span(arr);
  EXPECT_EQ(dynamic_span.size(), 4);
  EXPECT_EQ(dynamic_span.data(), arr.data());

  chaistl::span<int, 4> static_span(arr);
  EXPECT_EQ(static_span.size(), 4);
}

// ======================================================================
// Construction from chaistl::vector
// ======================================================================

TEST(SpanTest, ConstructFromChaistlVector) {
  chaistl::vector<int> vec = {1, 2, 3, 4, 5};

  chaistl::span<int> dynamic_span(vec);
  EXPECT_EQ(dynamic_span.size(), 5);
  EXPECT_EQ(dynamic_span.data(), vec.data());
}

TEST(SpanTest, ConstructFromConstChaistlVector) {
  const chaistl::vector<int> vec = {1, 2, 3};

  chaistl::span<const int> dynamic_span(vec);
  EXPECT_EQ(dynamic_span.size(), 3);
}

// ======================================================================
// Construction from std::vector
// ======================================================================

TEST(SpanTest, ConstructFromStdVector) {
  std::vector<int> vec = {1, 2, 3, 4, 5};

  chaistl::span<int> dynamic_span(vec);
  EXPECT_EQ(dynamic_span.size(), 5);
  EXPECT_EQ(dynamic_span.data(), vec.data());
}

// ======================================================================
// Construction from iterator pair
// ======================================================================

TEST(SpanTest, ConstructFromIterators) {
  int arr[] = {1, 2, 3, 4, 5};

  chaistl::span<int> dynamic_span(arr, 5);
  EXPECT_EQ(dynamic_span.size(), 5);

  chaistl::span<int, 5> static_span(arr, 5);
  EXPECT_EQ(static_span.size(), 5);
}

TEST(SpanTest, ConstructFromIteratorSentinel) {
  int arr[] = {1, 2, 3, 4, 5};

  chaistl::span<int> dynamic_span(arr, arr + 5);
  EXPECT_EQ(dynamic_span.size(), 5);

  chaistl::span<int, 5> static_span(arr, arr + 5);
  EXPECT_EQ(static_span.size(), 5);
}

// ======================================================================
// Construction from string
// ======================================================================

TEST(SpanTest, ConstructFromString) {
  std::string str = "hello";

  chaistl::span<char> char_span(str);
  EXPECT_EQ(char_span.size(), 5);
  EXPECT_EQ(char_span.data(), str.data());
}

TEST(SpanTest, ConstructFromConstString) {
  const std::string str = "world";

  chaistl::span<const char> char_span(str);
  EXPECT_EQ(char_span.size(), 5);
}

// ======================================================================
// Default construction
// ======================================================================

TEST(SpanTest, DefaultConstructDynamic) {
  chaistl::span<int> s;
  EXPECT_EQ(s.size(), 0);
  EXPECT_TRUE(s.empty());
}

TEST(SpanTest, DefaultConstructStaticZero) {
  chaistl::span<int, 0> s;
  EXPECT_EQ(s.size(), 0);
  EXPECT_TRUE(s.empty());
}

// ======================================================================
// Copy semantics
// ======================================================================

TEST(SpanTest, CopyConstruct) {
  int arr[] = {1, 2, 3};
  chaistl::span<int> s1(arr);
  chaistl::span<int> s2 = s1;

  EXPECT_EQ(s2.size(), s1.size());
  EXPECT_EQ(s2.data(), s1.data());
}

TEST(SpanTest, CopyAssign) {
  int arr1[] = {1, 2, 3};
  int arr2[] = {4, 5, 6, 7};
  chaistl::span<int> s1(arr1);
  chaistl::span<int> s2(arr2);

  s2 = s1;
  EXPECT_EQ(s2.size(), 3);
  EXPECT_EQ(s2.data(), arr1);
}

// ======================================================================
// Cross-type conversion
// ======================================================================

TEST(SpanTest, ConvertToConstSpan) {
  int arr[] = {1, 2, 3};
  chaistl::span<int> mutable_span(arr);
  chaistl::span<const int> const_span = mutable_span;

  EXPECT_EQ(const_span.size(), 3);
  EXPECT_EQ(const_span.data(), arr);
}

TEST(SpanTest, ConvertStaticToDynamic) {
  int arr[] = {1, 2, 3};
  chaistl::span<int, 3> static_span(arr);
  chaistl::span<int> dynamic_span = static_span;

  EXPECT_EQ(dynamic_span.size(), 3);
  EXPECT_EQ(dynamic_span.data(), arr);
}

// ======================================================================
// Element access
// ======================================================================

TEST(SpanTest, SubscriptOperator) {
  int arr[] = {10, 20, 30, 40, 50};
  chaistl::span<int> s(arr);

  EXPECT_EQ(s[0], 10);
  EXPECT_EQ(s[2], 30);
  EXPECT_EQ(s[4], 50);

  s[1] = 99;
  EXPECT_EQ(arr[1], 99);
}

// P2821R5 (C++26): span::at() — bounds-checked access.
TEST(SpanTest, AtValidIndex) {
  int arr[] = {10, 20, 30, 40, 50};
  chaistl::span<int> s(arr);

  EXPECT_EQ(s.at(0), 10);
  EXPECT_EQ(s.at(2), 30);
  EXPECT_EQ(s.at(4), 50);

  s.at(1) = 99;
  EXPECT_EQ(arr[1], 99);
}

TEST(SpanTest, AtThrowsOnOutOfRange) {
  int arr[] = {1, 2, 3};
  chaistl::span<int> s(arr);

  EXPECT_THROW(static_cast<void>(s.at(3)), std::out_of_range);
  EXPECT_THROW(static_cast<void>(s.at(100)), std::out_of_range);
}

TEST(SpanTest, AtOnEmptySpan) {
  chaistl::span<int> s;
  EXPECT_THROW(static_cast<void>(s.at(0)), std::out_of_range);
}

TEST(SpanTest, AtOnStaticExtentSpan) {
  int arr[] = {1, 2, 3, 4, 5};
  chaistl::span<int, 5> s(arr);

  EXPECT_EQ(s.at(0), 1);
  EXPECT_EQ(s.at(4), 5);
  EXPECT_THROW(static_cast<void>(s.at(5)), std::out_of_range);
}

TEST(SpanTest, Front) {
  int arr[] = {42, 2, 3};
  chaistl::span<int> s(arr);

  EXPECT_EQ(s.front(), 42);
}

TEST(SpanTest, Back) {
  int arr[] = {1, 2, 99};
  chaistl::span<int> s(arr);

  EXPECT_EQ(s.back(), 99);
}

TEST(SpanTest, Data) {
  int arr[] = {1, 2, 3};
  chaistl::span<int> s(arr);

  EXPECT_EQ(s.data(), arr);
}

// ======================================================================
// Iterators
// ======================================================================

TEST(SpanTest, Iterators) {
  int arr[] = {10, 20, 30, 40};
  chaistl::span<int> s(arr);

  auto it = s.begin();
  EXPECT_EQ(*it, 10);
  ++it;
  EXPECT_EQ(*it, 20);

  EXPECT_EQ(s.end() - s.begin(), 4);
}

TEST(SpanTest, RangeBasedFor) {
  int arr[] = {1, 2, 3, 4, 5};
  chaistl::span<int> s(arr);

  int sum = 0;
  for (int x : s) {
    sum += x;
  }
  EXPECT_EQ(sum, 15);
}

TEST(SpanTest, ReverseIterators) {
  int arr[] = {1, 2, 3, 4, 5};
  chaistl::span<int> s(arr);

  auto rit = s.rbegin();
  EXPECT_EQ(*rit, 5);
  ++rit;
  EXPECT_EQ(*rit, 4);
}

// ======================================================================
// Subviews
// ======================================================================

TEST(SpanTest, FirstTemplate) {
  int arr[] = {1, 2, 3, 4, 5};
  chaistl::span<int> s(arr);

  auto first3 = s.first<3>();
  static_assert(first3.extent == 3);
  EXPECT_EQ(first3.size(), 3);
  EXPECT_EQ(first3[0], 1);
  EXPECT_EQ(first3[2], 3);
}

TEST(SpanTest, FirstRuntime) {
  int arr[] = {1, 2, 3, 4, 5};
  chaistl::span<int> s(arr);

  auto first2 = s.first(2);
  static_assert(first2.extent == chaistl::dynamic_extent);
  EXPECT_EQ(first2.size(), 2);
  EXPECT_EQ(first2[1], 2);
}

TEST(SpanTest, LastTemplate) {
  int arr[] = {1, 2, 3, 4, 5};
  chaistl::span<int> s(arr);

  auto last3 = s.last<3>();
  static_assert(last3.extent == 3);
  EXPECT_EQ(last3.size(), 3);
  EXPECT_EQ(last3[0], 3);
  EXPECT_EQ(last3[2], 5);
}

TEST(SpanTest, LastRuntime) {
  int arr[] = {1, 2, 3, 4, 5};
  chaistl::span<int> s(arr);

  auto last2 = s.last(2);
  EXPECT_EQ(last2.size(), 2);
  EXPECT_EQ(last2[0], 4);
  EXPECT_EQ(last2[1], 5);
}

TEST(SpanTest, SubspanTemplate) {
  int arr[] = {1, 2, 3, 4, 5};
  chaistl::span<int> s(arr);

  auto sub = s.subspan<1, 3>();
  static_assert(sub.extent == 3);
  EXPECT_EQ(sub.size(), 3);
  EXPECT_EQ(sub[0], 2);
  EXPECT_EQ(sub[2], 4);
}

TEST(SpanTest, SubspanTemplateDynamicCount) {
  int arr[] = {1, 2, 3, 4, 5};
  chaistl::span<int> s(arr);

  auto sub = s.subspan<2>();
  static_assert(sub.extent == chaistl::dynamic_extent);
  EXPECT_EQ(sub.size(), 3);
  EXPECT_EQ(sub[0], 3);
}

TEST(SpanTest, SubspanRuntime) {
  int arr[] = {1, 2, 3, 4, 5};
  chaistl::span<int> s(arr);

  auto sub = s.subspan(1, 2);
  EXPECT_EQ(sub.size(), 2);
  EXPECT_EQ(sub[0], 2);
  EXPECT_EQ(sub[1], 3);
}

TEST(SpanTest, SubspanRuntimeDefaultCount) {
  int arr[] = {1, 2, 3, 4, 5};
  chaistl::span<int> s(arr);

  auto sub = s.subspan(2);
  EXPECT_EQ(sub.size(), 3);
  EXPECT_EQ(sub[0], 3);
  EXPECT_EQ(sub[2], 5);
}

// ======================================================================
// Observers
// ======================================================================

TEST(SpanTest, Size) {
  int arr[] = {1, 2, 3, 4, 5};
  chaistl::span<int> s(arr);

  EXPECT_EQ(s.size(), 5);
}

TEST(SpanTest, SizeBytes) {
  int arr[] = {1, 2, 3};
  chaistl::span<int> s(arr);

  EXPECT_EQ(s.size_bytes(), 3 * sizeof(int));
}

TEST(SpanTest, Empty) {
  chaistl::span<int> empty_span;
  EXPECT_TRUE(empty_span.empty());

  int arr[] = {1};
  chaistl::span<int> non_empty(arr);
  EXPECT_FALSE(non_empty.empty());
}

// ======================================================================
// as_bytes / as_writable_bytes
// ======================================================================

TEST(SpanTest, AsBytes) {
  int arr[] = {0x12345678, 0x0ABCDEF0};
  chaistl::span<int> s(arr);

  auto bytes = chaistl::as_bytes(s);
  EXPECT_EQ(bytes.size(), 2 * sizeof(int));
  EXPECT_EQ(bytes.size_bytes(), 2 * sizeof(int));
}

TEST(SpanTest, AsWritableBytes) {
  int arr[] = {1, 2, 3};
  chaistl::span<int> s(arr);

  auto bytes = chaistl::as_writable_bytes(s);
  EXPECT_EQ(bytes.size(), 3 * sizeof(int));
}

TEST(SpanTest, AsBytesStaticExtent) {
  int arr[4] = {};
  chaistl::span<int, 4> s(arr);

  auto bytes = chaistl::as_bytes(s);
  static_assert(bytes.extent == 4 * sizeof(int));
  EXPECT_EQ(bytes.size(), 4 * sizeof(int));
}

// ======================================================================
// Comparison operators
// ======================================================================

TEST(SpanTest, Equality) {
  int arr1[] = {1, 2, 3};
  int arr2[] = {1, 2, 3};
  int arr3[] = {1, 2, 4};

  chaistl::span<int> s1(arr1);
  chaistl::span<int> s2(arr2);
  chaistl::span<int> s3(arr3);

  EXPECT_TRUE(s1 == s2);
  EXPECT_FALSE(s1 == s3);
}

TEST(SpanTest, ThreeWayCompare) {
  int arr1[] = {1, 2, 3};
  int arr2[] = {1, 2, 4};
  int arr3[] = {1, 2};

  chaistl::span<int> s1(arr1);
  chaistl::span<int> s2(arr2);
  chaistl::span<int> s3(arr3);

  EXPECT_TRUE((s1 <=> s2) < 0);
  EXPECT_TRUE((s2 <=> s1) > 0);
  EXPECT_TRUE((s1 <=> s3) > 0);
  EXPECT_TRUE((s1 <=> s1) == 0);
}

// ======================================================================
// Deduction guides
// ======================================================================

TEST(SpanTest, DeductionFromCArray) {
  int arr[] = {1, 2, 3};
  chaistl::span s(arr);

  static_assert(std::same_as<decltype(s), chaistl::span<int, 3>>);
  EXPECT_EQ(s.size(), 3);
}

TEST(SpanTest, DeductionFromChaistlArray) {
  chaistl::array<int, 4> arr = {1, 2, 3, 4};
  chaistl::span s(arr);

  static_assert(std::same_as<decltype(s), chaistl::span<int, 4>>);
}

TEST(SpanTest, DeductionFromConstChaistlArray) {
  const chaistl::array<int, 3> arr = {1, 2, 3};
  chaistl::span s(arr);

  static_assert(std::same_as<decltype(s), chaistl::span<const int, 3>>);
}

TEST(SpanTest, DeductionFromVector) {
  chaistl::vector<int> vec = {1, 2, 3};
  chaistl::span s(vec);

  static_assert(std::same_as<decltype(s), chaistl::span<int>>);
  EXPECT_EQ(s.size(), 3);
}

TEST(SpanTest, DeductionFromIterators) {
  int arr[] = {1, 2, 3, 4, 5};
  chaistl::span s(arr, 3);

  static_assert(std::same_as<decltype(s), chaistl::span<int>>);
  EXPECT_EQ(s.size(), 3);
}

// ======================================================================
// Edge cases
// ======================================================================

TEST(SpanTest, EmptySpan) {
  chaistl::span<int> empty;
  EXPECT_EQ(empty.size(), 0);
  EXPECT_TRUE(empty.empty());
  EXPECT_EQ(empty.begin(), empty.end());
}

TEST(SpanTest, SingleElement) {
  int x = 42;
  chaistl::span<int> s(&x, 1);

  EXPECT_EQ(s.size(), 1);
  EXPECT_EQ(s.front(), 42);
  EXPECT_EQ(s.back(), 42);
  EXPECT_EQ(s[0], 42);
}

TEST(SpanTest, ConstElementType) {
  const int arr[] = {1, 2, 3};
  chaistl::span<const int> s(arr);

  EXPECT_EQ(s[0], 1);
  // s[0] = 10;  // Should not compile
}

// ======================================================================
// Static extent propagation through subviews
// ======================================================================

TEST(SpanTest, StaticExtentPropagation) {
  int arr[] = {1, 2, 3, 4, 5};
  chaistl::span<int, 5> s(arr);

  auto first3 = s.first<3>();
  static_assert(first3.extent == 3);

  auto last2 = s.last<2>();
  static_assert(last2.extent == 2);

  auto sub = s.subspan<1, 3>();
  static_assert(sub.extent == 3);
}

// ======================================================================
// Swap
// ======================================================================

TEST(SpanTest, Swap) {
  int arr1[] = {1, 2, 3};
  int arr2[] = {4, 5, 6, 7};

  chaistl::span<int> s1(arr1);
  chaistl::span<int> s2(arr2);

  chaistl::swap(s1, s2);

  EXPECT_EQ(s1.size(), 4);
  EXPECT_EQ(s1.data(), arr2);
  EXPECT_EQ(s2.size(), 3);
  EXPECT_EQ(s2.data(), arr1);
}

}  // namespace
