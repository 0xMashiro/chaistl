// SPDX-License-Identifier: Apache-2.0

// References:
// - std::vector requirements and semantics:
//   https://en.cppreference.com/w/cpp/container/vector
//   https://eel.is/c++draft/vector
// - These typed tests intentionally run the same behavior against std::vector
//   and chaistl::vector. They are compatibility probes, not copied upstream
//   standard-library tests.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/vector.hpp>

#include <algorithm>
#include <array>
#include <iterator>
#include <ranges>
#include <sstream>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

template <class Vector>
class VectorCompatibilityTest : public ::testing::Test {};

using CompatibleIntVectors = ::testing::Types<std::vector<int>, chaistl::vector<int>>;
TYPED_TEST_SUITE(VectorCompatibilityTest, CompatibleIntVectors);

TYPED_TEST(VectorCompatibilityTest, DefaultAndInitializerListConstruction) {
  TypeParam empty;
  EXPECT_THAT(empty, IsEmpty());
  EXPECT_EQ(empty.size(), 0uz);
  EXPECT_EQ(empty.capacity(), 0uz);

  TypeParam values{1, 2, 3};
  EXPECT_THAT(values, ElementsAre(1, 2, 3));
  EXPECT_EQ(values.front(), 1);
  EXPECT_EQ(values.back(), 3);
}

TYPED_TEST(VectorCompatibilityTest, CountConstructionAndAssignment) {
  TypeParam empty_default(0);
  TypeParam empty_copied(0, 7);
  EXPECT_THAT(empty_default, IsEmpty());
  EXPECT_THAT(empty_copied, IsEmpty());

  TypeParam values(3, 7);
  EXPECT_THAT(values, ElementsAre(7, 7, 7));

  values.assign(2, 9);
  EXPECT_THAT(values, ElementsAre(9, 9));

  const std::array source{4, 5, 6};
  values.assign(source.begin(), source.end());
  EXPECT_THAT(values, ElementsAre(4, 5, 6));
}

TYPED_TEST(VectorCompatibilityTest, IteratorConstructionAcceptsForwardAndInputIterators) {
  const std::array source{4, 3, 2, 1};
  TypeParam from_forward(source.begin(), source.end());
  EXPECT_THAT(from_forward, ElementsAre(4, 3, 2, 1));

  std::istringstream input("4 5 6");
  std::istream_iterator<int> first(input);
  std::istream_iterator<int> last;
  TypeParam from_input(first, last);
  EXPECT_THAT(from_input, ElementsAre(4, 5, 6));
}

TYPED_TEST(VectorCompatibilityTest, CopyAndMoveOperationsPreserveValues) {
  TypeParam original{1, 2, 3};

  TypeParam copy(original);
  EXPECT_THAT(copy, ElementsAre(1, 2, 3));

  TypeParam moved(std::move(original));
  EXPECT_THAT(moved, ElementsAre(1, 2, 3));

  TypeParam assigned;
  assigned = copy;
  EXPECT_THAT(assigned, ElementsAre(1, 2, 3));

  // Route through a reference so the deliberate self-assignment does not
  // trigger -Wself-assign-overloaded.
  TypeParam& self = assigned;
  assigned = self;
  EXPECT_THAT(assigned, ElementsAre(1, 2, 3));

  assigned = {7, 8};
  EXPECT_THAT(assigned, ElementsAre(7, 8));

  TypeParam move_assigned;
  move_assigned = std::move(copy);
  EXPECT_THAT(move_assigned, ElementsAre(1, 2, 3));
}

TYPED_TEST(VectorCompatibilityTest, IteratorAndDataInterop) {
  TypeParam values{3, 1, 2};
  const auto& const_values = values;

  std::ranges::sort(values);

  EXPECT_THAT(values, ElementsAre(1, 2, 3));
  EXPECT_EQ(values.data(), std::to_address(values.begin()));
  EXPECT_EQ(const_values.data(), std::to_address(const_values.begin()));
  EXPECT_TRUE(std::ranges::contiguous_range<TypeParam>);
}

