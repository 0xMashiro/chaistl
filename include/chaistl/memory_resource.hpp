// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace chaistl::pmr {

namespace detail {

constexpr bool is_power_of_two(std::size_t value) noexcept {
  return value != 0 && (value & (value - 1)) == 0;
}

constexpr bool is_valid_alignment(std::size_t alignment) noexcept {
  return is_power_of_two(alignment);
}

}  // namespace detail

class memory_resource {
  static constexpr std::size_t max_align = alignof(std::max_align_t);

 public:
  memory_resource() = default;
  memory_resource(const memory_resource&) = default;
  virtual ~memory_resource() = default;

  memory_resource& operator=(const memory_resource&) = default;

  [[nodiscard]] void* allocate(std::size_t bytes, std::size_t alignment = max_align) {
    if (!detail::is_valid_alignment(alignment)) [[unlikely]] {
      throw std::bad_alloc();
    }
    return do_allocate(bytes, alignment);
  }

  void deallocate(void* pointer, std::size_t bytes, std::size_t alignment = max_align) {
    if (!detail::is_valid_alignment(alignment)) [[unlikely]] {
      throw std::bad_alloc();
    }
    do_deallocate(pointer, bytes, alignment);
  }

  [[nodiscard]] bool is_equal(const memory_resource& other) const noexcept { return do_is_equal(other); }

 private:
  virtual void* do_allocate(std::size_t bytes, std::size_t alignment) = 0;
  virtual void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) = 0;
  virtual bool do_is_equal(const memory_resource& other) const noexcept = 0;
};

[[nodiscard]] inline bool operator==(const memory_resource& lhs, const memory_resource& rhs) noexcept {
  return &lhs == &rhs || lhs.is_equal(rhs);
}

[[nodiscard]] inline memory_resource* new_delete_resource() noexcept;

namespace detail {

inline bool is_overaligned(std::size_t alignment) noexcept {
  return alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__;
}

class new_delete_memory_resource final : public memory_resource {
 private:
  void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    if (is_overaligned(alignment)) {
      return ::operator new(bytes, std::align_val_t{alignment});
    }
    return ::operator new(bytes);
  }

  void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
    if (is_overaligned(alignment)) {
      ::operator delete(pointer, bytes, std::align_val_t{alignment});
      return;
    }
    ::operator delete(pointer, bytes);
  }

  bool do_is_equal(const memory_resource& other) const noexcept override { return this == &other; }
};

class null_memory_resource_impl final : public memory_resource {
 private:
  void* do_allocate(std::size_t, std::size_t) override { throw std::bad_alloc(); }
  void do_deallocate(void*, std::size_t, std::size_t) override {}
  bool do_is_equal(const memory_resource& other) const noexcept override { return this == &other; }
};

inline memory_resource* default_resource_storage(memory_resource* replacement = nullptr, bool store = false) noexcept {
  static std::atomic<memory_resource*> current{new_delete_resource()};

  if (store) {
    return current.exchange(replacement ? replacement : new_delete_resource(), std::memory_order_acq_rel);
  }
  return current.load(std::memory_order_acquire);
}

template <class Alloc>
class resource_adaptor_impl final : public memory_resource {
 public:
  using allocator_type = Alloc;
  using traits_type = std::allocator_traits<allocator_type>;

  static_assert(std::same_as<typename traits_type::pointer, typename allocator_type::value_type*>);
  static_assert(std::same_as<typename traits_type::const_pointer, const typename allocator_type::value_type*>);
  static_assert(std::same_as<typename traits_type::void_pointer, void*>);
  static_assert(std::same_as<typename traits_type::const_void_pointer, const void*>);

  resource_adaptor_impl() = default;
  resource_adaptor_impl(const resource_adaptor_impl&) = default;
  resource_adaptor_impl(resource_adaptor_impl&&) = default;

  explicit resource_adaptor_impl(const allocator_type& allocator) : allocator_(allocator) {}
  explicit resource_adaptor_impl(allocator_type&& allocator) : allocator_(std::move(allocator)) {}

