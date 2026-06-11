// SPDX-License-Identifier: Apache-2.0

// References:
// - libc++ __memory/uninitialized_algorithms.h
// - libstdc++ bits/stl_uninitialized.h allocator-aware _a helpers
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/memory/detail/lifetime/allocator_uninitialized.hpp>

#include <memory>
#include <stdexcept>
#include <utility>

namespace {

struct tracked {
  int value{};

  static inline int alive = 0;
  static inline int copies_before_throw = -1;

  tracked() : value(0) { ++alive; }

  explicit tracked(int init) : value(init) { ++alive; }

  tracked(const tracked& other) : value(other.value) {
    if (copies_before_throw == 0) {
      throw std::runtime_error("copy failed");
    }
    if (copies_before_throw > 0) {
      --copies_before_throw;
    }
    ++alive;
  }

  tracked(tracked&& other) noexcept : value(other.value) {
    other.value = -1;
    ++alive;
  }

  ~tracked() { --alive; }

  static void reset() {
    alive = 0;
    copies_before_throw = -1;
  }
};

struct trivially_copyable_nontrivial_default {
  int value;

  trivially_copyable_nontrivial_default() : value(42) {}
};

static_assert(std::is_trivially_copyable_v<trivially_copyable_nontrivial_default>);
static_assert(!std::is_trivially_default_constructible_v<trivially_copyable_nontrivial_default>);

template <class T>
struct counting_allocator {
  using value_type = T;

  static inline int constructs = 0;
  static inline int destroys = 0;

  counting_allocator() = default;

  template <class U>
  counting_allocator(const counting_allocator<U>&) {}

  [[nodiscard]] T* allocate(std::size_t count) { return std::allocator<T>{}.allocate(count); }

  void deallocate(T* pointer, std::size_t count) { std::allocator<T>{}.deallocate(pointer, count); }

  template <class... Args>
  void construct(T* pointer, Args&&... args) {
    ++constructs;
    std::construct_at(pointer, std::forward<Args>(args)...);
  }

  void destroy(T* pointer) {
    ++destroys;
    std::destroy_at(pointer);
  }

  static void reset() {
    constructs = 0;
    destroys = 0;
  }
};

template <class T, class U>
bool operator==(const counting_allocator<T>&, const counting_allocator<U>&) {
  return true;
}

}  // namespace

TEST(UninitializedAllocatorAlgorithmsTest, CopyConstructsThroughAllocator) {
  tracked::reset();
  counting_allocator<tracked>::reset();
  counting_allocator<tracked> allocator;

  tracked source[] = {tracked(1), tracked(2), tracked(3)};
  tracked* storage = allocator.allocate(3);

  tracked* last = chaistl::detail::uninitialized_allocator_copy(allocator, source, source + 3, storage);

  EXPECT_EQ(last, storage + 3);
  EXPECT_EQ(counting_allocator<tracked>::constructs, 3);
  EXPECT_EQ(storage[0].value, 1);
  EXPECT_EQ(storage[1].value, 2);
  EXPECT_EQ(storage[2].value, 3);

  chaistl::detail::allocator_destroy_forward(allocator, storage, last);
  EXPECT_EQ(counting_allocator<tracked>::destroys, 3);
  allocator.deallocate(storage, 3);
}

TEST(UninitializedAllocatorAlgorithmsTest, CopyRollsBackConstructedElementsWhenConstructionThrows) {
  tracked::reset();
  counting_allocator<tracked>::reset();
  counting_allocator<tracked> allocator;

  tracked source[] = {tracked(1), tracked(2), tracked(3)};
  tracked* storage = allocator.allocate(3);
  tracked::copies_before_throw = 1;

  EXPECT_THROW((void)chaistl::detail::uninitialized_allocator_copy(allocator, source, source + 3, storage),
               std::runtime_error);
  EXPECT_EQ(counting_allocator<tracked>::constructs, 2);
  EXPECT_EQ(counting_allocator<tracked>::destroys, 1);

  allocator.deallocate(storage, 3);
}

