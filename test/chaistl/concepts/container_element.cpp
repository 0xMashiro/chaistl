// SPDX-License-Identifier: Apache-2.0

// References:
// - C++ named requirements for container elements:
//   https://en.cppreference.com/w/cpp/named_req/Erasable
//   https://en.cppreference.com/w/cpp/named_req/DefaultInsertable
//   https://en.cppreference.com/w/cpp/named_req/MoveInsertable
//   https://en.cppreference.com/w/cpp/named_req/CopyInsertable
//   https://en.cppreference.com/w/cpp/named_req/EmplaceConstructible
// - The checks are chaistl learning concepts, not copied standard-library
//   implementation tests.

#include <chaistl/concepts.hpp>

#include <memory>
#include <string>
#include <type_traits>

struct no_default {
  no_default() = delete;
  explicit no_default(int) {}
};

struct move_only_element {
  move_only_element() = default;
  move_only_element(const move_only_element&) = delete;
  move_only_element(move_only_element&&) = default;
};

struct no_destructor {
  ~no_destructor() = delete;
};

template <class T>
struct matching_allocator {
  using value_type = T;

  [[nodiscard]] T* allocate(std::size_t count) { return std::allocator<T>{}.allocate(count); }

  void deallocate(T* pointer, std::size_t count) noexcept { std::allocator<T>{}.deallocate(pointer, count); }
};

static_assert(chaistl::concepts::allocator_value_for<std::allocator<int>, int>);
static_assert(!chaistl::concepts::allocator_value_for<std::allocator<long>, int>);

static_assert(chaistl::concepts::container_element<int>);
static_assert(chaistl::concepts::container_element<int[2]>);
static_assert(!chaistl::concepts::container_element<void>);
static_assert(!chaistl::concepts::container_element<int&>);
static_assert(!chaistl::concepts::container_element<int()>);
static_assert(!chaistl::concepts::container_element<no_destructor>);

static_assert(chaistl::concepts::erasable<int>);
static_assert(chaistl::concepts::erasable<std::string>);
static_assert(!chaistl::concepts::erasable<no_destructor>);

static_assert(chaistl::concepts::default_insertable<int>);
static_assert(chaistl::concepts::default_insertable<std::string>);
static_assert(!chaistl::concepts::default_insertable<no_default>);

static_assert(chaistl::concepts::emplace_constructible<no_default, std::allocator<no_default>, int>);
static_assert(!chaistl::concepts::emplace_constructible<no_default, std::allocator<no_default>>);

static_assert(chaistl::concepts::move_insertable<int>);
static_assert(chaistl::concepts::move_insertable<move_only_element>);
static_assert(!chaistl::concepts::copy_insertable<move_only_element>);
static_assert(chaistl::concepts::copy_insertable<int>);
static_assert(chaistl::concepts::copy_insertable<std::string>);

static_assert(chaistl::concepts::default_insertable<int, matching_allocator<int>>);
static_assert(!chaistl::concepts::default_insertable<int, matching_allocator<long>>);
