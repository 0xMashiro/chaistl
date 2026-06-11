// SPDX-License-Identifier: Apache-2.0

// References:
// - [unord.req.except]:
//   * single-element insert/emplace: if an exception is thrown by anything
//     other than the hash function, the insertion has no effect;
//   * erase: throws nothing unless Hash or Pred throws — and erase(iterator)
//     calls neither in this implementation (cached hash codes);
//   * rehash: no effect unless thrown by hash or comparison.
// - Iterator validity: a failed insert must not have rehashed
//   ([unord.req.general] (N + n) <= z * B; a duplicate insert has n == 0).
//
// The ASan preset turns every one of these tests into a leak check as well.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_set.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>

namespace {

using ::testing::UnorderedElementsAre;

struct InjectedError {};

// Countdown fuse: throws after the configured number of calls. Disarms
// itself on throw so post-failure assertions can use the container freely.
struct Fuse {
  int calls_until_throw = -1;  // negative: disarmed

  void tick() {
    if (calls_until_throw < 0) {
      return;
    }
    if (calls_until_throw == 0) {
      calls_until_throw = -1;
      throw InjectedError{};
    }
    --calls_until_throw;
  }
};

struct ThrowingHash {
  Fuse* fuse = nullptr;
  std::size_t operator()(int value) const {
    fuse->tick();
    return std::hash<int>{}(value);
  }
};

// Maps every key to bucket 0 so KeyEqual is guaranteed to run on insert.
struct CollidingThrowingHash {
  Fuse* fuse = nullptr;
  std::size_t operator()(int) const {
    fuse->tick();
    return 0;
  }
};

struct ThrowingEq {
  Fuse* fuse = nullptr;
  bool operator()(int lhs, int rhs) const {
    fuse->tick();
    return lhs == rhs;
  }
};

// A value whose copy constructor can be armed to throw. Moves never throw
// and are not counted, so test setup stays inert.
struct Pyro {
  int id = 0;
  static int copies_until_throw;  // negative: disarmed

  explicit Pyro(int identifier) : id(identifier) {}
  Pyro(Pyro&&) noexcept = default;
  Pyro& operator=(Pyro&&) noexcept = default;

  Pyro(const Pyro& other) : id(other.id) {
    if (copies_until_throw == 0) {
      copies_until_throw = -1;
      throw InjectedError{};
    }
    if (copies_until_throw > 0) {
      --copies_until_throw;
    }
  }
  Pyro& operator=(const Pyro&) = default;

  bool operator==(const Pyro& other) const { return id == other.id; }
};
int Pyro::copies_until_throw = -1;

struct PyroHash {
  std::size_t operator()(const Pyro& value) const { return std::hash<int>{}(value.id); }
};

// Allocator whose allocate() can be armed to throw.
template <class T>
struct FlakyAllocator {
  using value_type = T;

  Fuse* fuse = nullptr;

  FlakyAllocator() = default;
  explicit FlakyAllocator(Fuse* f) : fuse(f) {}
  template <class U>
  FlakyAllocator(const FlakyAllocator<U>& other) : fuse(other.fuse) {}  // NOLINT(google-explicit-constructor)

  T* allocate(std::size_t count) {
    if (fuse != nullptr) {
      fuse->tick();
    }
    return std::allocator<T>{}.allocate(count);
  }
  void deallocate(T* pointer, std::size_t count) { std::allocator<T>{}.deallocate(pointer, count); }

