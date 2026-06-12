// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/deque.hpp>
#include <chaistl/containers/flat_map.hpp>
#include <chaistl/containers/flat_multimap.hpp>
#include <chaistl/containers/flat_multiset.hpp>
#include <chaistl/containers/flat_set.hpp>
#include <chaistl/containers/forward_list.hpp>
#include <chaistl/containers/list.hpp>
#include <chaistl/containers/map.hpp>
#include <chaistl/containers/multimap.hpp>
#include <chaistl/containers/multiset.hpp>
#include <chaistl/containers/set.hpp>
#include <chaistl/containers/unordered_map.hpp>
#include <chaistl/containers/unordered_multimap.hpp>
#include <chaistl/containers/unordered_multiset.hpp>
#include <chaistl/containers/unordered_set.hpp>
#include <chaistl/containers/vector.hpp>
#include <chaistl/memory_resource.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <vector>

namespace {

class counting_resource final : public chaistl::pmr::memory_resource {
 public:
  int allocations = 0;
  int deallocations = 0;
  std::size_t last_bytes = 0;
  std::size_t last_alignment = 0;

 private:
  void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    ++allocations;
    last_bytes = bytes;
    last_alignment = alignment;
    if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
      return ::operator new(bytes, std::align_val_t{alignment});
    }
    return ::operator new(bytes);
  }

  void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
    ++deallocations;
    last_bytes = bytes;
    last_alignment = alignment;
    if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__) {
      ::operator delete(pointer, bytes, std::align_val_t{alignment});
      return;
    }
    ::operator delete(pointer, bytes);
  }

  bool do_is_equal(const chaistl::pmr::memory_resource& other) const noexcept override { return this == &other; }
};

struct uses_allocator_value {
  using allocator_type = chaistl::pmr::polymorphic_allocator<std::byte>;

  chaistl::pmr::memory_resource* resource = nullptr;
  std::string value;

  uses_allocator_value(std::allocator_arg_t, allocator_type alloc, std::string init)
      : resource(alloc.resource()), value(std::move(init)) {}
};

struct alignas(64) over_aligned_value {
  int value = 0;
};

[[nodiscard]] bool is_aligned(void* pointer, std::size_t alignment) noexcept {
  return reinterpret_cast<std::uintptr_t>(pointer) % alignment == 0;
}

}  // namespace

static_assert(std::same_as<chaistl::pmr::polymorphic_allocator<>::value_type, std::byte>);
static_assert(std::same_as<std::allocator_traits<chaistl::pmr::polymorphic_allocator<int>>::pointer, int*>);
static_assert(std::same_as<std::allocator_traits<chaistl::pmr::polymorphic_allocator<int>>::rebind_alloc<long>,
                           chaistl::pmr::polymorphic_allocator<long>>);
static_assert(std::same_as<chaistl::pmr::list<int>::allocator_type, chaistl::pmr::polymorphic_allocator<int>>);
static_assert(std::same_as<chaistl::pmr::forward_list<int>::allocator_type, chaistl::pmr::polymorphic_allocator<int>>);
static_assert(std::same_as<chaistl::pmr::map<int, int>::allocator_type,
                           chaistl::pmr::polymorphic_allocator<std::pair<const int, int>>>);
static_assert(std::same_as<chaistl::pmr::set<int>::allocator_type, chaistl::pmr::polymorphic_allocator<int>>);
static_assert(std::same_as<chaistl::pmr::unordered_map<int, int>::allocator_type,
                           chaistl::pmr::polymorphic_allocator<std::pair<const int, int>>>);
static_assert(std::same_as<chaistl::pmr::unordered_set<int>::allocator_type, chaistl::pmr::polymorphic_allocator<int>>);
static_assert(std::same_as<chaistl::pmr::flat_map<int, int>::key_container_type, chaistl::pmr::vector<int>>);
static_assert(std::same_as<chaistl::pmr::flat_map<int, int>::mapped_container_type, chaistl::pmr::vector<int>>);
static_assert(std::same_as<chaistl::pmr::flat_set<int>::container_type, chaistl::pmr::vector<int>>);

