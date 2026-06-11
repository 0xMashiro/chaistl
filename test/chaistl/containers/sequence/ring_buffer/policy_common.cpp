// SPDX-License-Identifier: Apache-2.0

// References:
// - Policy protocol: include/chaistl/experimental/containers/ring_buffer.hpp
//
// Behavior shared by every overflow policy: as long as the buffer never
// becomes full, the policy is never invoked, so construction, iteration,
// push/pop, insert/erase, comparison, swap, resize and set_capacity must be
// identical across all policies — checked against a std::deque oracle under
// randomized workloads, with validate() after every single mutation. New
// policies join the Policies list and inherit the whole suite.

#include <gtest/gtest.h>

#include <chaistl/experimental/containers/ring_buffer.hpp>

#include <algorithm>
#include <array>
#include <deque>
#include <random>
#include <ranges>
#include <string>
#include <utility>

#include "test_policies.hpp"

namespace {

template <class Policy>
class RingBufferPolicyCommonTest : public ::testing::Test {
 public:
  using int_buffer = chaistl::experimental::ring_buffer<int, chaistl::allocator<int>, Policy>;
  using string_buffer = chaistl::experimental::ring_buffer<std::string, chaistl::allocator<std::string>, Policy>;
};

using Policies = ::testing::
    Types<chaistl::experimental::overwrite_policy, chaistl::experimental::reject_policy, theirs::evict_oldest_policy>;
TYPED_TEST_SUITE(RingBufferPolicyCommonTest, Policies);

// ============================================================================
// Construction and assignment
// ============================================================================

TYPED_TEST(RingBufferPolicyCommonTest, ConstructionFamily) {
  using buffer = typename TestFixture::int_buffer;

  buffer by_default;
  EXPECT_TRUE(by_default.empty());
  EXPECT_EQ(by_default.capacity(), 0u);

  buffer by_capacity(4);
  EXPECT_TRUE(by_capacity.empty());
  EXPECT_EQ(by_capacity.capacity(), 4u);

  buffer by_count(3, 7);
  EXPECT_TRUE(std::ranges::equal(by_count, std::array{7, 7, 7}));

  std::array source{1, 2, 3};
  buffer by_range(source.begin(), source.end());
  EXPECT_TRUE(std::ranges::equal(by_range, source));

  buffer by_init{4, 5, 6};
  EXPECT_TRUE(std::ranges::equal(by_init, std::array{4, 5, 6}));

  buffer by_from_range(std::from_range, std::views::iota(1, 4));
  EXPECT_TRUE(std::ranges::equal(by_from_range, std::array{1, 2, 3}));

  buffer copied(by_init);
  EXPECT_TRUE(std::ranges::equal(copied, std::array{4, 5, 6}));

  buffer moved(std::move(copied));
  EXPECT_TRUE(std::ranges::equal(moved, std::array{4, 5, 6}));
}

TYPED_TEST(RingBufferPolicyCommonTest, AssignFamilyResetsCapacity) {
  using buffer = typename TestFixture::int_buffer;

  buffer rb;  // zero capacity: assignment must still work
  rb = {1, 2, 3};
  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 2, 3}));
  EXPECT_EQ(rb.capacity(), 3u);

  rb.assign(2, 9);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{9, 9}));
  EXPECT_EQ(rb.capacity(), 2u);

  std::array source{5, 6, 7, 8};
  rb.assign(source.begin(), source.end());
  EXPECT_TRUE(std::ranges::equal(rb, source));
  EXPECT_EQ(rb.capacity(), 4u);

  rb.assign_range(std::views::iota(0, 3));
  EXPECT_TRUE(std::ranges::equal(rb, std::array{0, 1, 2}));
}

// ============================================================================
// Modifiers below capacity (the policy must never be consulted)
// ============================================================================

TYPED_TEST(RingBufferPolicyCommonTest, PushPopBelowCapacity) {
  using buffer = typename TestFixture::int_buffer;

  buffer rb(8);
  rb.push_back(2);
  rb.push_back(3);
  rb.push_front(1);
  rb.emplace_back(4);
  rb.emplace_front(0);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{0, 1, 2, 3, 4}));

  EXPECT_TRUE(rb.try_push_back(5));
  EXPECT_TRUE(rb.try_push_front(-1));
  EXPECT_TRUE(std::ranges::equal(rb, std::array{-1, 0, 1, 2, 3, 4, 5}));

  rb.pop_front();
  rb.pop_back();
  EXPECT_TRUE(std::ranges::equal(rb, std::array{0, 1, 2, 3, 4}));
}

TYPED_TEST(RingBufferPolicyCommonTest, InsertEraseBelowCapacity) {
  using buffer = typename TestFixture::int_buffer;

  buffer rb(8);
  rb.push_back(1);
  rb.push_back(3);

  auto it = rb.insert(rb.begin() + 1, 2);
  EXPECT_EQ(*it, 2);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 2, 3}));

  it = rb.emplace(rb.end(), 4);
  EXPECT_EQ(*it, 4);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 2, 3, 4}));

  it = rb.erase(rb.begin() + 1);
  EXPECT_EQ(*it, 3);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 3, 4}));

  it = rb.erase(rb.begin(), rb.begin() + 2);
  EXPECT_EQ(*it, 4);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{4}));
}

// ============================================================================
// Iterators
// ============================================================================