  resource_adaptor_impl& operator=(const resource_adaptor_impl&) = default;

  [[nodiscard]] allocator_type get_allocator() const { return allocator_; }

 private:
  [[nodiscard]] static std::size_t allocation_count(std::size_t bytes) noexcept { return bytes == 0 ? 1 : bytes; }

  void* do_allocate(std::size_t bytes, std::size_t) override {
    const std::size_t count = allocation_count(bytes);
    if (count > traits_type::max_size(allocator_)) [[unlikely]] {
      throw std::bad_alloc();
    }
    return traits_type::allocate(allocator_, count);
  }

  void do_deallocate(void* pointer, std::size_t bytes, std::size_t) override {
    traits_type::deallocate(
        allocator_, static_cast<typename allocator_type::value_type*>(pointer), allocation_count(bytes));
  }

  bool do_is_equal(const memory_resource& other) const noexcept override {
    const auto* that = dynamic_cast<const resource_adaptor_impl*>(&other);
    try {
      return that != nullptr && allocator_ == that->allocator_;
    } catch (...) {
      return false;
    }
  }

  allocator_type allocator_{};
};

}  // namespace detail

[[nodiscard]] inline memory_resource* new_delete_resource() noexcept {
  static detail::new_delete_memory_resource instance;
  return &instance;
}

[[nodiscard]] inline memory_resource* null_memory_resource() noexcept {
  static detail::null_memory_resource_impl instance;
  return &instance;
}

[[nodiscard]] inline memory_resource* get_default_resource() noexcept {
  return detail::default_resource_storage();
}

inline memory_resource* set_default_resource(memory_resource* resource) noexcept {
  return detail::default_resource_storage(resource, true);
}

template <class T = std::byte>
class polymorphic_allocator {
 public:
  using value_type = T;

  polymorphic_allocator() noexcept : resource_(get_default_resource()) {}
  polymorphic_allocator(memory_resource* resource) noexcept : resource_(resource) {}
  polymorphic_allocator(const polymorphic_allocator&) = default;

  template <class U>
  polymorphic_allocator(const polymorphic_allocator<U>& other) noexcept : resource_(other.resource()) {}

  polymorphic_allocator& operator=(const polymorphic_allocator&) = delete;

  [[nodiscard]] T* allocate(std::size_t count) {
    if (count > max_object_count<T>()) [[unlikely]] {
      throw std::bad_array_new_length();
    }
    return static_cast<T*>(resource_->allocate(count * sizeof(T), alignof(T)));
  }

  void deallocate(T* pointer, std::size_t count) { resource_->deallocate(pointer, count * sizeof(T), alignof(T)); }

  [[nodiscard]] void* allocate_bytes(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) {
    return resource_->allocate(bytes, alignment);
  }

  void deallocate_bytes(void* pointer, std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) {
    resource_->deallocate(pointer, bytes, alignment);
  }

  template <class U>
  [[nodiscard]] U* allocate_object(std::size_t count = 1) {
    if (count > max_object_count<U>()) [[unlikely]] {
      throw std::bad_array_new_length();
    }
    return static_cast<U*>(allocate_bytes(count * sizeof(U), alignof(U)));
  }

  template <class U>
  void deallocate_object(U* pointer, std::size_t count = 1) {
    deallocate_bytes(pointer, count * sizeof(U), alignof(U));
  }

  template <class U, class... Args>
  [[nodiscard]] U* new_object(Args&&... args) {
    U* pointer = allocate_object<U>();
    try {
      construct(pointer, std::forward<Args>(args)...);
    } catch (...) {
      deallocate_object(pointer);
      throw;
    }
    return pointer;
  }

  template <class U>
  void delete_object(U* pointer) {
    std::destroy_at(pointer);
    deallocate_object(pointer);
  }

  template <class U, class... Args>
  void construct(U* pointer, Args&&... args) {
    std::uninitialized_construct_using_allocator(pointer, *this, std::forward<Args>(args)...);
  }