TEST(MemoryResourceTest, DefaultResourceStartsAsNewDeleteResource) {
  auto* previous = chaistl::pmr::set_default_resource(nullptr);
  (void)previous;

  EXPECT_EQ(chaistl::pmr::get_default_resource(), chaistl::pmr::new_delete_resource());
}

TEST(MemoryResourceTest, SetDefaultResourceReturnsPreviousResource) {
  counting_resource resource;

  auto* previous = chaistl::pmr::set_default_resource(&resource);
  EXPECT_EQ(chaistl::pmr::get_default_resource(), &resource);

  auto* restored = chaistl::pmr::set_default_resource(previous);
  EXPECT_EQ(restored, &resource);
}

TEST(MemoryResourceTest, NullMemoryResourceThrowsOnAllocate) {
  EXPECT_THROW((void)chaistl::pmr::null_memory_resource()->allocate(1), std::bad_alloc);
}

TEST(MemoryResourceTest, NewDeleteResourceHandlesZeroSizeAndOverAlignedAllocations) {
  void* zero = chaistl::pmr::new_delete_resource()->allocate(0);
  ASSERT_NE(zero, nullptr);
  chaistl::pmr::new_delete_resource()->deallocate(zero, 0);

  void* aligned =
      chaistl::pmr::new_delete_resource()->allocate(sizeof(over_aligned_value), alignof(over_aligned_value));
  EXPECT_TRUE(is_aligned(aligned, alignof(over_aligned_value)));
  chaistl::pmr::new_delete_resource()->deallocate(aligned, sizeof(over_aligned_value), alignof(over_aligned_value));
}

TEST(MemoryResourceTest, PolymorphicAllocatorUsesProvidedResource) {
  counting_resource resource;
  chaistl::pmr::polymorphic_allocator<int> alloc(&resource);

  int* pointer = alloc.allocate(3);
  EXPECT_EQ(resource.allocations, 1);
  EXPECT_EQ(resource.last_bytes, 3uz * sizeof(int));
  EXPECT_EQ(resource.last_alignment, alignof(int));

  alloc.deallocate(pointer, 3);
  EXPECT_EQ(resource.deallocations, 1);
}

TEST(MemoryResourceTest, PolymorphicAllocatorRejectsObjectCountOverflow) {
  chaistl::pmr::polymorphic_allocator<int> alloc;
  const auto too_many = std::numeric_limits<std::size_t>::max() / sizeof(int) + 1;

  EXPECT_THROW((void)alloc.allocate(too_many), std::bad_array_new_length);
  EXPECT_THROW((void)alloc.allocate_object<int>(too_many), std::bad_array_new_length);
}

TEST(MemoryResourceTest, PolymorphicAllocatorSupportsByteAndObjectHelpers) {
  counting_resource resource;
  chaistl::pmr::polymorphic_allocator<> alloc(&resource);

  void* bytes = alloc.allocate_bytes(64, alignof(double));
  EXPECT_EQ(resource.last_bytes, 64uz);
  EXPECT_EQ(resource.last_alignment, alignof(double));
  alloc.deallocate_bytes(bytes, 64, alignof(double));

  auto* value = alloc.new_object<uses_allocator_value>("pmr");
  EXPECT_EQ(value->resource, &resource);
  EXPECT_EQ(value->value, "pmr");
  alloc.delete_object(value);
}

TEST(MemoryResourceTest, PmrContainersUseChaistlPolymorphicAllocator) {
  counting_resource resource;

  chaistl::pmr::vector<int> vector{chaistl::pmr::polymorphic_allocator<int>(&resource)};
  vector.push_back(1);
  vector.push_back(2);

  EXPECT_EQ(vector[0], 1);
  EXPECT_EQ(vector[1], 2);
  EXPECT_GT(resource.allocations, 0);

  chaistl::pmr::deque<int> deque{chaistl::pmr::polymorphic_allocator<int>(&resource)};
  deque.push_back(3);
  deque.push_front(2);

  EXPECT_EQ(deque.front(), 2);
  EXPECT_EQ(deque.back(), 3);
}

