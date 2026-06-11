// SPDX-License-Identifier: Apache-2.0

// References:
// - [unord.req.general]: copy/move assignment and swap carry the hash
//   function, the predicate, and the maximum load factor along with the
//   elements; allocator movement follows POCCA/POCMA/POCS.
// - [container.reqmts]: alloc-extended move with an unequal allocator must
//   move element-wise (nodes cannot change owners).

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_set.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>

namespace {

using ::testing::UnorderedElementsAre;

struct StatefulHash {
  int id = 0;
  std::size_t operator()(int value) const { return std::hash<int>{}(value); }
};

struct StatefulEq {
  int id = 0;
  bool operator()(int lhs, int rhs) const { return lhs == rhs; }
};

// Stateful, non-propagating allocator that compares equal only by id.
template <class T>
struct ArenaAllocator {
  using value_type = T;

  int id = 0;

  ArenaAllocator() = default;
  explicit ArenaAllocator(int identifier) : id(identifier) {}
  template <class U>
  ArenaAllocator(const ArenaAllocator<U>& other) : id(other.id) {}  // NOLINT(google-explicit-constructor)

  T* allocate(std::size_t count) { return std::allocator<T>{}.allocate(count); }
  void deallocate(T* pointer, std::size_t count) { std::allocator<T>{}.deallocate(pointer, count); }

  template <class U>
  bool operator==(const ArenaAllocator<U>& other) const {
    return id == other.id;
  }
};

using arena_set = chaistl::unordered_set<int, std::hash<int>, std::equal_to<int>, ArenaAllocator<int>>;

// ============================================================================
// Hash / Pred state propagation
// ============================================================================

TEST(UnorderedSetStateful, FunctorsTravelOnCopyMoveAndSwap) {
  using set_t = chaistl::unordered_set<int, StatefulHash, StatefulEq>;
  set_t a(0, StatefulHash{1}, StatefulEq{10});
  set_t b(0, StatefulHash{2}, StatefulEq{20});
  a.insert(1);
  b.insert(2);

  set_t copy(a);
  EXPECT_EQ(copy.hash_function().id, 1);
  EXPECT_EQ(copy.key_eq().id, 10);

  copy = b;  // assignment copies hash and predicate too
  EXPECT_EQ(copy.hash_function().id, 2);
  EXPECT_EQ(copy.key_eq().id, 20);

  set_t moved(std::move(copy));
  EXPECT_EQ(moved.hash_function().id, 2);
  EXPECT_EQ(moved.key_eq().id, 20);

  a.swap(b);
  EXPECT_EQ(a.hash_function().id, 2);
  EXPECT_EQ(a.key_eq().id, 20);
  EXPECT_EQ(b.hash_function().id, 1);
  EXPECT_EQ(b.key_eq().id, 10);
  // The elements traveled with their hashers.
  EXPECT_TRUE(a.contains(2));
  EXPECT_TRUE(b.contains(1));
  EXPECT_TRUE(a.validate());
  EXPECT_TRUE(b.validate());
}

TEST(UnorderedSetStateful, MaxLoadFactorTravelsOnCopyAndAssignment) {
  chaistl::unordered_set<int> source;
  source.max_load_factor(0.25f);

  chaistl::unordered_set<int> copy(source);
  EXPECT_EQ(copy.max_load_factor(), 0.25f);

  chaistl::unordered_set<int> assigned;
  assigned = source;
  EXPECT_EQ(assigned.max_load_factor(), 0.25f);
}

// ============================================================================
// Allocator paths
// ============================================================================

TEST(UnorderedSetStateful, CopyConstructorTakesAllocatorFromSOCCC) {
  arena_set source{ArenaAllocator<int>(7)};
  source.insert(1);

  // ArenaAllocator has no select_on_container_copy_construction: the trait
  // default copies the allocator.
  arena_set copy(source);
  EXPECT_EQ(copy.get_allocator().id, 7);
  EXPECT_THAT(copy, UnorderedElementsAre(1));
}

TEST(UnorderedSetStateful, AllocExtendedMoveWithEqualAllocatorStealsNodes) {
  arena_set source{ArenaAllocator<int>(7)};
  source.insert(1);
  source.insert(2);
  const int* stable = &*source.find(1);

  arena_set target(std::move(source), ArenaAllocator<int>(7));

  EXPECT_EQ(target.get_allocator().id, 7);
  EXPECT_EQ(stable, &*target.find(1));  // nodes changed hands, not addresses
  EXPECT_THAT(target, UnorderedElementsAre(1, 2));
  EXPECT_TRUE(target.validate());
}

TEST(UnorderedSetStateful, AllocExtendedMoveWithUnequalAllocatorMovesElements) {
  arena_set source{ArenaAllocator<int>(1)};
  source.insert(1);
  source.insert(2);
  const int* old_address = &*source.find(1);

  arena_set target(std::move(source), ArenaAllocator<int>(2));

  EXPECT_EQ(target.get_allocator().id, 2);
  EXPECT_THAT(target, UnorderedElementsAre(1, 2));
  // Unequal allocators: nodes cannot change owners, so the elements were
  // re-created with the new allocator at new addresses.
  EXPECT_NE(old_address, &*target.find(1));
  EXPECT_TRUE(target.validate());
}

TEST(UnorderedSetStateful, MoveAssignmentWithUnequalNonPropagatingAllocatorMovesElements) {
  arena_set source{ArenaAllocator<int>(1)};
  source.insert(1);
  source.insert(2);
  arena_set target{ArenaAllocator<int>(2)};
  target.insert(9);

  target = std::move(source);

  // POCMA is false: the target keeps its own allocator and rebuilds.
  EXPECT_EQ(target.get_allocator().id, 2);
  EXPECT_THAT(target, UnorderedElementsAre(1, 2));
  EXPECT_FALSE(target.contains(9));
  EXPECT_TRUE(target.validate());
}

TEST(UnorderedSetStateful, MoveAssignmentWithEqualAllocatorSteals) {
  arena_set source{ArenaAllocator<int>(3)};
  source.insert(1);
  const int* stable = &*source.find(1);
  arena_set target{ArenaAllocator<int>(3)};
  target.insert(9);

  target = std::move(source);

  EXPECT_EQ(stable, &*target.find(1));
  EXPECT_THAT(target, UnorderedElementsAre(1));
  EXPECT_TRUE(target.validate());
}

}  // namespace
