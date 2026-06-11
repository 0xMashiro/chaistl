// SPDX-License-Identifier: Apache-2.0

#pragma once

// ============================================================================
// bitset - Fixed-size bit sequence
// ============================================================================
//
// chaistl::bitset follows the standard std::bitset surface closely while
// keeping the storage invariant explicit: unused bits in the last word are
// always zero. That invariant keeps equality, count, conversions, and hashing
// simple to reason about.
//
// References:
// - cppreference: https://en.cppreference.com/w/cpp/utility/bitset
// - libstdc++ <bitset> and EASTL bitset for implementation archaeology.

#include <chaistl/functional/hash.hpp>
#include <chaistl/utility/hardening.hpp>

#include <array>
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

template <std::size_t N>
class bitset {
 private:
  using block_type = unsigned long long;

  static constexpr std::size_t bits_per_block = CHAR_BIT * sizeof(block_type);
  static constexpr std::size_t block_count = N == 0 ? 0 : (N - 1) / bits_per_block + 1;
  static constexpr std::size_t storage_count = block_count == 0 ? 1 : block_count;
  static constexpr std::size_t last_bits = N % bits_per_block;
  static constexpr block_type all_ones = std::numeric_limits<block_type>::max();

  std::array<block_type, storage_count> words_{};

  [[nodiscard]] static constexpr std::size_t word_index(std::size_t pos) noexcept { return pos / bits_per_block; }

  [[nodiscard]] static constexpr std::size_t bit_index(std::size_t pos) noexcept { return pos % bits_per_block; }

  [[nodiscard]] static constexpr block_type bit_mask(std::size_t pos) noexcept {
    return block_type{1} << bit_index(pos);
  }

  [[nodiscard]] static constexpr block_type tail_mask() noexcept {
    if constexpr (N == 0) {
      return block_type{0};
    } else if constexpr (last_bits == 0) {
      return all_ones;
    } else {
      return (block_type{1} << last_bits) - block_type{1};
    }
  }

  constexpr void trim_tail() noexcept {
    if constexpr (N == 0) {
      words_[0] = block_type{0};
    } else {
      words_[block_count - 1] &= tail_mask();
    }
  }

  constexpr void check_position(std::size_t pos, const char* function) const {
    if (pos >= N) {
      throw std::out_of_range(function);
    }
  }

 public:
  class reference {
   private:
    friend class bitset;

    bitset* owner_ = nullptr;
    std::size_t pos_ = 0;

    constexpr reference(bitset& owner, std::size_t pos) noexcept : owner_(&owner), pos_(pos) {}

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

  constexpr bitset() noexcept = default;

  constexpr bitset(unsigned long long value) noexcept {
    if constexpr (block_count > 0) {
      words_[0] = static_cast<block_type>(value);
    }
    trim_tail();
  }

  template <class CharT, class Traits, class Allocator>
  explicit constexpr bitset(const std::basic_string<CharT, Traits, Allocator>& str,
                            typename std::basic_string<CharT, Traits, Allocator>::size_type pos = 0,
                            typename std::basic_string<CharT, Traits, Allocator>::size_type n =
                                std::basic_string<CharT, Traits, Allocator>::npos,
                            CharT zero = CharT('0'),
                            CharT one = CharT('1')) {
    if (pos > str.size()) {
      throw std::out_of_range("bitset string constructor");
    }

    const auto available = str.size() - pos;
    const auto used = n < available ? n : available;
    const auto limit = used < N ? used : N;

    for (std::size_t i = 0; i < limit; ++i) {
      const CharT ch = str[pos + used - 1 - i];
      if (Traits::eq(ch, one)) {
        set_unchecked(i, true);
      } else if (!Traits::eq(ch, zero)) {
        throw std::invalid_argument("bitset string constructor");
      }
    }

    for (auto i = limit; i < used; ++i) {
      const CharT ch = str[pos + used - 1 - i];
      if (!Traits::eq(ch, zero) && !Traits::eq(ch, one)) {
        throw std::invalid_argument("bitset string constructor");
      }
    }
  }

  [[nodiscard]] static constexpr std::size_t size() noexcept { return N; }

  [[nodiscard]] constexpr bool operator[](std::size_t pos) const noexcept {
    CHAI_HARDENED(pos < N, "bitset::operator[]: out of bounds");
    return test_unchecked(pos);
  }

