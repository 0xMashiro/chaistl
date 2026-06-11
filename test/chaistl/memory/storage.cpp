// SPDX-License-Identifier: Apache-2.0

// Tests for chaistl's internal raw-storage construction workflow.
// These verify the exception-safety primitives directly rather than through
// a container that happens to use them.

#include <gtest/gtest.h>

#include <chaistl/memory/detail/lifetime/construction_transaction.hpp>
#include <chaistl/memory/detail/storage/raw_storage_buffer.hpp>
#include <chaistl/memory/detail/storage/uninitialized_storage_builder.hpp>

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

struct storage_tracked {
  int value{};

  static inline int alive = 0;
  static inline int copies_before_throw = -1;
  static inline int throwing_value = -1;

  storage_tracked() { ++alive; }

  explicit storage_tracked(int init) : value(init) {
    if (init == throwing_value) {
      throw std::runtime_error("value construction failed");
    }
    ++alive;
  }

  storage_tracked(const storage_tracked& other) : value(other.value) {
    if (copies_before_throw == 0) {
      throw std::runtime_error("copy construction failed");
    }
    if (copies_before_throw > 0) {
      --copies_before_throw;
    }
    ++alive;
  }

  storage_tracked(storage_tracked&& other) noexcept : value(other.value) {
    other.value = -1;
    ++alive;
  }

  ~storage_tracked() { --alive; }

  static void reset() {
    alive = 0;
    copies_before_throw = -1;
    throwing_value = -1;
  }
};

template <class T>
struct counting_allocator {
  using value_type = T;

  static inline int allocations = 0;
  static inline int deallocations = 0;
  static inline int constructs = 0;
  static inline int destroys = 0;
  static inline std::vector<int> destroy_log{};

  counting_allocator() = default;

  template <class U>
  counting_allocator(const counting_allocator<U>&) {}

  [[nodiscard]] T* allocate(std::size_t count) {
    ++allocations;
    return std::allocator<T>{}.allocate(count);
  }

  void deallocate(T* pointer, std::size_t count) {
    ++deallocations;
    std::allocator<T>{}.deallocate(pointer, count);
  }

  template <class... Args>
  void construct(T* pointer, Args&&... args) {
    ++constructs;
    std::construct_at(pointer, std::forward<Args>(args)...);
  }

  void destroy(T* pointer) {
    ++destroys;
    destroy_log.push_back(pointer->value);
    std::destroy_at(pointer);
  }

  static void reset() {
    allocations = 0;
    deallocations = 0;
    constructs = 0;
    destroys = 0;
    destroy_log.clear();
  }
};

template <class T, class U>
bool operator==(const counting_allocator<T>&, const counting_allocator<U>&) {
  return true;
}

using tracked_allocator = counting_allocator<storage_tracked>;

}  // namespace

TEST(StorageWorkflowTest, RawStorageBufferDeallocatesOwnedStorage) {
  storage_tracked::reset();
  tracked_allocator::reset();
  tracked_allocator allocator;

  {
    chaistl::detail::raw_storage_buffer<storage_tracked, tracked_allocator> storage(allocator, 3);
    EXPECT_NE(storage.data(), nullptr);
    EXPECT_EQ(storage.capacity(), 3);
    EXPECT_EQ(tracked_allocator::allocations, 1);
  }

  EXPECT_EQ(tracked_allocator::deallocations, 1);
  EXPECT_EQ(storage_tracked::alive, 0);
}

TEST(StorageWorkflowTest, RawStorageBufferReleaseTransfersOwnership) {
  storage_tracked::reset();
  tracked_allocator::reset();
  tracked_allocator allocator;
  storage_tracked* released = nullptr;

  {
    chaistl::detail::raw_storage_buffer<storage_tracked, tracked_allocator> storage(allocator, 2);
    released = storage.release();
  }

  EXPECT_NE(released, nullptr);
  EXPECT_EQ(tracked_allocator::deallocations, 0);

  allocator.deallocate(released, 2);
  EXPECT_EQ(tracked_allocator::deallocations, 1);
}

TEST(StorageWorkflowTest, RawStorageBufferZeroCapacityDoesNotAllocate) {
  storage_tracked::reset();
  tracked_allocator::reset();
  tracked_allocator allocator;

  chaistl::detail::raw_storage_buffer<storage_tracked, tracked_allocator> storage(allocator, 0);

  EXPECT_EQ(storage.data(), nullptr);
  EXPECT_EQ(storage.capacity(), 0);
  EXPECT_EQ(tracked_allocator::allocations, 0);
  EXPECT_EQ(tracked_allocator::deallocations, 0);
}

