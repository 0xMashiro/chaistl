// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace chaistl::test {

struct Counted {
  int value{};

  static inline int move_assignments = 0;

  Counted() = default;
  explicit Counted(int init) : value(init) {}
  Counted(const Counted&) = default;
  Counted(Counted&&) noexcept = default;
  Counted& operator=(const Counted&) = default;
  Counted& operator=(Counted&& other) noexcept {
    value = other.value;
    ++move_assignments;
    return *this;
  }

  friend bool operator==(const Counted&, const Counted&) = default;
};

struct MoveMarksSource {
  int value{};

  MoveMarksSource() = default;
  explicit MoveMarksSource(int init) : value(init) {}
  MoveMarksSource(const MoveMarksSource&) = default;
  MoveMarksSource(MoveMarksSource&& other) noexcept : value(other.value) { other.value = -1; }
  MoveMarksSource& operator=(const MoveMarksSource&) = default;
  MoveMarksSource& operator=(MoveMarksSource&& other) noexcept {
    value = other.value;
    other.value = -1;
    return *this;
  }
};

struct ThrowOnCopy {
  int value{};

  static inline int alive = 0;
  static inline int copies_before_throw = -1;

  ThrowOnCopy() : value(0) { ++alive; }
  explicit ThrowOnCopy(int init) : value(init) { ++alive; }
  ThrowOnCopy(const ThrowOnCopy& other) : value(other.value) {
    if (copies_before_throw == 0) {
      throw std::runtime_error("copy construction failed");
    }
    if (copies_before_throw > 0) {
      --copies_before_throw;
    }
    ++alive;
  }
  ThrowOnCopy(ThrowOnCopy&& other) noexcept : value(other.value) {
    other.value = -1;
    ++alive;
  }
  ThrowOnCopy& operator=(const ThrowOnCopy&) = default;
  ThrowOnCopy& operator=(ThrowOnCopy&& other) noexcept {
    value = other.value;
    other.value = -1;
    return *this;
  }
  ~ThrowOnCopy() { --alive; }

  static void reset() {
    alive = 0;
    copies_before_throw = -1;
  }
};

struct ThrowOnMoveOnly {
  int value{};

  static inline int alive = 0;
  static inline int moves_before_throw = -1;

  ThrowOnMoveOnly() { ++alive; }
  explicit ThrowOnMoveOnly(int init) : value(init) { ++alive; }
  ThrowOnMoveOnly(const ThrowOnMoveOnly&) = delete;
  ThrowOnMoveOnly& operator=(const ThrowOnMoveOnly&) = delete;
  ThrowOnMoveOnly(ThrowOnMoveOnly&& other) : value(other.value) {
    if (moves_before_throw == 0) {
      throw std::runtime_error("move construction failed");
    }
    if (moves_before_throw > 0) {
      --moves_before_throw;
    }
    other.value = -1;
    ++alive;
  }
  ThrowOnMoveOnly& operator=(ThrowOnMoveOnly&& other) noexcept {
    value = other.value;
    other.value = -1;
    return *this;
  }
  ~ThrowOnMoveOnly() { --alive; }

  static void reset() {
    alive = 0;
    moves_before_throw = -1;
  }
};

struct ThrowOnDefault {
  int value{};

  static inline int alive = 0;
  static inline int defaults_before_throw = -1;

  ThrowOnDefault() {
    if (defaults_before_throw == 0) {
      throw std::runtime_error("default construction failed");
    }
    if (defaults_before_throw > 0) {
      --defaults_before_throw;
    }
    ++alive;
  }
  explicit ThrowOnDefault(int init) : value(init) { ++alive; }
  ThrowOnDefault(const ThrowOnDefault& other) : value(other.value) { ++alive; }
  ThrowOnDefault(ThrowOnDefault&& other) noexcept : value(other.value) {
    other.value = -1;
    ++alive;
  }
  ThrowOnDefault& operator=(const ThrowOnDefault&) = default;
  ThrowOnDefault& operator=(ThrowOnDefault&& other) noexcept {
    value = other.value;
    other.value = -1;
    return *this;
  }
  ~ThrowOnDefault() { --alive; }

  static void reset() {
    alive = 0;
    defaults_before_throw = -1;
  }
};

struct ThrowOnValue {
  int value{};

  static inline int alive = 0;
  static inline int throwing_value = -1;

