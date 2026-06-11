// SPDX-License-Identifier: Apache-2.0

// References:
// - Lookup and equality for unordered maps; heterogeneous lookup gating is
//   identical to unordered_set (both functors transparent) and the shared
//   core is locked by the unordered_set suites — this file covers the
//   map-facing surface.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_map.hpp>
#include <chaistl/functional/hash.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {

TEST(UnorderedMapLookup, FindCountContains) {
  chaistl::unordered_map<int, std::string> map{{1, "one"}, {2, "two"}};
  const auto& const_map = map;

  EXPECT_EQ(map.find(1)->second, "one");
  EXPECT_EQ(const_map.find(2)->second, "two");
  EXPECT_EQ(map.find(9), map.end());
  EXPECT_EQ(map.count(1), 1u);
  EXPECT_TRUE(const_map.contains(2));
  EXPECT_FALSE(const_map.contains(9));
}

TEST(UnorderedMapLookup, TransparentLookupByStringView) {
  struct StringHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view value) const noexcept { return chaistl::hash<std::string_view>{}(value); }
  };
  chaistl::unordered_map<std::string, int, StringHash, std::equal_to<>> map{{"apple", 1}, {"banana", 2}};

  using std::string_view_literals::operator""sv;
  EXPECT_TRUE(map.contains("apple"sv));
  EXPECT_EQ(map.find("banana"sv)->second, 2);
  EXPECT_FALSE(map.contains("cherry"sv));
}

TEST(UnorderedMapLookup, EqualityComparesKeysAndMappedValues) {
  chaistl::unordered_map<int, int> a{{1, 10}, {2, 20}};
  chaistl::unordered_map<int, int> b{{2, 20}, {1, 10}};
  chaistl::unordered_map<int, int> c{{1, 10}, {2, 99}};  // same keys, different mapped

  b.rehash(100);
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

TEST(UnorderedMapLookup, OracleAgainstStd) {
  chaistl::unordered_map<int, int> ours;
  std::unordered_map<int, int> oracle;

  std::uint64_t state = 0x9E3779B97F4A7C15ull;
  auto next = [&state] {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
  };

  for (int step = 0; step < 1500; ++step) {
    const int key = static_cast<int>(next() % 150);
    switch (next() % 4) {
      case 0:
        EXPECT_EQ(ours.erase(key), oracle.erase(key));
        break;
      case 1:
        ours[key] = step;
        oracle[key] = step;
        break;
      case 2: {
        auto ours_result = ours.try_emplace(key, step);
        auto oracle_result = oracle.try_emplace(key, step);
        EXPECT_EQ(ours_result.second, oracle_result.second);
        break;
      }
      default:
        ours.insert_or_assign(key, step);
        oracle.insert_or_assign(key, step);
        break;
    }
    EXPECT_EQ(ours.size(), oracle.size());
    if (step % 250 == 0) {
      ASSERT_TRUE(ours.validate());
      for (const auto& [key_in_oracle, mapped] : oracle) {
        auto it = ours.find(key_in_oracle);
        ASSERT_NE(it, ours.end());
        ASSERT_EQ(it->second, mapped);
      }
    }
  }
  ASSERT_TRUE(ours.validate());
}

}  // namespace