TEST(StorageWorkflowTest, RawStorageBufferMoveTransfersOwnership) {
  storage_tracked::reset();
  tracked_allocator::reset();
  tracked_allocator allocator;

  {
    chaistl::detail::raw_storage_buffer<storage_tracked, tracked_allocator> source(allocator, 2);
    storage_tracked* original = source.data();

    chaistl::detail::raw_storage_buffer<storage_tracked, tracked_allocator> moved(std::move(source));

    EXPECT_EQ(source.data(), nullptr);
    EXPECT_EQ(source.capacity(), 0);
    EXPECT_EQ(moved.data(), original);
    EXPECT_EQ(moved.capacity(), 2);
  }

  EXPECT_EQ(tracked_allocator::deallocations, 1);
}

TEST(StorageWorkflowTest, RawStorageBufferMoveAssignmentReleasesPreviousStorage) {
  storage_tracked::reset();
  tracked_allocator::reset();
  tracked_allocator allocator;

  {
    chaistl::detail::raw_storage_buffer<storage_tracked, tracked_allocator> source(allocator, 3);
    chaistl::detail::raw_storage_buffer<storage_tracked, tracked_allocator> target(allocator, 1);
    storage_tracked* source_data = source.data();

    target = std::move(source);

    EXPECT_EQ(source.data(), nullptr);
    EXPECT_EQ(source.capacity(), 0);
    EXPECT_EQ(target.data(), source_data);
    EXPECT_EQ(target.capacity(), 3);
    EXPECT_EQ(tracked_allocator::deallocations, 1);
  }

  EXPECT_EQ(tracked_allocator::deallocations, 2);
}

TEST(StorageWorkflowTest, TransactionDoesNotDestroyWhenConstructAtThrows) {
  storage_tracked::reset();
  tracked_allocator::reset();
  tracked_allocator allocator;
  storage_tracked* storage = allocator.allocate(1);

  {
    chaistl::detail::construction_transaction<tracked_allocator, storage_tracked*, storage_tracked> tx(allocator);
    storage_tracked::throwing_value = 7;
    EXPECT_THROW(tx.construct_at(storage, 7), std::runtime_error);
  }

  EXPECT_EQ(tracked_allocator::constructs, 1);
  EXPECT_EQ(tracked_allocator::destroys, 0);
  EXPECT_EQ(storage_tracked::alive, 0);

  allocator.deallocate(storage, 1);
}

TEST(StorageWorkflowTest, TransactionRollsBackRegisteredRangesInReverseOrder) {
  storage_tracked::reset();
  tracked_allocator::reset();
  tracked_allocator allocator;
  storage_tracked* storage = allocator.allocate(4);

  {
    chaistl::detail::construction_transaction<tracked_allocator, storage_tracked*, storage_tracked> tx(allocator);
    tx.construct_at(storage, 1);
    tx.construct_at(storage + 1, 2);
    tx.construct_at(storage + 3, 30);
  }

  EXPECT_EQ(storage_tracked::alive, 0);
  EXPECT_EQ(tracked_allocator::destroys, 3);
  EXPECT_EQ(tracked_allocator::destroy_log, (std::vector<int>{30, 2, 1}));

  allocator.deallocate(storage, 4);
}

TEST(StorageWorkflowTest, TransactionRollsBackExternallyRegisteredRange) {
  storage_tracked::reset();
  tracked_allocator::reset();
  tracked_allocator allocator;
  storage_tracked* storage = allocator.allocate(2);

  std::allocator_traits<tracked_allocator>::construct(allocator, storage, 11);
  std::allocator_traits<tracked_allocator>::construct(allocator, storage + 1, 22);

  {
    chaistl::detail::construction_transaction<tracked_allocator, storage_tracked*, storage_tracked> tx(allocator);
    tx.register_constructed_range(storage, storage + 2);
  }

  EXPECT_EQ(storage_tracked::alive, 0);
  EXPECT_EQ(tracked_allocator::destroys, 2);
  EXPECT_EQ(tracked_allocator::destroy_log, (std::vector<int>{22, 11}));

  allocator.deallocate(storage, 2);
}

