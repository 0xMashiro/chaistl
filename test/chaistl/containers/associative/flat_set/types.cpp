// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_set.hpp>

#include <algorithm>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using ::testing::ElementsAre;

// Move-only key: ordering goes through the pointee.
struct move_only_key {
  std::unique_ptr<int> value;

  explicit move_only_key(int v) : value(std::make_unique<int>(v)) {}
  move_only_key(move_only_key&&) noexcept = default;
  move_only_key& operator=(move_only_key&&) noexcept = default;

  friend bool operator<(const move_only_key& lhs, const move_only_key& rhs) { return *lhs.value < *rhs.value; }
};

// Key whose move constructor throws once a configurable number of moves is
// reached. Used to sweep every possible throw point of a bulk insert.
struct counted_throwing_move {
  static inline int budget = -1;  // -1 disables throwing
  int value = 0;

  counted_throwing_move() = default;
  explicit counted_throwing_move(int v) : value(v) {}
  counted_throwing_move(const counted_throwing_move& other) : value(other.value) {}
  counted_throwing_move(counted_throwing_move&& other) : value(other.value) {
    if (budget >= 0 && --budget < 0) {
      throw std::runtime_error("move budget exhausted");
    }
  }
  counted_throwing_move& operator=(const counted_throwing_move& other) = default;
  counted_throwing_move& operator=(counted_throwing_move&& other) {
    value = other.value;
    if (budget >= 0 && --budget < 0) {
      throw std::runtime_error("move budget exhausted");
    }
    return *this;
  }

  friend bool operator<(const counted_throwing_move& lhs, const counted_throwing_move& rhs) {
    return lhs.value < rhs.value;
  }
};

TEST(FlatSetTypes, MoveOnlyKeyType) {
  chaistl::flat_set<move_only_key> s;

  s.insert(move_only_key(2));
  s.emplace(1);
  s.insert(s.end(), move_only_key(3));

  EXPECT_EQ(s.size(), 3uz);
  EXPECT_EQ(*s.begin()->value, 1);

  auto storage = std::move(s).extract();
  EXPECT_EQ(storage.size(), 3uz);
  EXPECT_TRUE(s.empty());
}

// Sweep every throw point: whatever step of the bulk insert throws, the
// invariant must hold and the content must be either the original or cleared.
TEST(FlatSetTypes, ThrowingMoveSweepKeepsInvariant) {
  for (int budget = 0; budget < 64; ++budget) {
    chaistl::flat_set<counted_throwing_move> s;
    s.emplace(1);
    s.emplace(5);

    std::vector<counted_throwing_move> incoming;
    incoming.emplace_back(4);
    incoming.emplace_back(2);
    incoming.emplace_back(4);

    counted_throwing_move::budget = budget;
    bool threw = false;
    try {
      s.insert(incoming.begin(), incoming.end());
    } catch (const std::runtime_error&) {
      threw = true;
    }
    counted_throwing_move::budget = -1;

    ASSERT_TRUE(s.validate()) << "budget=" << budget;
    if (threw) {
      const bool rolled_back = s.size() == 2 && s.begin()->value == 1 && std::next(s.begin())->value == 5;
      ASSERT_TRUE(rolled_back || s.empty()) << "budget=" << budget << " size=" << s.size();
    } else {
      ASSERT_EQ(s.size(), 4uz) << "budget=" << budget;
      break;  // enough budget to finish: later sweeps are identical
    }
  }
}

TEST(FlatSetTypes, StatefulComparatorPropagatesOnSwap) {
  struct offset_less {
    int id = 0;
    bool operator()(int lhs, int rhs) const { return lhs < rhs; }
  };

  chaistl::flat_set<int, offset_less> a({1}, offset_less{1});
  chaistl::flat_set<int, offset_less> b({2}, offset_less{2});

  a.swap(b);

  EXPECT_EQ(a.key_comp().id, 2);
  EXPECT_EQ(b.key_comp().id, 1);
  EXPECT_TRUE(a.contains(2));
  EXPECT_TRUE(b.contains(1));
}

TEST(FlatSetTypes, IteratorWorksWithRangesAlgorithms) {
  chaistl::flat_set<int> s{1, 2, 3, 4};

  EXPECT_TRUE(std::ranges::is_sorted(s));
  EXPECT_EQ(std::ranges::count_if(s,
                                  [](int value) {
                                    return value % 2 == 0;
                                  }),
            2);
  const auto bound = std::ranges::lower_bound(s, 3);
  EXPECT_EQ(*bound, 3);
}

}  // namespace
