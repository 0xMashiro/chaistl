// SPDX-License-Identifier: Apache-2.0

// References:
// - try_emplace / insert_or_assign ([unord.map.modifiers]): a try_emplace hit
//   constructs nothing and must not consume the mapped arguments; an
//   insert_or_assign hit assigns (and so does consume).

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_map.hpp>

#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

// A move-observable mapped type: moved-from objects are marked.
struct Tattletale {
  int value = 0;

  explicit Tattletale(int v) : value(v) {}
  Tattletale(Tattletale&& other) noexcept : value(std::exchange(other.value, -1)) {}
  Tattletale& operator=(Tattletale&& other) noexcept {
    value = std::exchange(other.value, -1);
    return *this;
  }
  Tattletale(const Tattletale&) = default;
  Tattletale& operator=(const Tattletale&) = default;
};

TEST(UnorderedMapModifiers, InsertPair) {
  chaistl::unordered_map<int, std::string> map;

  auto [it, inserted] = map.insert({1, "one"});
  EXPECT_TRUE(inserted);
  EXPECT_EQ(it->second, "one");

  auto [dup, dup_inserted] = map.insert({1, "ignored"});
  EXPECT_FALSE(dup_inserted);
  EXPECT_EQ(dup->second, "one");  // duplicate insert never overwrites
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMapModifiers, InsertNonConstKeyPairTakesConstructFirstPath) {
  chaistl::unordered_map<int, std::string> map;
  // pair<int, string> is convertible to, but not, the value_type
  // pair<const int, string> — the dangling-key regression class.
  std::pair<int, std::string> loose{1, "one"};

  map.insert(loose);
  map.insert(std::move(loose));

  EXPECT_EQ(map.size(), 1u);
  EXPECT_EQ(map.at(1), "one");
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMapModifiers, EmplacePiecewise) {
  chaistl::unordered_map<int, std::string> map;

  auto [it, inserted] = map.emplace(std::piecewise_construct, std::forward_as_tuple(1), std::forward_as_tuple(3u, 'x'));

  EXPECT_TRUE(inserted);
  EXPECT_EQ(it->second, "xxx");
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMapModifiers, TryEmplaceHitDoesNotConsumeArguments) {
  chaistl::unordered_map<int, Tattletale> map;
  map.try_emplace(1, Tattletale(10));

  Tattletale probe(99);
  auto [it, inserted] = map.try_emplace(1, std::move(probe));

  EXPECT_FALSE(inserted);
  EXPECT_EQ(it->second.value, 10);  // existing mapped value untouched
  EXPECT_EQ(probe.value, 99);       // and the argument was NOT moved from
}

TEST(UnorderedMapModifiers, TryEmplaceMissConstructsFromArguments) {
  chaistl::unordered_map<int, Tattletale> map;

  Tattletale probe(7);
  auto [it, inserted] = map.try_emplace(2, std::move(probe));

  EXPECT_TRUE(inserted);
  EXPECT_EQ(it->second.value, 7);
  EXPECT_EQ(probe.value, -1);  // miss: the argument was consumed
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMapModifiers, InsertOrAssign) {
  chaistl::unordered_map<int, Tattletale> map;

  auto [it, inserted] = map.insert_or_assign(1, Tattletale(10));
  EXPECT_TRUE(inserted);
  EXPECT_EQ(it->second.value, 10);

  // Hit: assigns — in contrast to try_emplace, the argument IS consumed.
  Tattletale replacement(20);
  auto [hit, hit_inserted] = map.insert_or_assign(1, std::move(replacement));
  EXPECT_FALSE(hit_inserted);
  EXPECT_EQ(hit->second.value, 20);
  EXPECT_EQ(replacement.value, -1);
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMapModifiers, EraseAndEraseIf) {
  chaistl::unordered_map<int, int> map{{1, 10}, {2, 20}, {3, 30}, {4, 40}};

  EXPECT_EQ(map.erase(1), 1u);
  EXPECT_EQ(map.erase(99), 0u);

  const auto removed = chaistl::erase_if(map, [](const auto& entry) {
    return entry.second >= 30;
  });
  EXPECT_EQ(removed, 2u);
  EXPECT_THAT(map, UnorderedElementsAre(Pair(2, 20)));
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMapModifiers, InsertRange) {
  chaistl::unordered_map<int, int> map;
  // pair<int, int> elements: convertible, not value_type.
  const std::vector<std::pair<int, int>> entries{{1, 10}, {2, 20}, {1, 99}};

  map.insert_range(entries);

  EXPECT_THAT(map, UnorderedElementsAre(Pair(1, 10), Pair(2, 20)));
  EXPECT_TRUE(map.validate());
}

}  // namespace
