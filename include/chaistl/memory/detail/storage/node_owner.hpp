// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <utility>

namespace chaistl::detail {

/**
 * @brief Owns raw storage for a single allocator-backed node.
 *
 * This helper intentionally manages allocation only. Node-based containers
 * often need to start the node object's lifetime separately from the stored
 * value's lifetime, matching libc++/libstdc++ list/tree implementations.
 * `node_owner` handles the deallocation rollback while the container decides
 * which subobjects have been constructed.
 */
template <class NodeAllocator>
class node_owner {
 public:
  using allocator_type = NodeAllocator;
  using allocator_traits = std::allocator_traits<allocator_type>;
  using pointer = typename allocator_traits::pointer;
  using node_type = typename allocator_traits::value_type;

  explicit constexpr node_owner(allocator_type& alloc)
      : allocator_(std::addressof(alloc)), pointer_(allocator_traits::allocate(alloc, 1)) {}

  node_owner(const node_owner&) = delete;
  node_owner& operator=(const node_owner&) = delete;

  constexpr node_owner(node_owner&& other) noexcept
      : allocator_(other.allocator_), pointer_(std::exchange(other.pointer_, pointer{})) {}

  constexpr node_owner& operator=(node_owner&& other) noexcept {
    if (this != std::addressof(other)) {
      reset();
      allocator_ = other.allocator_;
      pointer_ = std::exchange(other.pointer_, pointer{});
    }
    return *this;
  }

  constexpr ~node_owner() { reset(); }

  [[nodiscard]] constexpr pointer allocator_pointer() const noexcept { return pointer_; }

  [[nodiscard]] constexpr node_type* get() const noexcept { return std::to_address(pointer_); }

  [[nodiscard]] constexpr node_type* release() noexcept {
    node_type* raw = get();
    pointer_ = pointer{};
    return raw;
  }

  constexpr void reset() noexcept {
    if (pointer_ != pointer{}) {
      allocator_traits::deallocate(*allocator_, pointer_, 1);
      pointer_ = pointer{};
    }
  }

 private:
  allocator_type* allocator_;
  pointer pointer_;
};

}  // namespace chaistl::detail
