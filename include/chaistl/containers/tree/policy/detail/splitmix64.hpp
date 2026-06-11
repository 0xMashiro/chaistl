// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace chaistl::detail::tree {

class splitmix64 {
 public:
  [[nodiscard]] constexpr std::uint64_t next() noexcept {
    std::uint64_t z = (state_ += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
  }

 private:
  // Fixed seeding gives deterministic reproducibility for tests and examples.
  // The tradeoff is that adversarial inputs can target the known sequence and
  // construct degenerate trees; this is a known limitation of these policies.
  std::uint64_t state_ = 0x9e3779b97f4a7c15ULL;
};

}  // namespace chaistl::detail::tree