  template <class U>
  void destroy(U* pointer) {
    std::destroy_at(pointer);
  }

  [[nodiscard]] polymorphic_allocator select_on_container_copy_construction() const noexcept {
    return polymorphic_allocator();
  }

  [[nodiscard]] memory_resource* resource() const noexcept { return resource_; }

 private:
  template <class U>
  static constexpr std::size_t max_object_count() noexcept {
    return std::numeric_limits<std::size_t>::max() / sizeof(U);
  }

  memory_resource* resource_;
};

template <class T1, class T2>
[[nodiscard]] bool operator==(const polymorphic_allocator<T1>& lhs, const polymorphic_allocator<T2>& rhs) noexcept {
  return *lhs.resource() == *rhs.resource();
}

template <class Alloc>
using resource_adaptor =
    detail::resource_adaptor_impl<typename std::allocator_traits<Alloc>::template rebind_alloc<char>>;

struct pool_options {
  std::size_t max_blocks_per_chunk = 0;
  std::size_t largest_required_pool_block = 0;
  bool release_empty_chunks = false;
};

// Groups small allocations by rounded block size. Individual deallocations only
// return blocks to a free list; release() returns whole chunks upstream.
class unsynchronized_pool_resource : public memory_resource {
 public:
  unsynchronized_pool_resource() : unsynchronized_pool_resource(pool_options{}, get_default_resource()) {}

  explicit unsynchronized_pool_resource(memory_resource* upstream)
      : unsynchronized_pool_resource(pool_options{}, upstream) {}

  explicit unsynchronized_pool_resource(const pool_options& options)
      : unsynchronized_pool_resource(options, get_default_resource()) {}

  unsynchronized_pool_resource(const pool_options& options, memory_resource* upstream)
      : options_(normalize_options(options)), upstream_(upstream) {}

  unsynchronized_pool_resource(const unsynchronized_pool_resource&) = delete;
  unsynchronized_pool_resource& operator=(const unsynchronized_pool_resource&) = delete;

  ~unsynchronized_pool_resource() override { release(); }

  void release() noexcept {
    for (auto& current : pools_) {
      current->release(upstream_);
    }
    pools_.clear();

    oversized_header* current = oversized_;
    while (current != nullptr) {
      oversized_header* next = current->next;
      upstream_->deallocate(current->base, current->allocation_size, current->allocation_alignment);
      current = next;
    }
    oversized_ = nullptr;
  }

  [[nodiscard]] memory_resource* upstream_resource() const noexcept { return upstream_; }
  [[nodiscard]] pool_options options() const noexcept { return options_; }

 private:
  struct free_block {
    free_block* next;
  };

  struct pool_chunk {
    pool_chunk* next;
    char* blocks;
    std::size_t allocation_size;
    std::size_t capacity;
    std::size_t next_index;
    std::size_t free_count;
    free_block* free_list;
  };

  struct oversized_header {
    oversized_header* previous;
    oversized_header* next;
    void* base;
    std::size_t allocation_size;
    std::size_t allocation_alignment;
  };

  class pool {
   public:
    explicit pool(std::size_t block_size) noexcept : block_size_(block_size) {}

    pool(pool&& other) = delete;
    pool& operator=(pool&& other) = delete;
    pool(const pool&) = delete;
    pool& operator=(const pool&) = delete;

    [[nodiscard]] std::size_t block_size() const noexcept { return block_size_; }

    void* allocate(unsynchronized_pool_resource& owner) {
      if (unfull_ == nullptr || is_full(unfull_)) {
        unfull_ = find_unfull_chunk();
      }
      if (unfull_ == nullptr) {
        unfull_ = allocate_chunk(owner);
      }

      pool_chunk* chunk = unfull_;
      void* block = nullptr;
      if (chunk->free_list != nullptr) {
        block = chunk->free_list;
        chunk->free_list = chunk->free_list->next;
      } else {
        block = chunk->blocks + chunk->next_index * block_size_;
        ++chunk->next_index;
        *chunk_slot(block) = chunk;
      }

      --chunk->free_count;
      if (is_full(chunk)) {
        unfull_ = find_unfull_chunk();
      }
      return block;
    }

