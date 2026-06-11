// SPDX-License-Identifier: Apache-2.0

// References:
// - std::map element access:
//   https://en.cppreference.com/w/cpp/container/map
//   https://eel.is/c++draft/map.access

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/map.hpp>

#include <stdexcept>
#include <string>

namespace {

TEST(MapElementAccess, SubscriptInsert) {
  chaistl::map<int, std::string> m;

  m[1] = "one";
  EXPECT_EQ(m[1], "one");
  EXPECT_EQ(m.size(), 1uz);
  EXPECT_TRUE(m.validate());

  m[2] = "two";
  EXPECT_EQ(m[2], "two");
  EXPECT_EQ(m.size(), 2uz);
  EXPECT_TRUE(m.validate());
}

TEST(MapElementAccess, SubscriptUpdate) {
  chaistl::map<int, std::string> m{{1, "original"}};

  m[1] = "updated";
  EXPECT_EQ(m[1], "updated");
  EXPECT_EQ(m.size(), 1uz);
}

TEST(MapElementAccess, SubscriptDefaultConstruct) {
  chaistl::map<int, int> m;

  // Accessing non-existent key default-constructs the value
  EXPECT_EQ(m[42], 0);
  EXPECT_EQ(m.size(), 1uz);
  EXPECT_TRUE(m.contains(42));
}

TEST(MapElementAccess, AtExisting) {
  chaistl::map<int, std::string> m{{1, "one"}};

  EXPECT_EQ(m.at(1), "one");
}

TEST(MapElementAccess, AtNonExistingThrows) {
  chaistl::map<int, std::string> m;

  EXPECT_THROW(m.at(1), std::out_of_range);
}

TEST(MapElementAccess, AtConst) {
  const chaistl::map<int, std::string> m{{1, "one"}};

  EXPECT_EQ(m.at(1), "one");
  EXPECT_THROW(m.at(2), std::out_of_range);
}

}  // namespace
