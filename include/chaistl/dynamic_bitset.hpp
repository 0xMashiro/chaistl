// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// dynamic_bitset - Runtime-sized bit sequence
// ============================================================================
//
// This is the runtime-sized counterpart of chaistl::bitset. It deliberately
// avoids vector<bool>-style iterators and exposes bit operations directly.
// Unused bits in the final block are always zero.
//
// References:
// - libstdc++ TR2 dynamic_bitset
// - Boost dynamic_bitset interface shape
// - dsacpp Bitmap as the teaching ancestor

#include <chaistl/containers/vector.hpp>
#include <chaistl/functional/hash.hpp>
#include <chaistl/utility/hardening.hpp>

#include <bit>
#include <climits>
#include <cstddef>
#include <functional>
#include <istream>
#include <limits>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>

namespace chaistl {

template <class Block = unsigned long long, class Allocator = allocator<Block>>
class dynamic_bitset {
  static_assert(std::is_unsigned_v<Block>, "dynamic_bitset block type must be unsigned");
  static_assert(std::is_integral_v<Block>, "dynamic_bitset block type must be integral");

 public:
  using block_type = Block;
  using allocator_type = Allocator;
  using size_type = std::size_t;

  static constexpr size_type npos = static_cast<size_type>(-1);

 private:
  static constexpr size_type bits_per_block = CHAR_BIT * sizeof(block_type);

  vector<block_type, allocator_type> blocks_;
  size_type size_ = 0;

  [[nodiscard]] static constexpr size_type block_count_for(size_type bit_count) noexcept {
    return bit_count == 0 ? 0 : (bit_count - 1) / bits_per_block + 1;
  }

  [[nodiscard]] static constexpr size_type block_index(size_type pos) noexcept { return pos / bits_per_block; }

  [[nodiscard]] static constexpr size_type bit_index(size_type pos) noexcept { return pos % bits_per_block; }

  [[nodiscard]] static constexpr block_type bit_mask(size_type pos) noexcept { return block_type{1} << bit_index(pos); }

  [[nodiscard]] constexpr block_type tail_mask() const noexcept {
    const auto used = size_ % bits_per_block;
    if (size_ == 0) {
      return block_type{0};
    }
    if (used == 0) {
      return std::numeric_limits<block_type>::max();
    }
    return (block_type{1} << used) - block_type{1};
  }

  constexpr void trim_tail() noexcept {
    if (!blocks_.empty()) {
      blocks_.back() &= tail_mask();
    }
  }

  constexpr void check_position(size_type pos, const char* function) const {
    if (pos >= size_) {
      throw std::out_of_range(function);
    }
  }

  constexpr void check_same_size(const dynamic_bitset& other, const char* function) const {
    if (size_ != other.size_) {
      throw std::invalid_argument(function);
    }
  }

 public:
  class reference {
   private:
    friend class dynamic_bitset;

    dynamic_bitset* owner_ = nullptr;
    size_type pos_ = 0;

    constexpr reference(dynamic_bitset& owner, size_type pos) noexcept : owner_(&owner), pos_(pos) {}

   public:
    reference() = delete;
    constexpr reference(const reference&) = default;
    constexpr ~reference() = default;

    constexpr reference& operator=(bool value) noexcept {
      owner_->set_unchecked(pos_, value);
      return *this;
    }

    constexpr reference& operator=(const reference& other) noexcept { return *this = static_cast<bool>(other); }

    constexpr bool operator~() const noexcept { return !static_cast<bool>(*this); }

    constexpr operator bool() const noexcept { return owner_->test_unchecked(pos_); }

    constexpr reference& flip() noexcept {
      owner_->flip_unchecked(pos_);
      return *this;
    }
  };

  constexpr dynamic_bitset() = default;

  explicit constexpr dynamic_bitset(const allocator_type& alloc) : blocks_(alloc) {}

  explicit constexpr dynamic_bitset(size_type bit_count,
                                    unsigned long long value = 0,
                                    const allocator_type& alloc = allocator_type{})
      : blocks_(block_count_for(bit_count), block_type{0}, alloc), size_(bit_count) {
    if (!blocks_.empty()) {
      blocks_[0] = static_cast<block_type>(value);
    }
    trim_tail();
  }

  template <class CharT, class Traits, class Alloc>
  explicit constexpr dynamic_bitset(
      const std::basic_string<CharT, Traits, Alloc>& str,
      typename std::basic_string<CharT, Traits, Alloc>::size_type pos = 0,
      typename std::basic_string<CharT, Traits, Alloc>::size_type n = std::basic_string<CharT, Traits, Alloc>::npos,
      CharT zero = CharT('0'),
      CharT one = CharT('1'),
      const allocator_type& alloc = allocator_type{})
      : blocks_(alloc) {
    if (pos > str.size()) {
      throw std::out_of_range("dynamic_bitset string constructor");
    }

    const auto available = str.size() - pos;
    const auto used = n < available ? n : available;
    resize(used);

    for (size_type i = 0; i < used; ++i) {
      const CharT ch = str[pos + used - 1 - i];
      if (Traits::eq(ch, one)) {
        set_unchecked(i, true);
      } else if (!Traits::eq(ch, zero)) {
        throw std::invalid_argument("dynamic_bitset string constructor");
      }
    }
  }

