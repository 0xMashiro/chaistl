// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_multiset.hpp>

#include <iterator>
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

TEST(FlatMultisetExceptionSafety, RangeInsertRestoresInvariantWhenComparatorThrows) {
  int budget = 1000;
  chaistl::flat_multiset<int, throwing_compare> s({1, 5}, throwing_compare{&budget});

  std::vector<int> incoming{4, 2, 3};
  budget = 1;
  EXPECT_THROW(s.insert(incoming.begin(), incoming.end()), std::runtime_error);
  budget = 1000;

  EXPECT_TRUE(s.validate());
  EXPECT_THAT(s, ElementsAre(1, 5));
}

TEST(FlatMultisetExceptionSafety, RangeInsertSucceedsAndStaysValidWithGenerousBudget) {
  int budget = 1000;
  chaistl::flat_multiset<int, throwing_compare> s({1, 2, 2, 5}, throwing_compare{&budget});

  std::vector<int> incoming{4, 2, 3, 2};
  s.insert(incoming.begin(), incoming.end());

  EXPECT_TRUE(s.validate());
  EXPECT_THAT(s, ElementsAre(1, 2, 2, 2, 2, 3, 4, 5));
  EXPECT_EQ(s.count(2), 4uz);
  auto [first, last] = s.equal_range(2);
  EXPECT_EQ(std::distance(first, last), 4);
}

}  // namespace
