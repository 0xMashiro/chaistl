// SPDX-License-Identifier: Apache-2.0

// References:
// - P2363R5 (adopted into the C++26 working draft, 2023-06): heterogeneous
//   try_emplace / insert_or_assign / operator[] / at / bucket. Implemented
//   here as a documented C++26-aligned extension (ADR: Hash Table Design,
//   stage 6). The observable contract: the Key is constructed from the probe
//   only when an insertion actually happens.

#include <gtest/gtest.h>

#include <chaistl/containers/unordered_map.hpp>

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

struct Probe {
  int id = 0;
};

struct Key {
  int id = 0;
  static int conversions;

  explicit Key(int identifier) : id(identifier) {}
  Key(const Probe& probe) : id(probe.id) { ++conversions; }  // NOLINT(google-explicit-constructor)

  bool operator==(const Key& other) const { return id == other.id; }
};
int Key::conversions = 0;

struct TransparentKeyHash {
  using is_transparent = void;
  std::size_t operator()(const Key& key) const { return std::hash<int>{}(key.id); }
  std::size_t operator()(const Probe& probe) const { return std::hash<int>{}(probe.id); }
};

struct TransparentKeyEq {
  using is_transparent = void;
  bool operator()(const Key& lhs, const Key& rhs) const { return lhs.id == rhs.id; }
  bool operator()(const Key& lhs, const Probe& rhs) const { return lhs.id == rhs.id; }
  bool operator()(const Probe& lhs, const Key& rhs) const { return lhs.id == rhs.id; }
};

using probe_map = chaistl::unordered_map<Key, std::string, TransparentKeyHash, TransparentKeyEq>;

TEST(UnorderedMapTransparent, SubscriptConvertsOnlyOnMiss) {
  probe_map map;
  map.try_emplace(Key(1), "one");

  Key::conversions = 0;
  map[Probe{1}] += "-hit";  // existing entry: no Key constructed
  EXPECT_EQ(Key::conversions, 0);
  EXPECT_EQ(map.at(Key(1)), "one-hit");

  map[Probe{2}] = "two";  // miss: exactly one conversion
  EXPECT_EQ(Key::conversions, 1);
  EXPECT_EQ(map.size(), 2u);
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMapTransparent, TryEmplaceHitConstructsNothing) {
  probe_map map;
  map.try_emplace(Key(1), "one");

  Key::conversions = 0;
  auto [it, inserted] = map.try_emplace(Probe{1}, "ignored");
  EXPECT_FALSE(inserted);
  EXPECT_EQ(it->second, "one");
  EXPECT_EQ(Key::conversions, 0);

  auto [miss, miss_inserted] = map.try_emplace(Probe{2}, "two");
  EXPECT_TRUE(miss_inserted);
  EXPECT_EQ(miss->second, "two");
  EXPECT_EQ(Key::conversions, 1);
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMapTransparent, InsertOrAssignThroughProbe) {
  probe_map map;
  map.try_emplace(Key(1), "one");

  Key::conversions = 0;
  auto [hit, hit_inserted] = map.insert_or_assign(Probe{1}, "uno");
  EXPECT_FALSE(hit_inserted);
  EXPECT_EQ(hit->second, "uno");  // hit assigns
  EXPECT_EQ(Key::conversions, 0);

  auto [miss, miss_inserted] = map.insert_or_assign(Probe{2}, "two");
  EXPECT_TRUE(miss_inserted);
  EXPECT_EQ(miss->second, "two");
  EXPECT_EQ(Key::conversions, 1);
  EXPECT_TRUE(map.validate());
}

TEST(UnorderedMapTransparent, HeterogeneousAtAndBucket) {
  probe_map map;
  map.try_emplace(Key(1), "one");
  const auto& const_map = map;

  Key::conversions = 0;
  EXPECT_EQ(map.at(Probe{1}), "one");
  EXPECT_EQ(const_map.at(Probe{1}), "one");
  EXPECT_THROW((void)map.at(Probe{9}), std::out_of_range);
  EXPECT_THROW((void)const_map.at(Probe{9}), std::out_of_range);

  const auto index = const_map.bucket(Probe{1});
  bool found = false;
  for (auto it = const_map.begin(index); it != const_map.end(index); ++it) {
    if (it->first.id == 1) {
      found = true;
    }
  }
  EXPECT_TRUE(found);
  EXPECT_EQ(Key::conversions, 0);  // none of the above constructed a Key
}

// Gating: without transparent functors none of the P2363 overloads exist.
struct PlainKeyHash {
  std::size_t operator()(const Key& key) const { return std::hash<int>{}(key.id); }
};
struct PlainKeyEq {
  bool operator()(const Key& lhs, const Key& rhs) const { return lhs.id == rhs.id; }
};

struct InconvertibleProbe {
  int id = 0;
};

template <class Map>
concept heterogeneous_map_ops = requires(Map& map, const InconvertibleProbe& probe) {
  map.try_emplace(probe, "x");
  map.insert_or_assign(probe, "x");
  map[probe];
  map.at(probe);
  map.bucket(probe);
};

struct WideKeyHash {
  using is_transparent = void;
  std::size_t operator()(const Key& key) const { return std::hash<int>{}(key.id); }
  std::size_t operator()(const InconvertibleProbe& probe) const { return std::hash<int>{}(probe.id); }
};
struct WideKeyEq {
  using is_transparent = void;
  bool operator()(const Key& lhs, const Key& rhs) const { return lhs.id == rhs.id; }
  bool operator()(const Key& lhs, const InconvertibleProbe& rhs) const { return lhs.id == rhs.id; }
  bool operator()(const InconvertibleProbe& lhs, const Key& rhs) const { return lhs.id == rhs.id; }
};

// Note: a fully transparent map still needs Key constructible from the probe
// for the INSERTING overloads to compile when they miss; the gating checks
// below only assert overload PRESENCE, which is what P2363 constrains.
static_assert(!heterogeneous_map_ops<chaistl::unordered_map<Key, std::string, PlainKeyHash, PlainKeyEq>>);
static_assert(!heterogeneous_map_ops<chaistl::unordered_map<Key, std::string, WideKeyHash, PlainKeyEq>>);
static_assert(!heterogeneous_map_ops<chaistl::unordered_map<Key, std::string, PlainKeyHash, WideKeyEq>>);

}  // namespace