TEST(StorageWorkflowTest, TransactionCompleteDisarmsRollback) {
  storage_tracked::reset();
  tracked_allocator::reset();
  tracked_allocator allocator;
  storage_tracked* storage = allocator.allocate(1);

  {
    chaistl::detail::construction_transaction<tracked_allocator, storage_tracked*, storage_tracked> tx(allocator);
    tx.construct_at(storage, 42);
    tx.complete();
  }

  EXPECT_EQ(storage_tracked::alive, 1);
  EXPECT_EQ(tracked_allocator::destroys, 0);

  chaistl::detail::allocator_destroy_forward(allocator, storage, storage + 1);
  allocator.deallocate(storage, 1);
  EXPECT_EQ(storage_tracked::alive, 0);
}

TEST(StorageWorkflowTest, ConstructedRangeGuardRollsBackAdvancedRange) {
  storage_tracked::reset();
  tracked_allocator::reset();
  tracked_allocator allocator;
  storage_tracked* storage = allocator.allocate(3);
  storage_tracked* current = storage;

  {
    chaistl::detail::constructed_range_guard<tracked_allocator, storage_tracked*> guard(allocator, storage, current);
    std::allocator_traits<tracked_allocator>::construct(allocator, current, 1);
    ++current;
    std::allocator_traits<tracked_allocator>::construct(allocator, current, 2);
    ++current;
  }

  EXPECT_EQ(storage_tracked::alive, 0);
  EXPECT_EQ(current, storage);
  EXPECT_EQ(tracked_allocator::destroys, 2);
  EXPECT_EQ(tracked_allocator::destroy_log, (std::vector<int>{2, 1}));

  allocator.deallocate(storage, 3);
}

TEST(StorageWorkflowTest, ConstructedRangeGuardCompleteDisarmsRollback) {
  storage_tracked::reset();
  tracked_allocator::reset();
  tracked_allocator allocator;
  storage_tracked* storage = allocator.allocate(2);
  storage_tracked* current = storage;

  {
    chaistl::detail::constructed_range_guard<tracked_allocator, storage_tracked*> guard(allocator, storage, current);
    std::allocator_traits<tracked_allocator>::construct(allocator, current, 1);
    ++current;
    guard.complete();
  }

  EXPECT_EQ(storage_tracked::alive, 1);
  EXPECT_EQ(tracked_allocator::destroys, 0);

  chaistl::detail::allocator_destroy_reverse(allocator, storage, current);
  allocator.deallocate(storage, 2);
  EXPECT_EQ(storage_tracked::alive, 0);
}

TEST(StorageWorkflowTest, BuilderRollsBackObjectsAndStorageWhenCopyThrows) {
  storage_tracked::reset();
  tracked_allocator::reset();
  tracked_allocator allocator;

  {
    storage_tracked source[] = {storage_tracked(10), storage_tracked(20)};

    {
      chaistl::detail::uninitialized_storage_builder<storage_tracked, tracked_allocator> builder(allocator, 3);
      builder.construct_at(builder.data(), 1);

      storage_tracked::copies_before_throw = 1;
      EXPECT_THROW((void)builder.uninitialized_copy(source, source + 2, builder.data() + 1), std::runtime_error);
    }

    EXPECT_EQ(storage_tracked::alive, 2);
    EXPECT_EQ(tracked_allocator::destroys, 2);
    EXPECT_EQ(tracked_allocator::deallocations, 1);
    EXPECT_EQ(tracked_allocator::destroy_log, (std::vector<int>{10, 1}));
  }

  EXPECT_EQ(storage_tracked::alive, 0);
}

TEST(StorageWorkflowTest, BuilderReleaseTransfersConstructedStorageToCaller) {
  storage_tracked::reset();
  tracked_allocator::reset();
  tracked_allocator allocator;
  storage_tracked* first = nullptr;
  storage_tracked* last = nullptr;

  {
    chaistl::detail::uninitialized_storage_builder<storage_tracked, tracked_allocator> builder(allocator, 3);
    first = builder.data();
    last = builder.uninitialized_default_construct_n(first, 2);
    first = builder.release();
  }

  EXPECT_EQ(storage_tracked::alive, 2);
  EXPECT_EQ(tracked_allocator::destroys, 0);
  EXPECT_EQ(tracked_allocator::deallocations, 0);

  chaistl::detail::allocator_destroy_reverse(allocator, first, last);
  allocator.deallocate(first, 3);

  EXPECT_EQ(storage_tracked::alive, 0);
  EXPECT_EQ(tracked_allocator::destroys, 2);
  EXPECT_EQ(tracked_allocator::deallocations, 1);
}