TEST(UninitializedAllocatorAlgorithmsTest, FillAndDefaultConstructReturnPastTheConstructedRange) {
  tracked::reset();
  counting_allocator<tracked>::reset();
  counting_allocator<tracked> allocator;

  tracked* storage = allocator.allocate(4);
  tracked value(42);

  tracked* middle = chaistl::detail::uninitialized_allocator_fill_n(allocator, storage, 2, value);
  tracked* last = chaistl::detail::uninitialized_allocator_default_construct_n(allocator, middle, 2);

  EXPECT_EQ(middle, storage + 2);
  EXPECT_EQ(last, storage + 4);
  EXPECT_EQ(counting_allocator<tracked>::constructs, 4);
  EXPECT_EQ(storage[0].value, 42);
  EXPECT_EQ(storage[1].value, 42);
  EXPECT_EQ(storage[2].value, 0);
  EXPECT_EQ(storage[3].value, 0);

  chaistl::detail::allocator_destroy_reverse(allocator, storage, last);
  EXPECT_EQ(counting_allocator<tracked>::destroys, 4);
  allocator.deallocate(storage, 4);
}

TEST(UninitializedAllocatorAlgorithmsTest, NonTrivialDefaultConstructorDoesNotUseMemset) {
  using value_type = trivially_copyable_nontrivial_default;

  counting_allocator<value_type>::reset();
  counting_allocator<value_type> allocator;
  value_type* storage = allocator.allocate(2);

  value_type* last = chaistl::detail::uninitialized_allocator_default_construct_n(allocator, storage, 2);

  EXPECT_EQ(last, storage + 2);
  EXPECT_EQ(counting_allocator<value_type>::constructs, 2);
  EXPECT_EQ(storage[0].value, 42);
  EXPECT_EQ(storage[1].value, 42);

  chaistl::detail::allocator_destroy_forward(allocator, storage, last);
  allocator.deallocate(storage, 2);
}

TEST(UninitializedAllocatorAlgorithmsTest, TrivialCopyStillConstructsThroughCustomAllocator) {
  counting_allocator<int>::reset();
  counting_allocator<int> allocator;

  int source[] = {1, 2, 3};
  int* storage = allocator.allocate(3);

  int* last = chaistl::detail::uninitialized_allocator_copy(allocator, source, source + 3, storage);

  EXPECT_EQ(last, storage + 3);
  EXPECT_EQ(counting_allocator<int>::constructs, 3);
  EXPECT_EQ(storage[0], 1);
  EXPECT_EQ(storage[1], 2);
  EXPECT_EQ(storage[2], 3);

  chaistl::detail::allocator_destroy_forward(allocator, storage, last);
  allocator.deallocate(storage, 3);
}

TEST(UninitializedAllocatorAlgorithmsTest, TrivialDefaultConstructionStillConstructsThroughCustomAllocator) {
  counting_allocator<int>::reset();
  counting_allocator<int> allocator;

  int* storage = allocator.allocate(2);

  int* last = chaistl::detail::uninitialized_allocator_default_construct_n(allocator, storage, 2);

  EXPECT_EQ(last, storage + 2);
  EXPECT_EQ(counting_allocator<int>::constructs, 2);
  EXPECT_EQ(storage[0], 0);
  EXPECT_EQ(storage[1], 0);

  chaistl::detail::allocator_destroy_forward(allocator, storage, last);
  allocator.deallocate(storage, 2);
}

TEST(UninitializedAllocatorAlgorithmsTest, MoveIfNoexceptConstructsThroughAllocator) {
  tracked::reset();
  counting_allocator<tracked>::reset();
  counting_allocator<tracked> allocator;

  tracked source[] = {tracked(7), tracked(8)};
  tracked* storage = allocator.allocate(2);

  tracked* last = chaistl::detail::uninitialized_allocator_move_if_noexcept(allocator, source, source + 2, storage);

  EXPECT_EQ(last, storage + 2);
  EXPECT_EQ(counting_allocator<tracked>::constructs, 2);
  EXPECT_EQ(storage[0].value, 7);
  EXPECT_EQ(storage[1].value, 8);

  chaistl::detail::allocator_destroy_forward(allocator, storage, last);
  allocator.deallocate(storage, 2);
}