    void deallocate(unsynchronized_pool_resource& owner, void* pointer) noexcept {
      pool_chunk* chunk = *chunk_slot(pointer);
      ++chunk->free_count;
      if (owner.options_.release_empty_chunks && chunk->free_count == chunk->capacity) {
        release_chunk(owner.upstream_, chunk);
        return;
      }

      auto* block = ::new (pointer) free_block{chunk->free_list};
      chunk->free_list = block;
      unfull_ = chunk;
    }

    void release(memory_resource* upstream) noexcept {
      pool_chunk* current = chunks_;
      while (current != nullptr) {
        pool_chunk* next = current->next;
        upstream->deallocate(current, current->allocation_size, block_size_);
        current = next;
      }
      chunks_ = nullptr;
      unfull_ = nullptr;
      next_capacity_ = default_next_capacity;
    }

   private:
    static constexpr std::size_t default_next_capacity = 4;

    [[nodiscard]] bool is_full(const pool_chunk* chunk) const noexcept { return chunk->free_count == 0; }

    // Store the owning chunk in the tail padding of each block. That keeps
    // deallocate(pointer, size) O(1) without a global pointer lookup table.
    [[nodiscard]] pool_chunk** chunk_slot(void* block) const noexcept {
      return reinterpret_cast<pool_chunk**>(static_cast<std::byte*>(block) + block_size_ - sizeof(pool_chunk*));
    }

    [[nodiscard]] pool_chunk* find_unfull_chunk() const noexcept {
      for (pool_chunk* current = chunks_; current != nullptr; current = current->next) {
        if (!is_full(current)) {
          return current;
        }
      }
      return nullptr;
    }

    void release_chunk(memory_resource* upstream, pool_chunk* chunk) noexcept {
      auto** link = &chunks_;
      while (*link != nullptr && *link != chunk) {
        link = &(*link)->next;
      }
      if (*link == nullptr) {
        return;
      }

      *link = chunk->next;
      if (unfull_ == chunk) {
        unfull_ = nullptr;
      }
      upstream->deallocate(chunk, chunk->allocation_size, block_size_);
      if (chunks_ == nullptr) {
        next_capacity_ = default_next_capacity;
      } else if (unfull_ == nullptr) {
        unfull_ = find_unfull_chunk();
      }
    }

    pool_chunk* allocate_chunk(unsynchronized_pool_resource& owner) {
      const std::size_t capacity = owner.next_chunk_capacity(next_capacity_);
      const std::size_t payload_size = capacity * block_size_;

      if (owner.add_overflows(sizeof(pool_chunk), block_size_ - 1) ||
          owner.add_overflows(sizeof(pool_chunk) + block_size_ - 1, payload_size)) {
        throw std::bad_alloc();
      }

      const std::size_t allocation_size = sizeof(pool_chunk) + block_size_ - 1 + payload_size;
      void* allocation = owner.upstream_->allocate(allocation_size, block_size_);
      const auto payload_begin =
          owner.align_up(reinterpret_cast<std::uintptr_t>(allocation) + sizeof(pool_chunk), block_size_);

      auto* chunk = ::new (allocation) pool_chunk{
          chunks_,
          reinterpret_cast<char*>(payload_begin),
          allocation_size,
          capacity,
          0,
          capacity,
          nullptr,
      };

      chunks_ = chunk;
      next_capacity_ = owner.next_chunk_capacity(capacity * 2);
      return chunk;
    }

    std::size_t block_size_;
    std::size_t next_capacity_ = default_next_capacity;
    pool_chunk* chunks_ = nullptr;
    pool_chunk* unfull_ = nullptr;
  };

  static constexpr std::size_t default_max_blocks_per_chunk = 64;
  static constexpr std::size_t default_largest_required_pool_block = 4096;

