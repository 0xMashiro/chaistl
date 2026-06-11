// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_set.hpp>

#include <stdexcept>
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

TEST(FlatSetExceptionSafety, RangeInsertRestoresInvariantWhenComparatorThrows) {
  int budget = 1000;
  chaistl::flat_set<int, throwing_compare> s({1, 5}, throwing_compare{&budget});

  std::vector<int> incoming{4, 2, 3};
  budget = 1;
  EXPECT_THROW(s.insert(incoming.begin(), incoming.end()), std::runtime_error);
  budget = 1000;

  // A throw while sorting the appended tail rolls back to the pre-insert
  // state; a throw during the merge may clear. Either way the invariant holds.
  EXPECT_TRUE(s.validate());
  EXPECT_THAT(s, ElementsAre(1, 5));
}

TEST(FlatSetExceptionSafety, RangeInsertSucceedsAndStaysValidWithGenerousBudget) {
  int budget = 1000;
  chaistl::flat_set<int, throwing_compare> s({1, 5}, throwing_compare{&budget});

  std::vector<int> incoming{4, 2, 1};
  s.insert(incoming.begin(), incoming.end());

  EXPECT_TRUE(s.validate());
  EXPECT_THAT(s, ElementsAre(1, 2, 4, 5));
}

}  // namespace
