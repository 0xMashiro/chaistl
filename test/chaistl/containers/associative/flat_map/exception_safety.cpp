// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_map.hpp>

#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using ::testing::ElementsAre;

// Comparator that throws once a configurable number of comparisons is reached.
struct throwing_compare {
  int* budget = nullptr;

  bool operator()(int lhs, int rhs) const {
    if (budget != nullptr && --*budget < 0) {
      throw std::runtime_error("comparison budget exhausted");
    }
    return lhs < rhs;
  }
};

// Mapped type whose copy constructor throws when armed.
struct throwing_copy {
  static inline bool armed = false;
  int value = 0;

  throwing_copy() = default;
  explicit throwing_copy(int v) : value(v) {}
  throwing_copy(const throwing_copy& other) : value(other.value) {
    if (armed) {
      throw std::runtime_error("copy failed");
    }
  }
  throwing_copy(throwing_copy&&) noexcept = default;
  throwing_copy& operator=(const throwing_copy&) = default;
  throwing_copy& operator=(throwing_copy&&) noexcept = default;
};

TEST(FlatMapExceptionSafety, SingleInsertRollsBackKeyWhenMappedCopyThrows) {
  chaistl::flat_map<int, throwing_copy> m;
  m.try_emplace(1, 10);
  m.try_emplace(3, 30);

  // Build the entry first, then arm copies: the throw happens inside
  // values_.insert, after the key has already been inserted.
  std::pair<int, throwing_copy> entry(2, throwing_copy(20));
  throwing_copy::armed = true;
  EXPECT_THROW(m.insert(entry), std::runtime_error);
  throwing_copy::armed = false;

  EXPECT_EQ(m.size(), 2uz);
  EXPECT_TRUE(m.validate());
  EXPECT_THAT(m.keys(), ElementsAre(1, 3));
}

TEST(FlatMapExceptionSafety, RangeInsertRestoresInvariantWhenComparatorThrows) {
  int budget = 1000;
  chaistl::flat_map<int, int, throwing_compare> m({{1, 10}, {5, 50}}, throwing_compare{&budget});

  std::vector<std::pair<int, int>> incoming{{4, 40}, {2, 20}, {3, 30}};
  budget = 1;
  EXPECT_THROW(m.insert(incoming.begin(), incoming.end()), std::runtime_error);
  budget = 1000;

  // A throw while normalizing the tail rolls back to the pre-insert state;
  // a throw later may clear. Either way the invariant must hold.
  EXPECT_TRUE(m.validate());
  EXPECT_THAT(m.keys(), ElementsAre(1, 5));
  EXPECT_EQ(m.at(1), 10);
}

TEST(FlatMapExceptionSafety, RangeInsertSucceedsAndStaysValidWithGenerousBudget) {
  int budget = 1000;
  chaistl::flat_map<int, int, throwing_compare> m({{1, 10}, {5, 50}}, throwing_compare{&budget});

  std::vector<std::pair<int, int>> incoming{{4, 40}, {2, 20}, {1, 999}};
  m.insert(incoming.begin(), incoming.end());

  EXPECT_TRUE(m.validate());
  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 4, 5));
  EXPECT_EQ(m.at(1), 10);  // existing mapped value wins over incoming duplicate
}

}  // namespace
