// SPDX-License-Identifier: Apache-2.0

// References:
// - operator[] / at:
//   https://eel.is/c++draft/unord.map.elem
//   operator[] default-constructs the mapped value on a miss; at throws
//   std::out_of_range.

#include <gtest/gtest.h>

#include <chaistl/containers/unordered_map.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace {

TEST(UnorderedMapElementAccess, SubscriptMissDefaultConstructs) {
  chaistl::unordered_map<int, std::string> map;

  std::string& mapped = map[1];

  EXPECT_TRUE(mapped.empty());
  EXPECT_EQ(map.size(), 1u);
  EXPECT_TRUE(map.contains(1));
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMapElementAccess, SubscriptHitReturnsMutableReference) {
  chaistl::unordered_map<int, std::string> map{{1, "one"}};

  map[1] = "uno";

  EXPECT_EQ(map.size(), 1u);
  EXPECT_EQ(map.at(1), "uno");
}

TEST(UnorderedMapElementAccess, SubscriptWithRvalueKey) {
  chaistl::unordered_map<std::string, int> map;

  std::string key = "movable";
  map[std::move(key)] = 7;

  EXPECT_EQ(map.at("movable"), 7);
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMapElementAccess, AtHitAndMiss) {
  chaistl::unordered_map<int, std::string> map{{1, "one"}};
  const auto& const_map = map;

  EXPECT_EQ(map.at(1), "one");
  EXPECT_EQ(const_map.at(1), "one");

  map.at(1) = "uno";  // mutable through at
  EXPECT_EQ(const_map.at(1), "uno");

  EXPECT_THROW((void)map.at(2), std::out_of_range);
  EXPECT_THROW((void)const_map.at(2), std::out_of_range);
  // A failed at changed nothing.
  EXPECT_EQ(map.size(), 1u);
}

}  // namespace