TEST(MemoryResourceTest, PmrSequenceAndTreeAliasesUseProvidedResource) {
  counting_resource resource;

  chaistl::pmr::list<int> list{chaistl::pmr::polymorphic_allocator<int>(&resource)};
  list.push_back(1);
  list.push_back(2);
  EXPECT_EQ(list.size(), 2uz);
  EXPECT_EQ(list.get_allocator().resource(), &resource);

  chaistl::pmr::forward_list<int> forward_list{chaistl::pmr::polymorphic_allocator<int>(&resource)};
  forward_list.push_front(3);
  forward_list.push_back(4);
  EXPECT_EQ(forward_list.size(), 2uz);
  EXPECT_EQ(forward_list.get_allocator().resource(), &resource);

  chaistl::pmr::map<int, int> map{chaistl::pmr::polymorphic_allocator<std::pair<const int, int>>(&resource)};
  map.emplace(1, 10);
  EXPECT_EQ(map.at(1), 10);
  EXPECT_EQ(map.get_allocator().resource(), &resource);

  chaistl::pmr::set<int> set{chaistl::pmr::polymorphic_allocator<int>(&resource)};
  set.insert(7);
  EXPECT_TRUE(set.contains(7));
  EXPECT_EQ(set.get_allocator().resource(), &resource);

  chaistl::pmr::multimap<int, int> multimap{chaistl::pmr::polymorphic_allocator<std::pair<const int, int>>(&resource)};
  multimap.emplace(1, 20);
  EXPECT_EQ(multimap.count(1), 1uz);
  EXPECT_EQ(multimap.get_allocator().resource(), &resource);

  chaistl::pmr::multiset<int> multiset{chaistl::pmr::polymorphic_allocator<int>(&resource)};
  multiset.insert(7);
  multiset.insert(7);
  EXPECT_EQ(multiset.count(7), 2uz);
  EXPECT_EQ(multiset.get_allocator().resource(), &resource);
}

TEST(MemoryResourceTest, PmrHashAliasesUseProvidedResource) {
  counting_resource resource;

  chaistl::pmr::unordered_map<int, int> map{chaistl::pmr::polymorphic_allocator<std::pair<const int, int>>(&resource)};
  map.emplace(1, 10);
  EXPECT_EQ(map.at(1), 10);
  EXPECT_EQ(map.get_allocator().resource(), &resource);

  chaistl::pmr::unordered_set<int> set{chaistl::pmr::polymorphic_allocator<int>(&resource)};
  set.insert(7);
  EXPECT_TRUE(set.contains(7));
  EXPECT_EQ(set.get_allocator().resource(), &resource);

  chaistl::pmr::unordered_multimap<int, int> multimap{
      chaistl::pmr::polymorphic_allocator<std::pair<const int, int>>(&resource)};
  multimap.emplace(1, 20);
  EXPECT_EQ(multimap.count(1), 1uz);
  EXPECT_EQ(multimap.get_allocator().resource(), &resource);

  chaistl::pmr::unordered_multiset<int> multiset{chaistl::pmr::polymorphic_allocator<int>(&resource)};
  multiset.insert(7);
  multiset.insert(7);
  EXPECT_EQ(multiset.count(7), 2uz);
  EXPECT_EQ(multiset.get_allocator().resource(), &resource);
}

