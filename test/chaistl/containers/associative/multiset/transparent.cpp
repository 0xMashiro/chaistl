// SPDX-License-Identifier: Apache-2.0

// References:
// - Heterogeneous lookup (transparent comparators):
//   https://en.cppreference.com/w/cpp/container/multiset/find
//   https://eel.is/c++draft/associative.reqmts — overloads constrained on
//   Compare::is_transparent
// - P2077: heterogeneous erasure / extraction

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/multiset.hpp>

#include <functional>
#include <string>
#include <string_view>

namespace {

using ::testing::ElementsAre;

using TransparentMultiset = chaistl::multiset<std::string, std::less<>>;

TEST(MultisetTransparent, LookupByStringView) {
  TransparentMultiset ms{"a", "b", "b", "c"};
  constexpr std::string_view key = "b";

  EXPECT_EQ(ms.count(key), 2uz);
  EXPECT_TRUE(ms.contains(key));
  EXPECT_NE(ms.find(key), ms.end());

  auto [first, last] = ms.equal_range(key);
  EXPECT_EQ(std::distance(first, last), 2);
  EXPECT_EQ(*ms.lower_bound(key), "b");
  EXPECT_EQ(*ms.upper_bound(key), "c");
}

TEST(MultisetTransparent, EraseByViewRemovesRun) {
  TransparentMultiset ms{"a", "b", "b", "c"};

  EXPECT_EQ(ms.erase(std::string_view{"b"}), 2uz);
  EXPECT_THAT(ms, ElementsAre("a", "c"));
  EXPECT_TRUE(ms.validate());
}

TEST(MultisetTransparent, ExtractByView) {
  TransparentMultiset ms{"x", "x"};

  auto nh = ms.extract(std::string_view{"x"});
  ASSERT_FALSE(nh.empty());
  EXPECT_EQ(nh.value(), "x");
  EXPECT_THAT(ms, ElementsAre("x"));
}

// Probed through a concept: in a non-dependent context a failing
// requires-expression is a hard error, not false.
template <class Container>
concept counts_string_view = requires(Container container) { container.count(std::string_view{"k"}); };

// Non-transparent comparator: heterogeneous overloads do not participate.
TEST(MultisetTransparent, NotAvailableWithoutIsTransparent) {
  static_assert(!counts_string_view<chaistl::multiset<std::string>>);
  static_assert(counts_string_view<TransparentMultiset>);
}

}  // namespace
