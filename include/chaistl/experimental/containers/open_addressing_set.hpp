// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// open_addressing_set - experimental open-addressing hash set
// ============================================================================
//
// Architecture:
//   - Stores values directly in a power-of-two bucket array.
//   - Empty buckets terminate lookup; erased buckets become tombstones so probe
//     chains remain searchable.
//   - The probing policy is a template parameter. The bundled policies cover
//     the classic teaching variants: linear probing, quadratic probing, and
//     double hashing.
//
// Standardization archaeology:
//   - The C++ standard specifies unordered associative containers by observable
//     semantics, not by collision strategy. Node-based separate chaining is the
//     usual implementation choice because reference stability is easier.
//   - Open addressing is therefore intentionally exposed here as an
//     experimental chaistl container, where the probing strategy is part of the
//     type and can be studied directly.
//
// References:
//   - C++ Draft unordered associative requirements:
//     https://eel.is/c++draft/unord.req
//   - cppreference unordered associative containers:
//     https://en.cppreference.com/w/cpp/container/unordered_set

#include <chaistl/memory.hpp>
#include <chaistl/utility/hardening.hpp>

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace chaistl::experimental {

struct linear_probing {
  [[nodiscard]] static constexpr std::size_t offset(std::size_t, std::size_t probe, std::size_t) noexcept {
    return probe;
  }
};

struct quadratic_probing {
  [[nodiscard]] static constexpr std::size_t offset(std::size_t, std::size_t probe, std::size_t) noexcept {
    // Triangular increments form a permutation modulo 2^n, so lookup can visit
    // every bucket while keeping the classic quadratic-probing shape.
    return probe * (probe + 1u) / 2u;
  }
};

struct double_hashing {
  [[nodiscard]] static constexpr std::size_t offset(std::size_t hash, std::size_t probe, std::size_t) noexcept {
    constexpr std::size_t half_width = std::numeric_limits<std::size_t>::digits / 2u;
    const std::size_t step = (hash >> half_width) | 1u;
    return probe * step;
  }
};

template <class Key,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class ProbingPolicy = linear_probing,
          class Allocator = chaistl::allocator<Key>>
class open_addressing_set {
  enum class slot_state : unsigned char { empty, occupied, tombstone };

  struct slot {
    slot_state state = slot_state::empty;
    std::size_t hash = 0;
    std::optional<Key> value;
  };

  using slot_allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<slot>;
  using slot_vector = std::vector<slot, slot_allocator_type>;

 public:
  using key_type = Key;
  using value_type = Key;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using hasher = Hash;
  using key_equal = KeyEqual;
  using allocator_type = Allocator;
  using reference = const value_type&;
  using const_reference = const value_type&;
  using pointer = const value_type*;
  using const_pointer = const value_type*;

  class const_iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = Key;
    using difference_type = std::ptrdiff_t;
    using pointer = const value_type*;
    using reference = const value_type&;

    constexpr const_iterator() = default;

    [[nodiscard]] constexpr reference operator*() const noexcept { return *table_->slots_[index_].value; }
    [[nodiscard]] constexpr pointer operator->() const noexcept { return std::addressof(**this); }

    constexpr const_iterator& operator++() noexcept {
      ++index_;
      skip_empty();
      return *this;
    }

    constexpr const_iterator operator++(int) noexcept {
      const_iterator copy = *this;
      ++(*this);
      return copy;
    }

    friend constexpr bool operator==(const const_iterator&, const const_iterator&) = default;

   private:
    friend class open_addressing_set;

    constexpr const_iterator(const open_addressing_set* table, size_type index) noexcept
        : table_(table), index_(index) {
      skip_empty();
    }

    constexpr void skip_empty() noexcept {
      if (table_ == nullptr) return;
      while (index_ < table_->slots_.size() && table_->slots_[index_].state != slot_state::occupied) {
        ++index_;
      }
    }

    const open_addressing_set* table_ = nullptr;
    size_type index_ = 0;
  };

  using iterator = const_iterator;

  constexpr open_addressing_set() = default;

  explicit constexpr open_addressing_set(const allocator_type& alloc)
      : allocator_(alloc), slots_(slot_allocator_type{allocator_}) {}

  template <std::input_iterator InputIt>
  constexpr open_addressing_set(InputIt first, InputIt last, const allocator_type& alloc = allocator_type{})
      : open_addressing_set(alloc) {
    if constexpr (std::forward_iterator<InputIt>) {
      reserve(static_cast<size_type>(std::ranges::distance(first, last)));
    }
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  constexpr open_addressing_set(std::initializer_list<value_type> init, const allocator_type& alloc = allocator_type{})
      : open_addressing_set(alloc) {
    reserve(init.size());
    for (const auto& value : init) {
      insert(value);
    }
  }

  [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
  [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
  [[nodiscard]] constexpr size_type bucket_count() const noexcept { return slots_.size(); }
  [[nodiscard]] constexpr allocator_type get_allocator() const { return allocator_; }
  [[nodiscard]] constexpr hasher hash_function() const { return hash_; }
  [[nodiscard]] constexpr key_equal key_eq() const { return equal_; }

  [[nodiscard]] constexpr float load_factor() const noexcept {
    if (bucket_count() == 0) return 0.0f;
    return static_cast<float>(size_) / static_cast<float>(bucket_count());
  }

  [[nodiscard]] constexpr float max_load_factor() const noexcept { return max_load_factor_; }

  constexpr void max_load_factor(float factor) {
    CHAI_HARDENED(factor > 0.0f && factor <= 0.875f,
                  "open_addressing_set::max_load_factor: factor must be in (0, 0.875]");
    max_load_factor_ = factor;
    if (bucket_count() != 0 && load_factor() > max_load_factor_) {
      rehash(required_bucket_count(size_));
    }
  }

  [[nodiscard]] constexpr iterator begin() noexcept { return iterator(this, 0); }
  [[nodiscard]] constexpr const_iterator begin() const noexcept { return const_iterator(this, 0); }
  [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }

  [[nodiscard]] constexpr iterator end() noexcept { return iterator(this, bucket_count()); }
  [[nodiscard]] constexpr const_iterator end() const noexcept { return const_iterator(this, bucket_count()); }
  [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }

  constexpr void clear() noexcept {
    for (slot& bucket : slots_) {
      bucket.value.reset();
      bucket.hash = 0;
      bucket.state = slot_state::empty;
    }
    size_ = 0;
    tombstones_ = 0;
  }

  constexpr std::pair<iterator, bool> insert(const value_type& value) { return insert_value(value); }
  constexpr std::pair<iterator, bool> insert(value_type&& value) { return insert_value(std::move(value)); }

  template <class... Args>
  constexpr std::pair<iterator, bool> emplace(Args&&... args) {
    value_type value(std::forward<Args>(args)...);
    return insert(std::move(value));
  }

  constexpr size_type erase(const key_type& key) {
    const size_type index = find_index(key);
    if (index == npos) return 0;

    slots_[index].value.reset();
    slots_[index].hash = 0;
    slots_[index].state = slot_state::tombstone;
    --size_;
    ++tombstones_;
    return 1;
  }

  [[nodiscard]] constexpr iterator find(const key_type& key) { return iterator(this, find_index(key)); }

  [[nodiscard]] constexpr const_iterator find(const key_type& key) const {
    return const_iterator(this, find_index(key));
  }

  [[nodiscard]] constexpr bool contains(const key_type& key) const { return find_index(key) != npos; }
  [[nodiscard]] constexpr size_type count(const key_type& key) const { return contains(key) ? 1u : 0u; }

  constexpr void reserve(size_type element_count) { rehash(required_bucket_count(element_count)); }

  constexpr void rehash(size_type bucket_count_hint) {
    const size_type target = next_bucket_count(bucket_count_hint);
    if (target == bucket_count() && tombstones_ == 0) return;
    rehash_exact(target);
  }

  [[nodiscard]] constexpr bool validate() const {
    size_type occupied = 0;
    size_type tombstones = 0;

    for (const slot& bucket : slots_) {
      if (bucket.state == slot_state::occupied) {
        if (!bucket.value.has_value() || find_index(*bucket.value) == npos) return false;
        ++occupied;
      } else if (bucket.state == slot_state::tombstone) {
        if (bucket.value.has_value()) return false;
        ++tombstones;
      } else if (bucket.value.has_value()) {
        return false;
      }
    }

    return occupied == size_ && tombstones == tombstones_ && load_factor() <= max_load_factor_;
  }

 private:
  static constexpr size_type min_bucket_count = 8;
  static constexpr size_type npos = static_cast<size_type>(-1);

  template <class Value>
  constexpr std::pair<iterator, bool> insert_value(Value&& value) {
    ensure_insert_capacity();
    const std::size_t hash = hash_(value);

    size_type first_tombstone = npos;
    for (size_type probe = 0; probe < bucket_count(); ++probe) {
      const size_type index = probe_index(hash, probe, bucket_count());
      slot& bucket = slots_[index];

      if (bucket.state == slot_state::occupied) {
        if (bucket.hash == hash && equal_(*bucket.value, value)) {
          return {iterator(this, index), false};
        }
        continue;
      }

      if (bucket.state == slot_state::tombstone) {
        if (first_tombstone == npos) first_tombstone = index;
        continue;
      }

      const size_type target = first_tombstone == npos ? index : first_tombstone;
      place_at(target, hash, std::forward<Value>(value));
      return {iterator(this, target), true};
    }

    rehash_exact(bucket_count() * 2u);
    return insert_value(std::forward<Value>(value));
  }

  constexpr void ensure_insert_capacity() {
    if (bucket_count() == 0) {
      rehash_exact(min_bucket_count);
      return;
    }

    const size_type used = size_ + tombstones_;
    if (exceeds_load(size_ + 1u, bucket_count()) || exceeds_load(used + 1u, bucket_count())) {
      rehash_exact(required_bucket_count(size_ + 1u));
    }
  }

  template <class Value>
  constexpr void place_at(size_type index, std::size_t hash, Value&& value) {
    slot& bucket = slots_[index];
    if (bucket.state == slot_state::tombstone) {
      --tombstones_;
    }
    bucket.hash = hash;
    bucket.value.emplace(std::forward<Value>(value));
    bucket.state = slot_state::occupied;
    ++size_;
  }

  constexpr void rehash_exact(size_type requested_bucket_count) {
    requested_bucket_count = next_bucket_count(requested_bucket_count);
    slot_vector old_slots(std::move(slots_));
    slots_ = slot_vector(requested_bucket_count, slot_allocator_type{allocator_});
    size_ = 0;
    tombstones_ = 0;

    for (slot& bucket : old_slots) {
      if (bucket.state == slot_state::occupied) {
        insert_existing(std::move(*bucket.value), bucket.hash);
      }
    }
  }

  constexpr void insert_existing(value_type&& value, std::size_t hash) {
    for (size_type probe = 0; probe < bucket_count(); ++probe) {
      const size_type index = probe_index(hash, probe, bucket_count());
      slot& bucket = slots_[index];
      if (bucket.state == slot_state::empty) {
        bucket.hash = hash;
        bucket.value.emplace(std::move(value));
        bucket.state = slot_state::occupied;
        ++size_;
        return;
      }
    }
  }

  [[nodiscard]] constexpr size_type find_index(const key_type& key) const {
    if (bucket_count() == 0) return npos;

    const std::size_t hash = hash_(key);
    for (size_type probe = 0; probe < bucket_count(); ++probe) {
      const size_type index = probe_index(hash, probe, bucket_count());
      const slot& bucket = slots_[index];

      if (bucket.state == slot_state::empty) return npos;
      if (bucket.state == slot_state::occupied && bucket.hash == hash && equal_(*bucket.value, key)) {
        return index;
      }
    }
    return npos;
  }

  [[nodiscard]] static constexpr size_type probe_index(std::size_t hash,
                                                       size_type probe,
                                                       size_type bucket_count) noexcept {
    return (hash + ProbingPolicy::offset(hash, probe, bucket_count)) & (bucket_count - 1u);
  }

  [[nodiscard]] constexpr size_type required_bucket_count(size_type element_count) const noexcept {
    size_type required = min_bucket_count;
    while (exceeds_load(element_count, required)) {
      required *= 2u;
    }
    return required;
  }

  [[nodiscard]] constexpr size_type next_bucket_count(size_type requested) const noexcept {
    requested = std::max({requested, min_bucket_count, required_bucket_count(size_)});
    return std::bit_ceil(requested);
  }

  [[nodiscard]] constexpr bool exceeds_load(size_type element_count, size_type buckets) const noexcept {
    return static_cast<float>(element_count) > max_load_factor_ * static_cast<float>(buckets);
  }

  allocator_type allocator_{};
  slot_vector slots_;
  size_type size_ = 0;
  size_type tombstones_ = 0;
  float max_load_factor_ = 0.5f;
  hasher hash_{};
  key_equal equal_{};
};

template <class Key,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = chaistl::allocator<Key>>
using linear_open_addressing_set = open_addressing_set<Key, Hash, KeyEqual, linear_probing, Allocator>;

template <class Key,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = chaistl::allocator<Key>>
using quadratic_open_addressing_set = open_addressing_set<Key, Hash, KeyEqual, quadratic_probing, Allocator>;

template <class Key,
          class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = chaistl::allocator<Key>>
using double_hashing_open_addressing_set = open_addressing_set<Key, Hash, KeyEqual, double_hashing, Allocator>;

}  // namespace chaistl::experimental