TEST(MemoryResourceTest, PmrFlatAliasesForwardAllocatorToUnderlyingVectors) {
  counting_resource resource;
  chaistl::pmr::polymorphic_allocator<int> alloc(&resource);

  chaistl::pmr::flat_map<int, int> map{alloc};
  map.emplace(2, 20);
  map.emplace(1, 10);
  EXPECT_EQ(map.at(1), 10);
  EXPECT_EQ(map.keys().get_allocator().resource(), &resource);
  EXPECT_EQ(map.values().get_allocator().resource(), &resource);

  chaistl::pmr::flat_set<int> set{alloc};
  set.insert(2);
  set.insert(1);
  EXPECT_TRUE(set.contains(1));
  EXPECT_EQ(set.keys().get_allocator().resource(), &resource);

  chaistl::pmr::flat_multimap<int, int> multimap{alloc};
  multimap.emplace(1, 10);
  multimap.emplace(1, 20);
  EXPECT_EQ(multimap.count(1), 2uz);
  EXPECT_EQ(multimap.keys().get_allocator().resource(), &resource);
  EXPECT_EQ(multimap.values().get_allocator().resource(), &resource);

  chaistl::pmr::flat_multiset<int> multiset{alloc};
  multiset.insert(1);
  multiset.insert(1);
  EXPECT_EQ(multiset.count(1), 2uz);
  EXPECT_EQ(multiset.keys().get_allocator().resource(), &resource);
}

TEST(MemoryResourceTest, MonotonicBufferResourceUsesInitialBufferBeforeUpstream) {
  alignas(std::max_align_t) std::byte buffer[256];
  chaistl::pmr::monotonic_buffer_resource resource(buffer, sizeof(buffer), chaistl::pmr::null_memory_resource());
  chaistl::pmr::polymorphic_allocator<int> alloc(&resource);

  int* first = alloc.allocate(1);
  int* second = alloc.allocate(1);

  auto* begin = buffer;
  auto* end = buffer + sizeof(buffer);
  EXPECT_GE(reinterpret_cast<std::byte*>(first), begin);
  EXPECT_LT(reinterpret_cast<std::byte*>(first), end);
  EXPECT_GE(reinterpret_cast<std::byte*>(second), begin);
  EXPECT_LT(reinterpret_cast<std::byte*>(second), end);

  alloc.deallocate(first, 1);
  alloc.deallocate(second, 1);
}

TEST(MemoryResourceTest, MonotonicBufferResourceHonorsOverAlignedRequests) {
  alignas(over_aligned_value) std::byte buffer[256];
  chaistl::pmr::monotonic_buffer_resource resource(buffer, sizeof(buffer), chaistl::pmr::null_memory_resource());

  void* pointer = resource.allocate(sizeof(over_aligned_value), alignof(over_aligned_value));

  EXPECT_TRUE(is_aligned(pointer, alignof(over_aligned_value)));
  EXPECT_GE(static_cast<std::byte*>(pointer), buffer);
  EXPECT_LT(static_cast<std::byte*>(pointer), buffer + sizeof(buffer));
}

TEST(MemoryResourceTest, MonotonicBufferResourceGrowsFromUpstreamAndReleaseFreesChunks) {
  counting_resource upstream;
  chaistl::pmr::monotonic_buffer_resource resource(16, &upstream);
  chaistl::pmr::polymorphic_allocator<std::byte> alloc(&resource);

  std::byte* first = alloc.allocate(128);
  std::byte* second = alloc.allocate(256);

  EXPECT_GT(upstream.allocations, 0);
  EXPECT_EQ(upstream.deallocations, 0);

  alloc.deallocate(first, 128);
  alloc.deallocate(second, 256);
  EXPECT_EQ(upstream.deallocations, 0);

  resource.release();
  EXPECT_EQ(upstream.deallocations, upstream.allocations);
}

TEST(MemoryResourceTest, MonotonicBufferResourceReleaseReusesInitialBuffer) {
  alignas(std::max_align_t) std::byte buffer[128];
  chaistl::pmr::monotonic_buffer_resource resource(buffer, sizeof(buffer), chaistl::pmr::null_memory_resource());

  void* before = resource.allocate(16, alignof(std::max_align_t));
  resource.release();
  void* after = resource.allocate(16, alignof(std::max_align_t));

  EXPECT_EQ(before, after);
}