  [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

  [[nodiscard]] constexpr size_type size() const noexcept { return size_; }

  [[nodiscard]] constexpr size_type num_blocks() const noexcept { return blocks_.size(); }

  [[nodiscard]] constexpr allocator_type get_allocator() const noexcept { return blocks_.get_allocator(); }

  constexpr void resize(size_type bit_count, bool value = false) {
    const auto old_size = size_;
    const auto old_blocks = blocks_.size();
    const auto new_blocks = block_count_for(bit_count);

    blocks_.resize(new_blocks, value ? std::numeric_limits<block_type>::max() : block_type{0});
    size_ = bit_count;

    if (value && bit_count > old_size && old_blocks != 0 && old_size % bits_per_block != 0) {
      blocks_[old_blocks - 1] |= ~tail_mask_for(old_size);
    }
    trim_tail();
  }

  constexpr void clear() noexcept {
    blocks_.clear();
    size_ = 0;
  }

  constexpr void reserve(size_type bit_count) { blocks_.reserve(block_count_for(bit_count)); }

  constexpr void push_back(bool value) {
    const auto pos = size_;
    resize(size_ + 1);
    set_unchecked(pos, value);
  }

  constexpr void pop_back() {
    CHAI_HARDENED(!empty(), "dynamic_bitset::pop_back: empty bitset");
    resize(size_ - 1);
  }

  [[nodiscard]] constexpr bool operator[](size_type pos) const noexcept {
    CHAI_HARDENED(pos < size_, "dynamic_bitset::operator[]: out of bounds");
    return test_unchecked(pos);
  }

  [[nodiscard]] constexpr reference operator[](size_type pos) noexcept {
    CHAI_HARDENED(pos < size_, "dynamic_bitset::operator[]: out of bounds");
    return reference(*this, pos);
  }

  [[nodiscard]] constexpr bool test(size_type pos) const {
    check_position(pos, "dynamic_bitset::test");
    return test_unchecked(pos);
  }

  [[nodiscard]] constexpr bool all() const noexcept {
    if (size_ == 0) {
      return true;
    }
    for (size_type i = 0; i + 1 < blocks_.size(); ++i) {
      if (blocks_[i] != std::numeric_limits<block_type>::max()) {
        return false;
      }
    }
    return blocks_.back() == tail_mask();
  }

  [[nodiscard]] constexpr bool any() const noexcept {
    for (block_type block : blocks_) {
      if (block != block_type{0}) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] constexpr bool none() const noexcept { return !any(); }

  [[nodiscard]] constexpr size_type count() const noexcept {
    size_type result = 0;
    for (block_type block : blocks_) {
      result += static_cast<size_type>(std::popcount(block));
    }
    return result;
  }

  constexpr dynamic_bitset& operator&=(const dynamic_bitset& other) {
    check_same_size(other, "dynamic_bitset::operator&=");
    for (size_type i = 0; i < blocks_.size(); ++i) {
      blocks_[i] &= other.blocks_[i];
    }
    return *this;
  }

  constexpr dynamic_bitset& operator|=(const dynamic_bitset& other) {
    check_same_size(other, "dynamic_bitset::operator|=");
    for (size_type i = 0; i < blocks_.size(); ++i) {
      blocks_[i] |= other.blocks_[i];
    }
    return *this;
  }

  constexpr dynamic_bitset& operator^=(const dynamic_bitset& other) {
    check_same_size(other, "dynamic_bitset::operator^=");
    for (size_type i = 0; i < blocks_.size(); ++i) {
      blocks_[i] ^= other.blocks_[i];
    }
    return *this;
  }

  constexpr dynamic_bitset& operator<<=(size_type pos) noexcept {
    if (pos >= size_) {
      reset();
      return *this;
    }

    const auto word_shift = pos / bits_per_block;
    const auto shift = pos % bits_per_block;

    for (size_type i = blocks_.size(); i-- > 0;) {
      block_type value = block_type{0};
      if (i >= word_shift) {
        value = blocks_[i - word_shift] << shift;
        if (shift != 0 && i > word_shift) {
          value |= blocks_[i - word_shift - 1] >> (bits_per_block - shift);
        }
      }
      blocks_[i] = value;
    }

    trim_tail();
    return *this;
  }

  constexpr dynamic_bitset& operator>>=(size_type pos) noexcept {
    if (pos >= size_) {
      reset();
      return *this;
    }

    const auto word_shift = pos / bits_per_block;
    const auto shift = pos % bits_per_block;

    for (size_type i = 0; i < blocks_.size(); ++i) {
      block_type value = block_type{0};
      const auto source = i + word_shift;
      if (source < blocks_.size()) {
        value = blocks_[source] >> shift;
        if (shift != 0 && source + 1 < blocks_.size()) {
          value |= blocks_[source + 1] << (bits_per_block - shift);
        }
      }
      blocks_[i] = value;
    }

    trim_tail();
    return *this;
  }

  constexpr dynamic_bitset& set() noexcept {
    for (auto& block : blocks_) {
      block = std::numeric_limits<block_type>::max();
    }
    trim_tail();
    return *this;
  }

  constexpr dynamic_bitset& set(size_type pos, bool value = true) {
    check_position(pos, "dynamic_bitset::set");
    set_unchecked(pos, value);
    return *this;
  }

  constexpr dynamic_bitset& reset() noexcept {
    for (auto& block : blocks_) {
      block = block_type{0};
    }
    return *this;
  }

  constexpr dynamic_bitset& reset(size_type pos) {
    check_position(pos, "dynamic_bitset::reset");
    set_unchecked(pos, false);
    return *this;
  }

  constexpr dynamic_bitset& flip() noexcept {
    for (auto& block : blocks_) {
      block = ~block;
    }
    trim_tail();
    return *this;
  }

  constexpr dynamic_bitset& flip(size_type pos) {
    check_position(pos, "dynamic_bitset::flip");
    flip_unchecked(pos);
    return *this;
  }

  [[nodiscard]] constexpr dynamic_bitset operator~() const {
    dynamic_bitset result(*this);
    result.flip();
    return result;
  }

  [[nodiscard]] constexpr size_type find_first() const noexcept { return find_next_impl(npos); }

  [[nodiscard]] constexpr size_type find_next(size_type pos) const noexcept { return find_next_impl(pos); }

  template <class CharT = char, class Traits = std::char_traits<CharT>, class Alloc = std::allocator<CharT>>
  [[nodiscard]] constexpr std::basic_string<CharT, Traits, Alloc> to_string(CharT zero = CharT('0'),
                                                                            CharT one = CharT('1')) const {
    std::basic_string<CharT, Traits, Alloc> result;
    result.resize(size_, zero);
    for (size_type i = 0; i < size_; ++i) {
      if (test_unchecked(i)) {
        result[size_ - 1 - i] = one;
      }
    }
    return result;
  }

  [[nodiscard]] constexpr unsigned long to_ulong() const {
    return to_integer<unsigned long>("dynamic_bitset::to_ulong");
  }

  [[nodiscard]] constexpr unsigned long long to_ullong() const {
    return to_integer<unsigned long long>("dynamic_bitset::to_ullong");
  }

  constexpr void swap(dynamic_bitset& other) noexcept(noexcept(blocks_.swap(other.blocks_))) {
    using std::swap;
    blocks_.swap(other.blocks_);
    swap(size_, other.size_);
  }

  [[nodiscard]] friend constexpr bool operator==(const dynamic_bitset& lhs, const dynamic_bitset& rhs) noexcept {
    return lhs.size_ == rhs.size_ && lhs.blocks_ == rhs.blocks_;
  }

 private:
  [[nodiscard]] static constexpr block_type tail_mask_for(size_type bit_count) noexcept {
    const auto used = bit_count % bits_per_block;
    if (bit_count == 0) {
      return block_type{0};
    }
    if (used == 0) {
      return std::numeric_limits<block_type>::max();
    }
    return (block_type{1} << used) - block_type{1};
  }

  [[nodiscard]] constexpr bool test_unchecked(size_type pos) const noexcept {
    return (blocks_[block_index(pos)] & bit_mask(pos)) != block_type{0};
  }

  constexpr void set_unchecked(size_type pos, bool value) noexcept {
    if (value) {
      blocks_[block_index(pos)] |= bit_mask(pos);
    } else {
      blocks_[block_index(pos)] &= ~bit_mask(pos);
    }
  }

  constexpr void flip_unchecked(size_type pos) noexcept { blocks_[block_index(pos)] ^= bit_mask(pos); }

  [[nodiscard]] constexpr size_type find_next_impl(size_type previous) const noexcept {
    const size_type start = previous == npos ? 0 : previous + 1;
    if (start >= size_) {
      return npos;
    }

    size_type index = block_index(start);
    block_type block = blocks_[index] & (std::numeric_limits<block_type>::max() << bit_index(start));

    while (true) {
      if (block != block_type{0}) {
        const size_type pos = index * bits_per_block + static_cast<size_type>(std::countr_zero(block));
        return pos < size_ ? pos : npos;
      }
      ++index;
      if (index >= blocks_.size()) {
        return npos;
      }
      block = blocks_[index];
    }
  }

  template <class Integer>
  [[nodiscard]] constexpr Integer to_integer(const char* function) const {
    constexpr auto digits = std::numeric_limits<Integer>::digits;
    for (size_type i = digits; i < size_; ++i) {
      if (test_unchecked(i)) {
        throw std::overflow_error(function);
      }
    }

    Integer result = 0;
    const auto limit = size_ < digits ? size_ : static_cast<size_type>(digits);
    for (size_type i = 0; i < limit; ++i) {
      if (test_unchecked(i)) {
        result |= Integer{1} << i;
      }
    }
    return result;
  }
};

template <class Block, class Allocator>
[[nodiscard]] constexpr dynamic_bitset<Block, Allocator> operator&(dynamic_bitset<Block, Allocator> lhs,
                                                                   const dynamic_bitset<Block, Allocator>& rhs) {
  lhs &= rhs;
  return lhs;
}

template <class Block, class Allocator>
[[nodiscard]] constexpr dynamic_bitset<Block, Allocator> operator|(dynamic_bitset<Block, Allocator> lhs,
                                                                   const dynamic_bitset<Block, Allocator>& rhs) {
  lhs |= rhs;
  return lhs;
}

template <class Block, class Allocator>
[[nodiscard]] constexpr dynamic_bitset<Block, Allocator> operator^(dynamic_bitset<Block, Allocator> lhs,
                                                                   const dynamic_bitset<Block, Allocator>& rhs) {
  lhs ^= rhs;
  return lhs;
}

template <class Block, class Allocator>
[[nodiscard]] constexpr dynamic_bitset<Block, Allocator> operator<<(dynamic_bitset<Block, Allocator> value,
                                                                    std::size_t pos) noexcept {
  value <<= pos;
  return value;
}

template <class Block, class Allocator>
[[nodiscard]] constexpr dynamic_bitset<Block, Allocator> operator>>(dynamic_bitset<Block, Allocator> value,
                                                                    std::size_t pos) noexcept {
  value >>= pos;
  return value;
}

template <class Block, class Allocator, class CharT, class Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
                                              const dynamic_bitset<Block, Allocator>& value) {
  return os << value.template to_string<CharT, Traits, std::allocator<CharT>>();
}

template <class Block, class Allocator, class CharT, class Traits>
std::basic_istream<CharT, Traits>& operator>>(std::basic_istream<CharT, Traits>& is,
                                              dynamic_bitset<Block, Allocator>& value) {
  dynamic_bitset<Block, Allocator> temp(value.get_allocator());
  std::size_t read = 0;

  typename Traits::int_type next = is.rdbuf()->sgetc();
  while (!Traits::eq_int_type(next, Traits::eof())) {
    const auto ch = Traits::to_char_type(next);
    if (!Traits::eq(ch, CharT('0')) && !Traits::eq(ch, CharT('1'))) {
      break;
    }
    temp.push_back(false);
    temp <<= 1;
    if (Traits::eq(ch, CharT('1'))) {
      temp.set(0);
    }
    is.rdbuf()->snextc();
    ++read;
    next = is.rdbuf()->sgetc();
  }

  if (read == 0) {
    is.setstate(std::ios_base::failbit);
  } else {
    value = temp;
  }
  return is;
}

template <class Block, class Allocator>
constexpr void swap(dynamic_bitset<Block, Allocator>& lhs,
                    dynamic_bitset<Block, Allocator>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

template <class Block, class Allocator>
struct hash<dynamic_bitset<Block, Allocator>> {
  [[nodiscard]] constexpr std::size_t operator()(const dynamic_bitset<Block, Allocator>& value) const noexcept {
    std::size_t state = detail::fnv::offset_basis;
    unsigned char byte = 0;
    unsigned int bit = 0;

    for (std::size_t i = 0; i < value.size(); ++i) {
      if (value[i]) {
        byte |= static_cast<unsigned char>(1u << bit);
      }
      ++bit;
      if (bit == CHAR_BIT) {
        state = detail::fnv::append_byte(state, byte);
        byte = 0;
        bit = 0;
      }
    }

    if (bit != 0) {
      state = detail::fnv::append_byte(state, byte);
    }
    return state;
  }
};

}  // namespace chaistl

template <class Block, class Allocator>
struct std::hash<chaistl::dynamic_bitset<Block, Allocator>> {
  [[nodiscard]] constexpr std::size_t operator()(
      const chaistl::dynamic_bitset<Block, Allocator>& value) const noexcept {
    return chaistl::hash<chaistl::dynamic_bitset<Block, Allocator>>{}(value);
  }
};