TYPED_TEST(VectorCompatibilityTest, ElementAccessAndReverseIterators) {
  TypeParam values{1, 2, 3};
  const auto& const_values = values;

  EXPECT_EQ(values[0], 1);
  EXPECT_EQ(const_values[0], 1);
  EXPECT_EQ(values.at(1), 2);
  EXPECT_EQ(const_values.at(1), 2);
  EXPECT_EQ(values.front(), 1);
  EXPECT_EQ(const_values.front(), 1);
  EXPECT_EQ(values.back(), 3);
  EXPECT_EQ(const_values.back(), 3);

  EXPECT_THROW((void)values.at(3), std::out_of_range);
  EXPECT_THROW((void)const_values.at(3), std::out_of_range);

  EXPECT_THAT(std::vector<int>(values.rbegin(), values.rend()), ElementsAre(3, 2, 1));
  EXPECT_EQ(*values.cbegin(), 1);
  EXPECT_EQ(*(values.cend() - 1), 3);
  EXPECT_EQ(*values.crbegin(), 3);
  EXPECT_EQ(*(values.crend() - 1), 1);
}

TYPED_TEST(VectorCompatibilityTest, CapacityOperationsDoNotChangeElements) {
  TypeParam values{1, 2, 3};
  const auto* original_data = values.data();
  const auto original_capacity = values.capacity();

  values.reserve(values.size());
  EXPECT_EQ(values.data(), original_data);
  EXPECT_EQ(values.capacity(), original_capacity);

  values.reserve(16);
  EXPECT_GE(values.capacity(), 16uz);
  EXPECT_THAT(values, ElementsAre(1, 2, 3));

  values.resize(5);
  EXPECT_THAT(values, ElementsAre(1, 2, 3, 0, 0));

  values.resize(2);
  EXPECT_THAT(values, ElementsAre(1, 2));

  values.resize(4, 9);
  EXPECT_THAT(values, ElementsAre(1, 2, 9, 9));

  values.resize(1, 8);
  EXPECT_THAT(values, ElementsAre(1));

  values.shrink_to_fit();
  EXPECT_THAT(values, ElementsAre(1));
  EXPECT_GE(values.capacity(), values.size());
}

TYPED_TEST(VectorCompatibilityTest, ModifiersPreserveStandardOrderSemantics) {
  TypeParam values{1, 4};

  auto inserted = values.insert(values.begin() + 1, 2);
  EXPECT_EQ(*inserted, 2);
  EXPECT_THAT(values, ElementsAre(1, 2, 4));

  values.insert(values.begin() + 2, 1, 3);
  EXPECT_THAT(values, ElementsAre(1, 2, 3, 4));

  values.insert(values.end(), {6, 7});
  EXPECT_THAT(values, ElementsAre(1, 2, 3, 4, 6, 7));

  values.emplace(values.end(), 5);
  EXPECT_THAT(values, ElementsAre(1, 2, 3, 4, 6, 7, 5));

  auto erased = values.erase(values.begin() + 1);
  EXPECT_EQ(*erased, 3);
  EXPECT_THAT(values, ElementsAre(1, 3, 4, 6, 7, 5));

  erased = values.erase(values.begin() + 1, values.begin() + 3);
  EXPECT_EQ(*erased, 6);
  EXPECT_THAT(values, ElementsAre(1, 6, 7, 5));

  erased = values.erase(values.begin() + 1, values.begin() + 1);
  EXPECT_EQ(*erased, 6);
  EXPECT_THAT(values, ElementsAre(1, 6, 7, 5));

  values.pop_back();
  EXPECT_THAT(values, ElementsAre(1, 6, 7));

  values.clear();
  EXPECT_THAT(values, IsEmpty());
}

TYPED_TEST(VectorCompatibilityTest, ComparisonOperatorsAreLexicographical) {
  EXPECT_EQ(TypeParam({1, 2}), TypeParam({1, 2}));
  EXPECT_LT(TypeParam({1, 2}), TypeParam({1, 3}));
}

}  // namespace