  static constexpr std::size_t max_size(std::size_t lhs, std::size_t rhs) noexcept { return lhs < rhs ? rhs : lhs; }

  static constexpr bool add_overflows(std::size_t lhs, std::size_t rhs) noexcept {
    return lhs > std::numeric_limits<std::size_t>::max() - rhs;
  }

  static constexpr std::uintptr_t align_up(std::uintptr_t value, std::size_t alignment) noexcept {
    return (value + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
  }

  static constexpr std::size_t bit_ceil(std::size_t value) noexcept {
    std::size_t result = 1;
    while (result < value && result <= std::numeric_limits<std::size_t>::max() / 2) {
      result *= 2;
    }
    return result < value ? std::numeric_limits<std::size_t>::max() : result;
  }

  static constexpr pool_options normalize_options(pool_options options) noexcept {
    if (options.max_blocks_per_chunk == 0) {
      options.max_blocks_per_chunk = default_max_blocks_per_chunk;
    }
    if (options.largest_required_pool_block == 0) {
      options.largest_required_pool_block = default_largest_required_pool_block;
    }

    options.max_blocks_per_chunk = max_size(options.max_blocks_per_chunk, 1);
    options.largest_required_pool_block = bit_ceil(max_size(options.largest_required_pool_block, sizeof(void*)));
    return options;
  }

  std::size_t next_chunk_capacity(std::size_t requested) const noexcept {
    return std::min(max_size(requested, 1), options_.max_blocks_per_chunk);
  }

  static std::size_t block_size_for(std::size_t bytes, std::size_t alignment) {
    if (add_overflows(bytes, sizeof(pool_chunk*))) {
      throw std::bad_alloc();
    }
    const std::size_t min_block_size = sizeof(free_block) + sizeof(pool_chunk*);
    return bit_ceil(max_size(max_size(bytes + sizeof(pool_chunk*), min_block_size), alignment));
  }

  void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    if (bytes <= options_.largest_required_pool_block) {
      return find_or_create_pool(block_size_for(bytes, alignment)).allocate(*this);
    }
    return allocate_oversized(bytes, alignment);
  }

  void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
    if (bytes <= options_.largest_required_pool_block) {
      find_or_create_pool(block_size_for(bytes, alignment)).deallocate(*this, pointer);
      return;
    }
    deallocate_oversized(pointer);
  }

  bool do_is_equal(const memory_resource& other) const noexcept override { return this == &other; }

  pool& find_or_create_pool(std::size_t block_size) {
    auto position =
        std::lower_bound(pools_.begin(), pools_.end(), block_size, [](const auto& current, std::size_t size) {
          return current->block_size() < size;
        });
    if (position == pools_.end() || (*position)->block_size() != block_size) {
      position = pools_.emplace(position, std::make_unique<pool>(block_size));
    }
    return **position;
  }

  void* allocate_oversized(std::size_t bytes, std::size_t alignment) {
    const std::size_t allocation_alignment = max_size(alignof(oversized_header), alignment);
    if (add_overflows(sizeof(oversized_header), allocation_alignment - 1) ||
        add_overflows(sizeof(oversized_header) + allocation_alignment - 1, bytes)) {
      throw std::bad_alloc();
    }

    const std::size_t allocation_size = sizeof(oversized_header) + allocation_alignment - 1 + bytes;
    void* base = upstream_->allocate(allocation_size, allocation_alignment);
    const auto result_address =
        align_up(reinterpret_cast<std::uintptr_t>(base) + sizeof(oversized_header), allocation_alignment);
    auto* header = reinterpret_cast<oversized_header*>(result_address - sizeof(oversized_header));
    ::new (header) oversized_header{nullptr, oversized_, base, allocation_size, allocation_alignment};
    if (oversized_ != nullptr) {
      oversized_->previous = header;
    }
    oversized_ = header;
    return reinterpret_cast<void*>(result_address);
  }

  void deallocate_oversized(void* pointer) noexcept {
    auto* header = reinterpret_cast<oversized_header*>(static_cast<std::byte*>(pointer) - sizeof(oversized_header));
    if (header->previous != nullptr) {
      header->previous->next = header->next;
    } else {
      oversized_ = header->next;
    }
    if (header->next != nullptr) {
      header->next->previous = header->previous;
    }
    upstream_->deallocate(header->base, header->allocation_size, header->allocation_alignment);
  }

  pool_options options_;
  memory_resource* upstream_;
  std::vector<std::unique_ptr<pool>> pools_;
  oversized_header* oversized_ = nullptr;
};