TEST(MemoryResourceTest, MonotonicBufferResourceWorksWithPmrVector) {
  alignas(std::max_align_t) std::byte buffer[1024];
  chaistl::pmr::monotonic_buffer_resource resource(buffer, sizeof(buffer));
  chaistl::pmr::vector<int> values{chaistl::pmr::polymorphic_allocator<int>(&resource)};

  for (int value = 0; value != 32; ++value) {
    values.push_back(value);
  }

  EXPECT_EQ(values.size(), 32uz);
  EXPECT_EQ(values.front(), 0);
  EXPECT_EQ(values.back(), 31);
}

TEST(MemoryResourceTest, UnsynchronizedPoolResourceReusesReturnedBlocks) {
  counting_resource upstream;
  chaistl::pmr::unsynchronized_pool_resource resource({.max_blocks_per_chunk = 8, .largest_required_pool_block = 64},
                                                      &upstream);
  chaistl::pmr::polymorphic_allocator<int> alloc(&resource);

  int* first = alloc.allocate(1);
  alloc.deallocate(first, 1);
  int* second = alloc.allocate(1);

  EXPECT_EQ(first, second);
  alloc.deallocate(second, 1);
}

TEST(MemoryResourceTest, UnsynchronizedPoolResourceHonorsOverAlignedSmallAllocations) {
  counting_resource upstream;
  chaistl::pmr::unsynchronized_pool_resource resource({.max_blocks_per_chunk = 4, .largest_required_pool_block = 128},
                                                      &upstream);

  void* first = resource.allocate(sizeof(over_aligned_value), alignof(over_aligned_value));
  EXPECT_TRUE(is_aligned(first, alignof(over_aligned_value)));

  resource.deallocate(first, sizeof(over_aligned_value), alignof(over_aligned_value));
  void* second = resource.allocate(sizeof(over_aligned_value), alignof(over_aligned_value));
  EXPECT_EQ(first, second);

  resource.deallocate(second, sizeof(over_aligned_value), alignof(over_aligned_value));
}

TEST(MemoryResourceTest, UnsynchronizedPoolResourceReleaseFreesPoolChunks) {
  counting_resource upstream;
  chaistl::pmr::unsynchronized_pool_resource resource({.max_blocks_per_chunk = 4, .largest_required_pool_block = 64},
                                                      &upstream);
  chaistl::pmr::polymorphic_allocator<int> alloc(&resource);
  std::vector<int*> pointers;

  for (int index = 0; index != 16; ++index) {
    pointers.push_back(alloc.allocate(1));
  }

  EXPECT_GT(upstream.allocations, 0);
  EXPECT_EQ(upstream.deallocations, 0);

  for (int* pointer : pointers) {
    alloc.deallocate(pointer, 1);
  }
  EXPECT_EQ(upstream.deallocations, 0);

  resource.release();
  EXPECT_EQ(upstream.deallocations, upstream.allocations);
}

TEST(MemoryResourceTest, UnsynchronizedPoolResourceKeepsChunkOwnershipAcrossPoolInsertions) {
  counting_resource upstream;
  chaistl::pmr::unsynchronized_pool_resource resource({.max_blocks_per_chunk = 2, .largest_required_pool_block = 256},
                                                      &upstream);

  void* large = resource.allocate(128, alignof(std::max_align_t));
  void* small = resource.allocate(1, alignof(char));
  void* medium = resource.allocate(32, alignof(double));

  resource.deallocate(large, 128, alignof(std::max_align_t));
  resource.deallocate(small, 1, alignof(char));
  resource.deallocate(medium, 32, alignof(double));

  EXPECT_GT(upstream.allocations, 1);
  resource.release();
  EXPECT_EQ(upstream.deallocations, upstream.allocations);
}