  template <class U>
  bool operator==(const FlakyAllocator<U>& other) const {
    return fuse == other.fuse;
  }
};

// Grows @p set until the very next unique insert would trigger a rehash.
template <class Set>
void fill_to_rehash_boundary(Set& set, int seed) {
  while (static_cast<float>(set.size() + 1) <= set.max_load_factor() * static_cast<float>(set.bucket_count())) {
    set.insert(typename Set::value_type(seed++));
  }
}

// ============================================================================
// Hash / KeyEqual failure points
// ============================================================================

TEST(UnorderedSetExceptions, HashThrowOnInsertHasNoEffect) {
  Fuse fuse;
  chaistl::unordered_set<int, ThrowingHash> set(0, ThrowingHash{&fuse});
  set.insert(1);
  set.insert(2);
  const auto size_before = set.size();
  const auto buckets_before = set.bucket_count();

  fuse.calls_until_throw = 0;
  EXPECT_THROW(set.insert(3), InjectedError);

  EXPECT_EQ(set.size(), size_before);
  EXPECT_EQ(set.bucket_count(), buckets_before);
  EXPECT_THAT(set, UnorderedElementsAre(1, 2));
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetExceptions, KeyEqualThrowOnInsertHasNoEffect) {
  Fuse hash_fuse;
  Fuse eq_fuse;
  chaistl::unordered_set<int, CollidingThrowingHash, ThrowingEq> set(
      0, CollidingThrowingHash{&hash_fuse}, ThrowingEq{&eq_fuse});
  set.insert(1);
  set.insert(2);
  const auto size_before = set.size();

  eq_fuse.calls_until_throw = 0;
  EXPECT_THROW(set.insert(3), InjectedError);  // every key collides: Eq runs

  EXPECT_EQ(set.size(), size_before);
  EXPECT_THAT(set, UnorderedElementsAre(1, 2));
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetExceptions, HashThrowOnEmplaceDestroysConstructedNode) {
  Fuse fuse;
  chaistl::unordered_set<int, ThrowingHash> set(0, ThrowingHash{&fuse});
  set.insert(1);

  // emplace constructs first; the hash failure must destroy that node
  // (the ASan preset verifies the deallocation).
  fuse.calls_until_throw = 0;
  EXPECT_THROW(set.emplace(2), InjectedError);

  EXPECT_THAT(set, UnorderedElementsAre(1));
  EXPECT_TRUE(set.validate());
}

// ============================================================================
// Value construction failure points
// ============================================================================

TEST(UnorderedSetExceptions, ValueCopyThrowOnInsertHasNoEffect) {
  chaistl::unordered_set<Pyro, PyroHash> set;
  set.insert(Pyro(1));
  const Pyro probe(2);

  Pyro::copies_until_throw = 0;
  EXPECT_THROW(set.insert(probe), InjectedError);
  Pyro::copies_until_throw = -1;

  EXPECT_EQ(set.size(), 1u);
  EXPECT_FALSE(set.contains(probe));
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetExceptions, FailedInsertNeverRehashes) {
  chaistl::unordered_set<Pyro, PyroHash> set;
  for (int i = 0; i < 8; ++i) {
    set.insert(Pyro(i));
  }
  fill_to_rehash_boundary(set, 1000);
  const auto buckets_before = set.bucket_count();
  const auto size_before = set.size();
  const auto walker = set.begin();

  // The next unique insert would rehash — but node construction precedes the
  // rehash, so a throwing copy must leave the bucket array (and with it every
  // iterator) untouched.
  const Pyro doomed(9999);
  Pyro::copies_until_throw = 0;
  EXPECT_THROW(set.insert(doomed), InjectedError);
  Pyro::copies_until_throw = -1;

  EXPECT_EQ(set.size(), size_before);
  EXPECT_EQ(set.bucket_count(), buckets_before);
  std::size_t walked = 0;
  for (auto it = walker; it != set.end(); ++it) {
    ++walked;
  }
  EXPECT_EQ(walked, size_before);
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetExceptions, CopyConstructorFailureLeavesSourceIntactAndLeaksNothing) {
  using pyro_set = chaistl::unordered_set<Pyro, PyroHash>;
  pyro_set source;
  for (int i = 0; i < 6; ++i) {
    source.insert(Pyro(i));
  }

  Pyro::copies_until_throw = 3;
  EXPECT_THROW(pyro_set{source}, InjectedError);
  Pyro::copies_until_throw = -1;

  EXPECT_EQ(source.size(), 6u);
  EXPECT_TRUE(source.validate());
}

// ============================================================================
// Allocation failure points
// ============================================================================

TEST(UnorderedSetExceptions, NodeAllocationFailureHasNoEffect) {
  Fuse fuse;
  chaistl::unordered_set<int, std::hash<int>, std::equal_to<int>, FlakyAllocator<int>> set{FlakyAllocator<int>(&fuse)};
  set.insert(1);

  fuse.calls_until_throw = 0;
  EXPECT_THROW(set.insert(2), InjectedError);

  EXPECT_THAT(set, UnorderedElementsAre(1));
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetExceptions, BucketArrayAllocationFailureDuringGrowthHasNoEffect) {
  Fuse fuse;
  chaistl::unordered_set<int, std::hash<int>, std::equal_to<int>, FlakyAllocator<int>> set{FlakyAllocator<int>(&fuse)};
  set.insert(0);
  fill_to_rehash_boundary(set, 100);
  const auto buckets_before = set.bucket_count();
  const auto size_before = set.size();

  // Next insert: node allocation (1st allocate) succeeds, the grown bucket
  // array (2nd allocate) throws. The guard frees the node; the old table —
  // and every iterator into it — survives.
  fuse.calls_until_throw = 1;
  EXPECT_THROW(set.insert(7777), InjectedError);
  fuse.calls_until_throw = -1;

  EXPECT_EQ(set.size(), size_before);
  EXPECT_EQ(set.bucket_count(), buckets_before);
  EXPECT_FALSE(set.contains(7777));
  EXPECT_TRUE(set.validate());

  // The container still grows fine once allocation recovers.
  set.insert(7777);
  EXPECT_TRUE(set.contains(7777));
  EXPECT_TRUE(set.validate());
}

// ============================================================================
// Erase and duplicate-insert guarantees
// ============================================================================

TEST(UnorderedSetExceptions, EraseAndClearCallNoUserCode) {
  Fuse hash_fuse;
  Fuse eq_fuse;
  chaistl::unordered_set<int, ThrowingHash, ThrowingEq> set(0, ThrowingHash{&hash_fuse}, ThrowingEq{&eq_fuse});
  for (int i = 0; i < 10; ++i) {
    set.insert(i);
  }

  // Arm both functors: any Hash/KeyEqual call would now throw. erase(it)
  // locates the bucket from the cached hash instead.
  hash_fuse.calls_until_throw = 0;
  eq_fuse.calls_until_throw = 0;
  EXPECT_NO_THROW(set.erase(set.begin()));
  EXPECT_EQ(set.size(), 9u);
  EXPECT_NO_THROW(set.clear());
  EXPECT_TRUE(set.empty());
}

TEST(UnorderedSetExceptions, DuplicateInsertNeverRehashes) {
  chaistl::unordered_set<int> set;
  set.insert(0);
  fill_to_rehash_boundary(set, 1);
  const auto buckets_before = set.bucket_count();

  // n == 0 in the (N + n) <= z * B rule: a duplicate may not rehash even at
  // the boundary.
  auto [it, inserted] = set.insert(0);
  EXPECT_FALSE(inserted);
  EXPECT_EQ(set.bucket_count(), buckets_before);
  EXPECT_TRUE(set.validate());
}

}  // namespace