// Thread-safe facade over the same size-class pool strategy. The mutex keeps
// allocation and deallocation simple and explicit for teaching; it is not a
// per-thread-cache allocator.
class synchronized_pool_resource : public memory_resource {
 public:
  synchronized_pool_resource() : synchronized_pool_resource(pool_options{}, get_default_resource()) {}

  explicit synchronized_pool_resource(memory_resource* upstream)
      : synchronized_pool_resource(pool_options{}, upstream) {}

  explicit synchronized_pool_resource(const pool_options& options)
      : synchronized_pool_resource(options, get_default_resource()) {}

  synchronized_pool_resource(const pool_options& options, memory_resource* upstream) : inner_(options, upstream) {}

  synchronized_pool_resource(const synchronized_pool_resource&) = delete;
  synchronized_pool_resource& operator=(const synchronized_pool_resource&) = delete;

  void release() {
    const std::lock_guard lock(mutex_);
    inner_.release();
  }

  [[nodiscard]] memory_resource* upstream_resource() const noexcept { return inner_.upstream_resource(); }

  [[nodiscard]] pool_options options() const noexcept { return inner_.options(); }

 private:
  void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    const std::lock_guard lock(mutex_);
    return inner_.allocate(bytes, alignment);
  }

  void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override {
    const std::lock_guard lock(mutex_);
    inner_.deallocate(pointer, bytes, alignment);
  }

  bool do_is_equal(const memory_resource& other) const noexcept override { return this == &other; }

  mutable std::mutex mutex_;
  unsynchronized_pool_resource inner_;
};

// Bump-pointer resource: allocation advances through the current buffer,
// deallocation is a no-op, and release() drops all upstream chunks at once.
class monotonic_buffer_resource : public memory_resource {
 public:
  explicit monotonic_buffer_resource(memory_resource* upstream) noexcept : upstream_(upstream) {}

  monotonic_buffer_resource(std::size_t initial_size, memory_resource* upstream) noexcept
      : next_buffer_size_(round_buffer_size(initial_size)), upstream_(upstream) {}

  monotonic_buffer_resource(void* buffer, std::size_t buffer_size, memory_resource* upstream) noexcept
      : initial_buffer_(buffer),
        initial_buffer_size_(buffer_size),
        current_buffer_(buffer),
        space_available_(buffer_size),
        next_buffer_size_(buffer_size == 0 ? min_buffer_size : scale_buffer_size(buffer_size)),
        upstream_(upstream) {}

  monotonic_buffer_resource() noexcept : monotonic_buffer_resource(get_default_resource()) {}

  explicit monotonic_buffer_resource(std::size_t initial_size) noexcept
      : monotonic_buffer_resource(initial_size, get_default_resource()) {}

  monotonic_buffer_resource(void* buffer, std::size_t buffer_size) noexcept
      : monotonic_buffer_resource(buffer, buffer_size, get_default_resource()) {}

  monotonic_buffer_resource(const monotonic_buffer_resource&) = delete;
  monotonic_buffer_resource& operator=(const monotonic_buffer_resource&) = delete;

  ~monotonic_buffer_resource() override { release(); }

  void release() noexcept {
    auto* chunk = chunks_;
    while (chunk != nullptr) {
      auto* next = chunk->next;
      upstream_->deallocate(chunk, chunk->allocation_size, chunk->allocation_alignment);
      chunk = next;
    }

    chunks_ = nullptr;
    current_buffer_ = initial_buffer_;
    space_available_ = initial_buffer_size_;
    next_buffer_size_ = initial_buffer_size_ == 0 ? min_buffer_size : scale_buffer_size(initial_buffer_size_);
  }