TEST(MemoryResourceTest, UnsynchronizedPoolResourceSendsOversizedAllocationsDirectlyUpstream) {
  counting_resource upstream;
  chaistl::pmr::unsynchronized_pool_resource resource({.max_blocks_per_chunk = 4, .largest_required_pool_block = 32},
                                                      &upstream);

  void* pointer = resource.allocate(128, alignof(std::max_align_t));

  EXPECT_EQ(upstream.allocations, 1);
  resource.deallocate(pointer, 128, alignof(std::max_align_t));
  EXPECT_EQ(upstream.deallocations, 1);
}

TEST(MemoryResourceTest, UnsynchronizedPoolResourceHonorsOverAlignedOversizedAllocations) {
  counting_resource upstream;
  chaistl::pmr::unsynchronized_pool_resource resource({.max_blocks_per_chunk = 4, .largest_required_pool_block = 32},
                                                      &upstream);

  void* pointer = resource.allocate(sizeof(over_aligned_value), alignof(over_aligned_value));

  EXPECT_TRUE(is_aligned(pointer, alignof(over_aligned_value)));
  EXPECT_EQ(upstream.allocations, 1);
  resource.deallocate(pointer, sizeof(over_aligned_value), alignof(over_aligned_value));
  EXPECT_EQ(upstream.deallocations, 1);
}

TEST(MemoryResourceTest, UnsynchronizedPoolResourceWorksWithPmrVector) {
  chaistl::pmr::unsynchronized_pool_resource resource;
  chaistl::pmr::vector<int> values{chaistl::pmr::polymorphic_allocator<int>(&resource)};

  for (int value = 0; value != 32; ++value) {
    values.push_back(value);
  }

  EXPECT_EQ(values.size(), 32uz);
  EXPECT_EQ(values.front(), 0);
  EXPECT_EQ(values.back(), 31);
}

TEST(MemoryResourceTest, SynchronizedPoolResourceReusesBlocksAndReportsOptions) {
  counting_resource upstream;
  chaistl::pmr::pool_options options{.max_blocks_per_chunk = 4, .largest_required_pool_block = 64};
  chaistl::pmr::synchronized_pool_resource resource(options, &upstream);
  chaistl::pmr::polymorphic_allocator<int> alloc(&resource);

  int* first = alloc.allocate(1);
  alloc.deallocate(first, 1);
  int* second = alloc.allocate(1);

  EXPECT_EQ(first, second);
  EXPECT_EQ(resource.upstream_resource(), &upstream);
  EXPECT_EQ(resource.options().max_blocks_per_chunk, 4uz);
  EXPECT_EQ(resource.options().largest_required_pool_block, 64uz);

  alloc.deallocate(second, 1);
  resource.release();
  EXPECT_EQ(upstream.deallocations, upstream.allocations);
}

TEST(MemoryResourceTest, SynchronizedPoolResourceHandlesConcurrentSmallAllocations) {
  counting_resource upstream;
  chaistl::pmr::synchronized_pool_resource resource({.max_blocks_per_chunk = 16, .largest_required_pool_block = 64},
                                                    &upstream);

  auto worker = [&resource] {
    chaistl::pmr::polymorphic_allocator<int> alloc(&resource);
    std::array<int*, 32> pointers{};
    for (int round = 0; round != 64; ++round) {
      for (int*& pointer : pointers) {
        pointer = alloc.allocate(1);
        *pointer = round;
      }
      for (int* pointer : pointers) {
        alloc.deallocate(pointer, 1);
      }
    }
  };

  std::thread first(worker);
  std::thread second(worker);
  std::thread third(worker);
  std::thread fourth(worker);

  first.join();
  second.join();
  third.join();
  fourth.join();

  EXPECT_GT(upstream.allocations, 0);
  EXPECT_EQ(upstream.deallocations, 0);
  resource.release();
  EXPECT_EQ(upstream.deallocations, upstream.allocations);
}