  [[nodiscard]] constexpr reference operator[](std::size_t pos) noexcept {
    CHAI_HARDENED(pos < N, "bitset::operator[]: out of bounds");
    return reference(*this, pos);
  }

  [[nodiscard]] constexpr bool test(std::size_t pos) const {
    check_position(pos, "bitset::test");
    return test_unchecked(pos);
  }

  [[nodiscard]] constexpr bool all() const noexcept {
    if constexpr (N == 0) {
      return true;
    } else {
      for (std::size_t i = 0; i + 1 < block_count; ++i) {
        if (words_[i] != all_ones) {
          return false;
        }
      }
      return words_[block_count - 1] == tail_mask();
    }
  }

  [[nodiscard]] constexpr bool any() const noexcept {
    for (std::size_t i = 0; i < block_count; ++i) {
      if (words_[i] != block_type{0}) {
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] constexpr bool none() const noexcept { return !any(); }

  [[nodiscard]] constexpr std::size_t count() const noexcept {
    std::size_t result = 0;
    for (std::size_t i = 0; i < block_count; ++i) {
      result += static_cast<std::size_t>(std::popcount(words_[i]));
    }
    return result;
  }

  constexpr bitset& operator&=(const bitset& other) noexcept {
    for (std::size_t i = 0; i < block_count; ++i) {
      words_[i] &= other.words_[i];
    }
    return *this;
  }

  constexpr bitset& operator|=(const bitset& other) noexcept {
    for (std::size_t i = 0; i < block_count; ++i) {
      words_[i] |= other.words_[i];
    }
    return *this;
  }

  constexpr bitset& operator^=(const bitset& other) noexcept {
    for (std::size_t i = 0; i < block_count; ++i) {
      words_[i] ^= other.words_[i];
    }
    return *this;
  }

  constexpr bitset& operator<<=(std::size_t pos) noexcept {
    if (pos >= N) {
      reset();
      return *this;
    }

    const auto word_shift = pos / bits_per_block;
    const auto bit_shift = pos % bits_per_block;

    for (std::size_t i = block_count; i-- > 0;) {
      block_type value = block_type{0};
      if (i >= word_shift) {
        value = words_[i - word_shift] << bit_shift;
        if (bit_shift != 0 && i > word_shift) {
          value |= words_[i - word_shift - 1] >> (bits_per_block - bit_shift);
        }
      }
      words_[i] = value;
    }

    trim_tail();
    return *this;
  }

  constexpr bitset& operator>>=(std::size_t pos) noexcept {
    if (pos >= N) {
      reset();
      return *this;
    }

    const auto word_shift = pos / bits_per_block;
    const auto bit_shift = pos % bits_per_block;

    for (std::size_t i = 0; i < block_count; ++i) {
      block_type value = block_type{0};
      const auto source = i + word_shift;
      if (source < block_count) {
        value = words_[source] >> bit_shift;
        if (bit_shift != 0 && source + 1 < block_count) {
          value |= words_[source + 1] << (bits_per_block - bit_shift);
        }
      }
      words_[i] = value;
    }

    trim_tail();
    return *this;
  }

  constexpr bitset& set() noexcept {
    for (std::size_t i = 0; i < block_count; ++i) {
      words_[i] = all_ones;
    }
    trim_tail();
    return *this;
  }

  constexpr bitset& set(std::size_t pos, bool value = true) {
    check_position(pos, "bitset::set");
    set_unchecked(pos, value);
    return *this;
  }

  constexpr bitset& reset() noexcept {
    for (auto& word : words_) {
      word = block_type{0};
    }
    return *this;
  }

  constexpr bitset& reset(std::size_t pos) {
    check_position(pos, "bitset::reset");
    set_unchecked(pos, false);
    return *this;
  }

  constexpr bitset& flip() noexcept {
    for (std::size_t i = 0; i < block_count; ++i) {
      words_[i] = ~words_[i];
    }
    trim_tail();
    return *this;
  }

  constexpr bitset& flip(std::size_t pos) {
    check_position(pos, "bitset::flip");
    flip_unchecked(pos);
    return *this;
  }

  [[nodiscard]] constexpr bitset operator~() const noexcept {
    bitset result(*this);
    result.flip();
    return result;
  }

  template <class CharT = char, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
  [[nodiscard]] constexpr std::basic_string<CharT, Traits, Allocator> to_string(CharT zero = CharT('0'),
                                                                                CharT one = CharT('1')) const {
    std::basic_string<CharT, Traits, Allocator> result;
    result.resize(N, zero);
    for (std::size_t i = 0; i < N; ++i) {
      if (test_unchecked(i)) {
        result[N - 1 - i] = one;
      }
    }
    return result;
  }

  [[nodiscard]] constexpr unsigned long to_ulong() const { return to_integer<unsigned long>("bitset::to_ulong"); }

  [[nodiscard]] constexpr unsigned long long to_ullong() const {
    return to_integer<unsigned long long>("bitset::to_ullong");
  }

  [[nodiscard]] friend constexpr bool operator==(const bitset& lhs, const bitset& rhs) noexcept = default;

 private:
  [[nodiscard]] constexpr bool test_unchecked(std::size_t pos) const noexcept {
    return (words_[word_index(pos)] & bit_mask(pos)) != block_type{0};
  }

  constexpr void set_unchecked(std::size_t pos, bool value) noexcept {
    if (value) {
      words_[word_index(pos)] |= bit_mask(pos);
    } else {
      words_[word_index(pos)] &= ~bit_mask(pos);
    }
  }

  constexpr void flip_unchecked(std::size_t pos) noexcept { words_[word_index(pos)] ^= bit_mask(pos); }

  template <class Integer>
  [[nodiscard]] constexpr Integer to_integer(const char* function) const {
    constexpr auto digits = std::numeric_limits<Integer>::digits;
    for (std::size_t i = digits; i < N; ++i) {
      if (test_unchecked(i)) {
        throw std::overflow_error(function);
      }
    }

    Integer result = 0;
    const auto limit = N < digits ? N : static_cast<std::size_t>(digits);
    for (std::size_t i = 0; i < limit; ++i) {
      if (test_unchecked(i)) {
        result |= Integer{1} << i;
      }
    }
    return result;
  }
};

template <std::size_t N>
[[nodiscard]] constexpr bitset<N> operator&(bitset<N> lhs, const bitset<N>& rhs) noexcept {
  lhs &= rhs;
  return lhs;
}

template <std::size_t N>
[[nodiscard]] constexpr bitset<N> operator|(bitset<N> lhs, const bitset<N>& rhs) noexcept {
  lhs |= rhs;
  return lhs;
}

template <std::size_t N>
[[nodiscard]] constexpr bitset<N> operator^(bitset<N> lhs, const bitset<N>& rhs) noexcept {
  lhs ^= rhs;
  return lhs;
}

template <std::size_t N>
[[nodiscard]] constexpr bitset<N> operator<<(bitset<N> value, std::size_t pos) noexcept {
  value <<= pos;
  return value;
}

template <std::size_t N>
[[nodiscard]] constexpr bitset<N> operator>>(bitset<N> value, std::size_t pos) noexcept {
  value >>= pos;
  return value;
}

template <std::size_t N, class CharT, class Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const bitset<N>& value) {
  return os << value.template to_string<CharT, Traits, std::allocator<CharT>>();
}

template <std::size_t N, class CharT, class Traits>
std::basic_istream<CharT, Traits>& operator>>(std::basic_istream<CharT, Traits>& is, bitset<N>& value) {
  bitset<N> temp;
  if constexpr (N == 0) {
    value = temp;
    return is;
  }

  std::size_t read = 0;

  typename Traits::int_type next = is.rdbuf()->sgetc();
  while (read < N && !Traits::eq_int_type(next, Traits::eof())) {
    const auto ch = Traits::to_char_type(next);
    if (!Traits::eq(ch, CharT('0')) && !Traits::eq(ch, CharT('1'))) {
      break;
    }
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

template <std::size_t N>
struct hash<bitset<N>> {
  [[nodiscard]] constexpr std::size_t operator()(const bitset<N>& value) const noexcept {
    std::size_t state = detail::fnv::offset_basis;
    unsigned char byte = 0;
    unsigned int bit = 0;

    for (std::size_t i = 0; i < N; ++i) {
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

template <std::size_t N>
struct std::hash<chaistl::bitset<N>> {
  [[nodiscard]] constexpr std::size_t operator()(const chaistl::bitset<N>& value) const noexcept {
    return chaistl::hash<chaistl::bitset<N>>{}(value);
  }
};
