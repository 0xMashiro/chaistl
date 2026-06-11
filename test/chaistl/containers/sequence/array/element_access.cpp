// SPDX-License-Identifier: Apache-2.0

// References:
// - std::array element access:
//   https://en.cppreference.com/w/cpp/container/array
//   https://eel.is/c++draft/array
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/containers/array.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

struct NoDefault {
  NoDefault() = delete;
  explicit NoDefault(int value) : value(value) {}

  int value;
};

struct CopyOnly {
  explicit CopyOnly(int value) : value(value) {}
  CopyOnly(const CopyOnly&) = default;
  CopyOnly(CopyOnly&&) = delete;

  int value;
};

}  // namespace

TEST(ArrayTest, ZeroSizedArrayHasEmptyIteratorRange) {
  chaistl::array<int, 0> values{};
  const auto& const_values = values;

  EXPECT_TRUE(values.empty());
  EXPECT_EQ(values.size(), 0uz);
  EXPECT_EQ(values.begin(), values.end());
  EXPECT_EQ(const_values.begin(), const_values.end());
  EXPECT_EQ(values.rbegin(), values.rend());
  EXPECT_THROW((void)values.at(0), std::out_of_range);
}

TEST(ArrayTest, ZeroSizedArrayDoesNotConstructDummyElement) {
  chaistl::array<NoDefault, 0> values{};
  const auto& const_values = values;

  EXPECT_TRUE(values.empty());
  EXPECT_EQ(values.size(), 0uz);
  EXPECT_EQ(values.data(), nullptr);
  EXPECT_EQ(const_values.data(), nullptr);
  EXPECT_EQ(values.begin(), values.end());
  EXPECT_EQ(const_values.begin(), const_values.end());
}

TEST(ArrayTest, BoolArrayUsesOrdinaryContiguousBoolStorage) {
  chaistl::array<bool, 3> flags{true, false, true};

  bool* raw = flags.data();
  raw[1] = true;

  EXPECT_TRUE(flags[0]);
  EXPECT_TRUE(flags[1]);
  EXPECT_TRUE(flags[2]);
  EXPECT_EQ(std::to_address(flags.begin()), raw);
}

TEST(ArrayTest, ToArrayCopiesAndMovesBuiltInArrays) {
  int values[] = {1, 2, 3};
  auto copied = chaistl::to_array(values);

  values[0] = 9;
  EXPECT_TRUE(std::ranges::equal(copied, std::array{1, 2, 3}));
  static_assert(std::same_as<decltype(copied), chaistl::array<int, 3>>);

  std::string words[] = {"red", "green"};
  auto moved = chaistl::to_array(std::move(words));

  EXPECT_TRUE(std::ranges::equal(moved, std::array<std::string, 2>{"red", "green"}));

  const CopyOnly copy_only_values[] = {CopyOnly(1), CopyOnly(2)};
  auto copy_only = chaistl::to_array(copy_only_values);

  EXPECT_EQ(copy_only[0].value, 1);
  EXPECT_EQ(copy_only[1].value, 2);
}
