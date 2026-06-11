// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/flat_map.hpp>

#include <stdexcept>
#include <string>

namespace {

TEST(FlatMapElementAccess, SubscriptInsertAndUpdate) {
  chaistl::flat_map<int, std::string> m;

  m[2] = "two";
  m[1] = "one";
  m[2] = "updated";

  EXPECT_EQ(m.size(), 2uz);
  EXPECT_EQ(m[1], "one");
  EXPECT_EQ(m[2], "updated");
  EXPECT_TRUE(m.validate());
}

TEST(FlatMapElementAccess, AtThrowsForMissingKey) {
  chaistl::flat_map<int, std::string> m{{1, "one"}};

  EXPECT_EQ(m.at(1), "one");
  EXPECT_THROW((void)m.at(2), std::out_of_range);
}

TEST(FlatMapElementAccess, ConstAt) {
  const chaistl::flat_map<int, std::string> m{{1, "one"}};

  EXPECT_EQ(m.at(1), "one");
  EXPECT_THROW((void)m.at(2), std::out_of_range);
}

}  // namespace
