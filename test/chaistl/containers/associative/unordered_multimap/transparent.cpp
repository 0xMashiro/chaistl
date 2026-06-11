// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/containers/unordered_multimap.hpp>

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

TEST(UnorderedMultimapTransparent, LookupEraseAndExtract) {
  chaistl::unordered_multimap<std::string, int, TransparentHash, TransparentEqual> map{
      {"alpha", 1}, {"alpha", 2}, {"beta", 3}};

  EXPECT_EQ(map.count(std::string_view("alpha")), 2u);
  auto [first, last] = map.equal_range(std::string_view("alpha"));
  EXPECT_EQ(std::distance(first, last), 2);
  auto nh = map.extract(std::string_view("beta"));
  ASSERT_FALSE(nh.empty());
  EXPECT_EQ(nh.mapped(), 3);
  EXPECT_EQ(map.erase(std::string_view("alpha")), 2u);
  EXPECT_TRUE(map.validate());
}

}  // namespace
