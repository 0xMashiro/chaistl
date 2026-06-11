// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/unordered_multiset.hpp>

#include <string>
#include <string_view>

namespace {

struct TransparentHash {
  using is_transparent = void;
  std::size_t operator()(std::string_view value) const { return std::hash<std::string_view>{}(value); }
};

struct TransparentEqual {
  using is_transparent = void;
  bool operator()(std::string_view lhs, std::string_view rhs) const { return lhs == rhs; }
};

TEST(UnorderedMultisetTransparent, LookupEraseAndInsert) {
  chaistl::unordered_multiset<std::string, TransparentHash, TransparentEqual> set;
  set.insert("alpha");
  set.insert(std::string_view("alpha"));
  set.insert("beta");

  EXPECT_EQ(set.count(std::string_view("alpha")), 2u);
  auto [first, last] = set.equal_range(std::string_view("alpha"));
  EXPECT_EQ(std::distance(first, last), 2);
  EXPECT_EQ(set.erase(std::string_view("alpha")), 2u);
  EXPECT_FALSE(set.contains(std::string_view("alpha")));
  EXPECT_TRUE(set.validate());
}

}  // namespace
