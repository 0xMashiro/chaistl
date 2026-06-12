// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/deque.hpp>
#include <chaistl/containers/vector.hpp>
#include <chaistl/memory_resource.hpp>

#include <cstddef>
#include <memory>
#include <new>
#include <string>
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

}  // namespace

static_assert(std::same_as<chaistl::pmr::polymorphic_allocator<>::value_type, std::byte>);
static_assert(std::same_as<std::allocator_traits<chaistl::pmr::polymorphic_allocator<int>>::pointer, int*>);
static_assert(std::same_as<std::allocator_traits<chaistl::pmr::polymorphic_allocator<int>>::rebind_alloc<long>,
                           chaistl::pmr::polymorphic_allocator<long>>);

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