  [[nodiscard]] memory_resource* upstream_resource() const noexcept { return upstream_; }

 private:
  struct chunk_header {
    chunk_header* next;
    std::size_t allocation_size;
    std::size_t allocation_alignment;
  };

  static constexpr std::size_t min_buffer_size = 1024;
  static constexpr std::size_t max_buffer_size = std::numeric_limits<std::size_t>::max() / 2;

  static constexpr std::size_t max_size(std::size_t lhs, std::size_t rhs) noexcept { return lhs < rhs ? rhs : lhs; }

  static constexpr std::size_t round_buffer_size(std::size_t size) noexcept { return max_size(size, min_buffer_size); }

  static constexpr std::size_t scale_buffer_size(std::size_t size) noexcept {
    if (size >= max_buffer_size) {
      return std::numeric_limits<std::size_t>::max();
    }
    return max_size(size * 2, min_buffer_size);
  }

  static constexpr bool add_overflows(std::size_t lhs, std::size_t rhs) noexcept {
    return lhs > std::numeric_limits<std::size_t>::max() - rhs;
  }

  static constexpr std::uintptr_t align_up(std::uintptr_t value, std::size_t alignment) noexcept {
    return (value + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
  }

  void* do_allocate(std::size_t bytes, std::size_t alignment) override {
    if (void* result = allocate_from_current_buffer(bytes, alignment)) {
      return result;
    }
    return allocate_from_new_buffer(bytes, alignment);
  }

  void do_deallocate(void*, std::size_t, std::size_t) override {}

  bool do_is_equal(const memory_resource& other) const noexcept override { return this == &other; }

  void* allocate_from_current_buffer(std::size_t bytes, std::size_t alignment) noexcept {
    const auto current = reinterpret_cast<std::uintptr_t>(current_buffer_);
    const auto aligned = align_up(current, alignment);
    const std::size_t padding = aligned - current;
    if (padding > space_available_ || bytes > space_available_ - padding) {
      return nullptr;
    }

    current_buffer_ = reinterpret_cast<std::byte*>(aligned) + bytes;
    space_available_ -= padding + bytes;
    return reinterpret_cast<void*>(aligned);
  }

  void* allocate_from_new_buffer(std::size_t bytes, std::size_t alignment) {
    const std::size_t payload_size = max_size(bytes, next_buffer_size_);
    const std::size_t allocation_alignment = max_size(alignof(chunk_header), alignment);

    if (add_overflows(sizeof(chunk_header), allocation_alignment - 1) ||
        add_overflows(sizeof(chunk_header) + allocation_alignment - 1, payload_size)) {
      throw std::bad_alloc();
    }

    const std::size_t allocation_size = sizeof(chunk_header) + allocation_alignment - 1 + payload_size;
    void* allocation = upstream_->allocate(allocation_size, allocation_alignment);
    auto* chunk = ::new (allocation) chunk_header{chunks_, allocation_size, allocation_alignment};
    chunks_ = chunk;

    current_buffer_ = static_cast<std::byte*>(allocation) + sizeof(chunk_header);
    space_available_ = allocation_size - sizeof(chunk_header);
    next_buffer_size_ = scale_buffer_size(payload_size);

    void* result = allocate_from_current_buffer(bytes, alignment);
    if (result == nullptr) {
      throw std::bad_alloc();
    }
    return result;
  }

  void* initial_buffer_ = nullptr;
  std::size_t initial_buffer_size_ = 0;
  void* current_buffer_ = nullptr;
  std::size_t space_available_ = 0;
  std::size_t next_buffer_size_ = min_buffer_size;
  memory_resource* upstream_ = get_default_resource();
  chunk_header* chunks_ = nullptr;
};

}  // namespace chaistl::pmr
