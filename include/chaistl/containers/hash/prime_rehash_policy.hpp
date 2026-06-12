// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <chaistl/utility/hardening.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <limits>

namespace chaistl::detail::hash {

/**
 * @brief Compile-time primality test used to validate the bucket count table.
 *
 * Trial division with a 6k±1 wheel. Arithmetic runs in unsigned long long so
 * the @c f*f termination check cannot overflow on 32-bit @c size_t targets.
 */
[[nodiscard]] consteval bool is_prime(unsigned long long candidate) noexcept {
  if (candidate < 2) {
    return false;
  }
  if (candidate % 2 == 0) {
    return candidate == 2;
  }
  if (candidate % 3 == 0) {
    return candidate == 3;
  }
  for (unsigned long long factor = 5; factor * factor <= candidate; factor += 6) {
    if (candidate % factor == 0 || candidate % (factor + 2) == 0) {
      return false;
    }
  }
  return true;
}

/**
 * @brief Bucket counts available to @c prime_rehash_policy.
 *
 * Roughly doubling primes (the classic SGI progression plus small entries).
 * Geometric growth comes from the spacing of this table: the policy never
 * multiplies a bucket count, it only looks up the smallest entry that
 * satisfies the load-factor requirement, and the next entry is ~2x away.
 *
 * The table ends at the largest prime below 2^32. Tables beyond ~4.29e9
 * buckets are outside this teaching library's scope; @c max_bucket_count()
 * reports the cap honestly.
 */
inline constexpr std::array<std::size_t, 32> prime_bucket_counts = {
    2,        5,         11,        23,        47,        97,         193,        389,
    769,      1543,      3079,      6151,      12289,     24593,      49157,      98317,
    196613,   393241,    786433,    1572869,   3145739,   6291469,    12582917,   25165843,
    50331653, 100663319, 201326611, 402653189, 805306457, 1610612741, 3221225473, 4294967291,
};

static_assert(std::ranges::is_sorted(prime_bucket_counts));
static_assert(std::ranges::all_of(prime_bucket_counts,
                                  [](std::size_t n) {
                                    return is_prime(n);
                                  }),
              "every bucket count must be prime — the policy's tolerance of weak hash functions "
              "depends on it");

/**
 * @brief Default rehash policy: prime bucket counts, modulo bucket indexing.
 *
 * Prime-based sizing is the default because `hash % prime` spreads even
 * low-quality hash functions across buckets, which suits a teaching library.
 * A power-of-two policy with explicit mixing is a planned experimental
 * alternative, not a default (ADR: Hash Table Design).
 *
 * The whole policy surface is three operations; resize triggers beyond the
 * load-factor check, range hashing, probing, and tombstones are deliberately
 * not its job.
 */
class prime_rehash_policy {
 public:
  using size_type = std::size_t;

  /**
   * @brief Smallest supported bucket count that is >= @p requested.
   *
   * @pre @p requested does not exceed @c max_bucket_count().
   */
  [[nodiscard]] static constexpr size_type next_bucket_count(size_type requested) noexcept {
    CHAI_HARDENED(requested <= prime_bucket_counts.back(),
                  "prime_rehash_policy::next_bucket_count: beyond max_bucket_count()");
    return *std::ranges::lower_bound(prime_bucket_counts, requested);
  }

  /**
   * @brief Maps a hash code to a bucket index in [0, bucket_count).
   *
   * @pre @p bucket_count > 0.
   */
  [[nodiscard]] static constexpr size_type bucket_for_hash(std::size_t hash_code, size_type bucket_count) noexcept {
    CHAI_HARDENED(bucket_count > 0, "prime_rehash_policy::bucket_for_hash: zero buckets");
    return hash_code % bucket_count;
  }

  /**
   * @brief Whether inserting @p incoming_count more elements requires a rehash.
   *
   * Mirrors the standard's iterator-validity rule: no rehash while
   * `(N + n) <= z * B` ([unord.req.general]). A zero-bucket table always
   * rehashes — the default-constructed table allocates nothing until then.
   */
  [[nodiscard]] static constexpr bool need_rehash(size_type current_size,
                                                  size_type incoming_count,
                                                  size_type bucket_count,
                                                  float max_load_factor) noexcept {
    if (bucket_count == 0) {
      return true;
    }
    return static_cast<float>(current_size + incoming_count) > max_load_factor * static_cast<float>(bucket_count);
  }

  /// Largest bucket count this policy can produce.
  [[nodiscard]] static constexpr size_type max_bucket_count() noexcept { return prime_bucket_counts.back(); }
};

/**
 * @brief Power-of-two bucket counts with mask-based bucket indexing.
 *
 * This policy mirrors the MSVC / PBDS style trade-off: `hash & (B - 1)` is
 * cheaper than modulo, but the hash function must provide useful low bits.
 * It is intentionally an opt-in policy; the default remains prime-based for
 * tolerance of simple teaching hashers.
 */
class power2_rehash_policy {
 public:
  using size_type = std::size_t;

  [[nodiscard]] static constexpr size_type next_bucket_count(size_type requested) noexcept {
    constexpr size_type min_bucket_count = 2;
    if (requested <= min_bucket_count) {
      return min_bucket_count;
    }
    CHAI_HARDENED(requested <= max_bucket_count(),
                  "power2_rehash_policy::next_bucket_count: beyond max_bucket_count()");
    return std::bit_ceil(requested);
  }

  [[nodiscard]] static constexpr size_type bucket_for_hash(std::size_t hash_code, size_type bucket_count) noexcept {
    CHAI_HARDENED(bucket_count > 0, "power2_rehash_policy::bucket_for_hash: zero buckets");
    CHAI_HARDENED((bucket_count & (bucket_count - 1)) == 0,
                  "power2_rehash_policy::bucket_for_hash: bucket count must be a power of two");
    return hash_code & (bucket_count - 1);
  }

  [[nodiscard]] static constexpr bool need_rehash(size_type current_size,
                                                  size_type incoming_count,
                                                  size_type bucket_count,
                                                  float max_load_factor) noexcept {
    if (bucket_count == 0) {
      return true;
    }
    return static_cast<float>(current_size + incoming_count) > max_load_factor * static_cast<float>(bucket_count);
  }

  [[nodiscard]] static constexpr size_type max_bucket_count() noexcept {
    return size_type{1} << (std::numeric_limits<size_type>::digits - 1);
  }
};

}  // namespace chaistl::detail::hash