  ThrowOnValue() { ++alive; }
  explicit ThrowOnValue(int init) : value(init) {
    if (init == throwing_value) {
      throw std::runtime_error("value construction failed");
    }
    ++alive;
  }
  ThrowOnValue(const ThrowOnValue& other) : value(other.value) { ++alive; }
  ThrowOnValue(ThrowOnValue&& other) noexcept : value(other.value) {
    other.value = -1;
    ++alive;
  }
  ThrowOnValue& operator=(const ThrowOnValue&) = default;
  ThrowOnValue& operator=(ThrowOnValue&& other) noexcept {
    value = other.value;
    other.value = -1;
    return *this;
  }
  ~ThrowOnValue() { --alive; }

  static void reset() {
    alive = 0;
    throwing_value = -1;
  }
};

template <class T>
struct TaggedAllocator {
  using value_type = T;
  using propagate_on_container_copy_assignment = std::false_type;
  using propagate_on_container_move_assignment = std::false_type;
  using propagate_on_container_swap = std::false_type;
  using is_always_equal = std::false_type;

  int id{};

  TaggedAllocator() = default;
  explicit TaggedAllocator(int init) : id(init) {}
  template <class U>
  TaggedAllocator(const TaggedAllocator<U>& other) : id(other.id) {}

  [[nodiscard]] T* allocate(std::size_t count) { return std::allocator<T>{}.allocate(count); }
  void deallocate(T* pointer, std::size_t count) noexcept { std::allocator<T>{}.deallocate(pointer, count); }

  template <class U>
  [[nodiscard]] bool operator==(const TaggedAllocator<U>& other) const noexcept {
    return id == other.id;
  }
};

template <class T>
struct CompatibleTaggedAllocator {
  using value_type = T;
  using propagate_on_container_copy_assignment = std::false_type;
  using propagate_on_container_move_assignment = std::false_type;
  using propagate_on_container_swap = std::false_type;
  using is_always_equal = std::false_type;

  int id{};

  CompatibleTaggedAllocator() = default;
  explicit CompatibleTaggedAllocator(int init) : id(init) {}
  template <class U>
  CompatibleTaggedAllocator(const CompatibleTaggedAllocator<U>& other) : id(other.id) {}

  [[nodiscard]] T* allocate(std::size_t count) { return std::allocator<T>{}.allocate(count); }
  void deallocate(T* pointer, std::size_t count) noexcept { std::allocator<T>{}.deallocate(pointer, count); }

  template <class U>
  [[nodiscard]] bool operator==(const CompatibleTaggedAllocator<U>&) const noexcept {
    return true;
  }
};

template <class T>
struct PropagatingAllocator {
  using value_type = T;
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;
  using is_always_equal = std::false_type;

  int id{};

  PropagatingAllocator() = default;
  explicit PropagatingAllocator(int init) : id(init) {}
  template <class U>
  PropagatingAllocator(const PropagatingAllocator<U>& other) : id(other.id) {}

  [[nodiscard]] T* allocate(std::size_t count) { return std::allocator<T>{}.allocate(count); }
  void deallocate(T* pointer, std::size_t count) noexcept { std::allocator<T>{}.deallocate(pointer, count); }

  template <class U>
  [[nodiscard]] bool operator==(const PropagatingAllocator<U>& other) const noexcept {
    return id == other.id;
  }
};

template <class T>
struct SmallMaxAllocator {
  using value_type = T;

  SmallMaxAllocator() = default;
  template <class U>
  SmallMaxAllocator(const SmallMaxAllocator<U>&) {}

  [[nodiscard]] T* allocate(std::size_t count) { return std::allocator<T>{}.allocate(count); }
  void deallocate(T* pointer, std::size_t count) noexcept { std::allocator<T>{}.deallocate(pointer, count); }
  [[nodiscard]] constexpr std::size_t max_size() const noexcept { return 3; }

  template <class U>
  [[nodiscard]] bool operator==(const SmallMaxAllocator<U>&) const noexcept {
    return true;
  }
};

template <class T>
struct ThrowOnAllocateAllocator {
  using value_type = T;

  static inline bool throw_on_allocate = false;

  ThrowOnAllocateAllocator() = default;
  template <class U>
  ThrowOnAllocateAllocator(const ThrowOnAllocateAllocator<U>&) {}

  [[nodiscard]] T* allocate(std::size_t count) {
    if (throw_on_allocate) {
      throw std::runtime_error("allocation failed");
    }
    return std::allocator<T>{}.allocate(count);
  }
  void deallocate(T* pointer, std::size_t count) noexcept { std::allocator<T>{}.deallocate(pointer, count); }

  static void reset() { throw_on_allocate = false; }

  template <class U>
  [[nodiscard]] bool operator==(const ThrowOnAllocateAllocator<U>&) const noexcept {
    return true;
  }
};

}  // namespace chaistl::test