TYPED_TEST(RingBufferPolicyCommonTest, RandomAccessIteration) {
  using buffer = typename TestFixture::int_buffer;
  static_assert(std::random_access_iterator<typename buffer::iterator>);
  static_assert(std::random_access_iterator<typename buffer::const_iterator>);

  buffer rb{10, 20, 30, 40};

  auto it = rb.begin();
  EXPECT_EQ(it[2], 30);
  EXPECT_EQ(*(it + 3), 40);
  EXPECT_EQ(*(3 + it), 40);
  EXPECT_EQ(rb.end() - rb.begin(), 4);
  it += 2;
  it -= 1;
  EXPECT_EQ(*it, 20);
  it++;
  it--;
  EXPECT_EQ(*it, 20);

  EXPECT_TRUE(std::ranges::equal(std::views::reverse(rb), std::array{40, 30, 20, 10}));
}

// ============================================================================
// Capacity operations
// ============================================================================

TYPED_TEST(RingBufferPolicyCommonTest, ResizeAndSetCapacity) {
  using buffer = typename TestFixture::int_buffer;

  buffer rb{1, 2, 3};
  rb.resize(2);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 2}));

  rb.resize(4, 9);  // grows past capacity (Boost.CircularBuffer semantics)
  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 2, 9, 9}));
  EXPECT_GE(rb.capacity(), 4u);

  rb.set_capacity(2);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 2}));
  EXPECT_EQ(rb.capacity(), 2u);

  rb.set_capacity(5);
  EXPECT_TRUE(std::ranges::equal(rb, std::array{1, 2}));
  EXPECT_EQ(rb.capacity(), 5u);
}

// ============================================================================
// Comparison, swap, clear
// ============================================================================

TYPED_TEST(RingBufferPolicyCommonTest, ComparisonSwapClear) {
  using buffer = typename TestFixture::int_buffer;

  buffer a{1, 2, 3};
  buffer b{1, 2, 3};
  buffer c{1, 2, 4};
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a == c);
  EXPECT_TRUE(a < c);

  a.swap(c);
  EXPECT_TRUE(std::ranges::equal(a, std::array{1, 2, 4}));
  EXPECT_TRUE(std::ranges::equal(c, std::array{1, 2, 3}));

  a.clear();
  EXPECT_TRUE(a.empty());
}

// ============================================================================
// Non-trivial element type (exercises the allocator construct/destroy paths)
// ============================================================================

TYPED_TEST(RingBufferPolicyCommonTest, StringElements) {
  using buffer = typename TestFixture::string_buffer;

  buffer rb(4);
  rb.push_back("bb");
  rb.push_front("aa");
  rb.emplace_back(3, 'c');  // "ccc"
  EXPECT_TRUE(std::ranges::equal(rb, std::array<std::string, 3>{"aa", "bb", "ccc"}));

  rb.insert(rb.begin() + 1, "ab");
  EXPECT_EQ(rb[1], "ab");
  rb.erase(rb.begin());
  EXPECT_TRUE(std::ranges::equal(rb, std::array<std::string, 3>{"ab", "bb", "ccc"}));
}

// ============================================================================
// Randomized workload vs std::deque oracle (never reaches full, so every
// policy must produce byte-identical behavior)
// ============================================================================

TYPED_TEST(RingBufferPolicyCommonTest, RandomizedOpsMatchDequeOracle) {
  using buffer = typename TestFixture::int_buffer;
  constexpr std::size_t kCapacity = 32;

  for (const unsigned seed : {1u, 2u, 3u}) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> value(0, 999);
    std::uniform_int_distribution<int> op(0, 99);

    buffer rb(kCapacity);
    std::deque<int> oracle;

    for (int step = 0; step < 4000; ++step) {
      const int action = op(rng);
      // Guard: stay strictly below capacity so the policy is never invoked.
      const bool can_grow = oracle.size() + 1 < kCapacity;

      if (action < 30 && can_grow) {
        const int v = value(rng);
        rb.push_back(v);
        oracle.push_back(v);
      } else if (action < 50 && can_grow) {
        const int v = value(rng);
        rb.push_front(v);
        oracle.push_front(v);
      } else if (action < 65 && can_grow) {
        const int v = value(rng);
        const auto offset = static_cast<std::ptrdiff_t>(rng() % (oracle.size() + 1));
        auto it = rb.insert(rb.begin() + offset, v);
        auto oracle_it = oracle.insert(oracle.begin() + offset, v);
        ASSERT_EQ(*it, *oracle_it) << "seed " << seed << " step " << step;
      } else if (action < 75 && !oracle.empty()) {
        rb.pop_back();
        oracle.pop_back();
      } else if (action < 85 && !oracle.empty()) {
        rb.pop_front();
        oracle.pop_front();
      } else if (!oracle.empty()) {
        const auto offset = static_cast<std::ptrdiff_t>(rng() % oracle.size());
        rb.erase(rb.begin() + offset);
        oracle.erase(oracle.begin() + offset);
      }

      ASSERT_TRUE(rb.validate()) << "seed " << seed << " step " << step;
      ASSERT_EQ(rb.size(), oracle.size()) << "seed " << seed << " step " << step;
      ASSERT_TRUE(std::ranges::equal(rb, oracle)) << "seed " << seed << " step " << step;
    }
  }
}

}  // namespace
