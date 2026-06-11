// SPDX-License-Identifier: Apache-2.0

// References:
// - Allocator named requirement:
//   https://en.cppreference.com/w/cpp/named_req/Allocator
// - std::allocator:
//   https://en.cppreference.com/w/cpp/memory/allocator
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/concepts.hpp>
#include <chaistl/memory/allocator.hpp>

#include <memory>
#include <string>
#include <type_traits>

namespace {

struct tracked {
  static inline int alive = 0;

  int value{};

  explicit tracked(int init) : value(init) { ++alive; }

  tracked(const tracked& other) : value(other.value) { ++alive; }

  ~tracked() { --alive; }
};

}  // namespace

static_assert(std::same_as<chaistl::allocator<int>::value_type, int>);
static_assert(std::same_as<std::allocator_traits<chaistl::allocator<int>>::pointer, int*>);
static_assert(std::same_as<std::allocator_traits<chaistl::allocator<int>>::const_pointer, const int*>);
static_assert(std::allocator_traits<chaistl::allocator<int>>::is_always_equal::value);
static_assert(chaistl::concepts::allocator_value_for<chaistl::allocator<int>, int>);
static_assert(chaistl::concepts::allocator_for<chaistl::allocator<int>, int>);
static_assert(chaistl::concepts::allocator_for<std::allocator<std::string>, std::string>);
static_assert(!chaistl::concepts::allocator_for<std::allocator<int>, const int>);
static_assert(!chaistl::concepts::allocator_for<std::allocator<int>, volatile int>);
static_assert(!chaistl::concepts::allocator_for<std::allocator<int>, int&>);
static_assert(!chaistl::concepts::allocator_for<std::allocator<int>, int()>);
static_assert(chaistl::concepts::erasable<int, chaistl::allocator<int>>);
static_assert(chaistl::concepts::default_insertable<int, chaistl::allocator<int>>);
static_assert(chaistl::concepts::move_insertable<std::string, chaistl::allocator<std::string>>);
static_assert(chaistl::concepts::copy_insertable<std::string, chaistl::allocator<std::string>>);

TEST(AllocatorTest, AllocatesConstructsDestroysAndDeallocates) {
  tracked::alive = 0;
  chaistl::allocator<tracked> alloc;
  using traits = std::allocator_traits<decltype(alloc)>;

  tracked* storage = traits::allocate(alloc, 2);
  traits::construct(alloc, storage, 11);
  traits::construct(alloc, storage + 1, *storage);

  EXPECT_EQ(tracked::alive, 2);
  EXPECT_EQ(storage[0].value, 11);
  EXPECT_EQ(storage[1].value, 11);

  traits::destroy(alloc, storage + 1);
  traits::destroy(alloc, storage);
  EXPECT_EQ(tracked::alive, 0);

  traits::deallocate(alloc, storage, 2);
}

TEST(AllocatorTest, AllocateAtLeastReturnsRequestedCount) {
  chaistl::allocator<int> alloc;

  auto result = alloc.allocate_at_least(4);

  EXPECT_NE(result.ptr, nullptr);
  EXPECT_GE(result.count, 4uz);

  alloc.deallocate(result.ptr, result.count);
}

TEST(AllocatorTest, AllocatorInstancesCompareEqualAcrossValueTypes) {
  chaistl::allocator<int> ints;
  chaistl::allocator<long> longs;

  EXPECT_TRUE(ints == ints);
  EXPECT_TRUE(ints == longs);
}

TEST(AllocatorTest, HandlesZeroAndNullStorageOperations) {
  chaistl::allocator<int> alloc;

  int* storage = alloc.allocate(0);

  EXPECT_EQ(storage, nullptr);
  alloc.deallocate(storage, 0);
  alloc.deallocate(nullptr, 4);
}

TEST(AllocatorTest, ThrowsWhenRequestedCountExceedsMaxSize) {
  chaistl::allocator<int> alloc;

  EXPECT_THROW((void)alloc.allocate(alloc.max_size() + 1), std::bad_array_new_length);
}
