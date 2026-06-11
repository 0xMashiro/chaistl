// SPDX-License-Identifier: Apache-2.0

// References:
// - Heterogeneous lookup for unordered containers (C++20):
//   P0919R3 + P1690R1. The overloads participate in overload resolution only
//   when BOTH Hash::is_transparent and Pred::is_transparent denote types.
//   https://eel.is/c++draft/unord.req

#include <gtest/gtest.h>

#include <chaistl/containers/unordered_set.hpp>
#include <chaistl/functional/hash.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace {

// ============================================================================
// Instrumented key: transparent lookup is observable as "zero conversions".
// ============================================================================

struct Probe {
  int id = 0;
};

struct Key {
  int id = 0;
  static int conversions;

  explicit Key(int identifier) : id(identifier) {}

  // Implicit and counted: the non-transparent fallback path must pay here.
  Key(const Probe& probe) : id(probe.id) { ++conversions; }  // NOLINT(google-explicit-constructor)

  bool operator==(const Key& other) const { return id == other.id; }
};
int Key::conversions = 0;

struct PlainKeyHash {
  std::size_t operator()(const Key& key) const { return std::hash<int>{}(key.id); }
};

struct PlainKeyEq {
  bool operator()(const Key& lhs, const Key& rhs) const { return lhs.id == rhs.id; }
};

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

// ============================================================================
// Gating: a probe type with NO conversion to Key makes the overload's
// presence directly testable — both functors must be transparent (P1690).
// ============================================================================

struct InconvertibleProbe {
  int id = 0;
};

struct WideHash {
  using is_transparent = void;
  std::size_t operator()(const Key& key) const { return std::hash<int>{}(key.id); }
  std::size_t operator()(const InconvertibleProbe& probe) const { return std::hash<int>{}(probe.id); }
};

struct WideEq {
  using is_transparent = void;
  bool operator()(const Key& lhs, const Key& rhs) const { return lhs.id == rhs.id; }
  bool operator()(const Key& lhs, const InconvertibleProbe& rhs) const { return lhs.id == rhs.id; }
  bool operator()(const InconvertibleProbe& lhs, const Key& rhs) const { return lhs.id == rhs.id; }
};

template <class Set>
concept lookupable_by_inconvertible_probe = requires(const Set& set, const InconvertibleProbe& probe) {
  set.find(probe);
  set.count(probe);
  set.contains(probe);
  set.equal_range(probe);
};

// Both transparent: heterogeneous overloads exist.
static_assert(lookupable_by_inconvertible_probe<chaistl::unordered_set<Key, WideHash, WideEq>>);
// Hash transparent, Pred not: absent.
static_assert(!lookupable_by_inconvertible_probe<chaistl::unordered_set<Key, WideHash, PlainKeyEq>>);
// Pred transparent, Hash not: absent.
static_assert(!lookupable_by_inconvertible_probe<chaistl::unordered_set<Key, PlainKeyHash, WideEq>>);
// Neither: absent.
static_assert(!lookupable_by_inconvertible_probe<chaistl::unordered_set<Key, PlainKeyHash, PlainKeyEq>>);

// ============================================================================
// Behavior
// ============================================================================

TEST(UnorderedSetTransparent, LookupAvoidsKeyConversion) {
  chaistl::unordered_set<Key, TransparentKeyHash, TransparentKeyEq> set;
  set.insert(Key(1));
  set.insert(Key(2));

  Key::conversions = 0;
  EXPECT_TRUE(set.contains(Probe{1}));
  EXPECT_NE(set.find(Probe{2}), set.end());
  EXPECT_EQ(set.count(Probe{1}), 1u);
  EXPECT_FALSE(set.contains(Probe{99}));
  auto [first, last] = set.equal_range(Probe{2});
  EXPECT_NE(first, last);

  // The probe never became a Key.
  EXPECT_EQ(Key::conversions, 0);
}

TEST(UnorderedSetTransparent, NonTransparentFallbackConverts) {
  chaistl::unordered_set<Key, PlainKeyHash, PlainKeyEq> set;
  set.insert(Key(1));

  Key::conversions = 0;
  // Without transparent functors the Probe must convert to Key to use the
  // plain overload — the cost the transparent overloads exist to remove.
  EXPECT_TRUE(set.contains(Probe{1}));
  EXPECT_EQ(Key::conversions, 1);
}

TEST(UnorderedSetTransparent, HeterogeneousEraseAndExtract) {
  chaistl::unordered_set<Key, WideHash, WideEq> set;
  set.insert(Key(1));
  set.insert(Key(2));
  set.insert(Key(3));

  // P2077 (C++23): erase/extract through the probe, no Key construction.
  EXPECT_EQ(set.erase(InconvertibleProbe{1}), 1u);
  EXPECT_EQ(set.erase(InconvertibleProbe{99}), 0u);

  auto nh = set.extract(InconvertibleProbe{2});
  ASSERT_FALSE(nh.empty());
  EXPECT_EQ(nh.value().id, 2);
  EXPECT_TRUE(set.extract(InconvertibleProbe{99}).empty());

  EXPECT_EQ(set.size(), 1u);
  EXPECT_TRUE(set.contains(InconvertibleProbe{3}));
  EXPECT_TRUE(set.validate());
}

// The anti-hijack gate: passing an iterator must select erase(const_iterator)
// (returning iterator), never the heterogeneous erase (returning size_type).
using wide_set = chaistl::unordered_set<Key, WideHash, WideEq>;
static_assert(std::is_same_v<decltype(std::declval<wide_set&>().erase(std::declval<wide_set::const_iterator>())),
                             wide_set::iterator>);

template <class Set>
concept erasable_by_probe = requires(Set& set, const InconvertibleProbe& probe) {
  set.erase(probe);
  set.extract(probe);
};

static_assert(erasable_by_probe<wide_set>);
// Without transparent functors the heterogeneous overloads are absent.
static_assert(!erasable_by_probe<chaistl::unordered_set<Key, PlainKeyHash, PlainKeyEq>>);
static_assert(!erasable_by_probe<chaistl::unordered_set<Key, WideHash, PlainKeyEq>>);

TEST(UnorderedSetTransparent, HeterogeneousInsertConvertsOnlyOnMiss) {
  // P2363 (C++26-aligned extension): insert(K&&) hashes the probe directly
  // and constructs the Key only when the element is absent.
  chaistl::unordered_set<Key, TransparentKeyHash, TransparentKeyEq> set;
  set.insert(Key(1));

  Key::conversions = 0;
  auto [hit, hit_inserted] = set.insert(Probe{1});
  EXPECT_FALSE(hit_inserted);
  EXPECT_EQ(hit->id, 1);
  EXPECT_EQ(Key::conversions, 0);  // a hit never constructs a Key

  auto [miss, miss_inserted] = set.insert(Probe{2});
  EXPECT_TRUE(miss_inserted);
  EXPECT_EQ(miss->id, 2);
  EXPECT_EQ(Key::conversions, 1);  // a miss converts exactly once
  EXPECT_TRUE(set.validate());

  EXPECT_EQ(*set.insert(set.begin(), Probe{3}), Key(3));  // hint variant
}

TEST(UnorderedSetTransparent, HeterogeneousBucket) {
  chaistl::unordered_set<Key, TransparentKeyHash, TransparentKeyEq> set;
  set.insert(Key(1));

  const auto index = set.bucket(Probe{1});
  bool found = false;
  for (auto it = set.begin(index); it != set.end(index); ++it) {
    if (it->id == 1) {
      found = true;
    }
  }
  EXPECT_TRUE(found);
}

TEST(UnorderedSetTransparent, StringLookupByStringView) {
  struct StringHash {
    using is_transparent = void;
    std::size_t operator()(std::string_view value) const noexcept { return chaistl::hash<std::string_view>{}(value); }
  };
  chaistl::unordered_set<std::string, StringHash, std::equal_to<>> set{"apple", "banana"};

  using std::string_view_literals::operator""sv;
  EXPECT_TRUE(set.contains("apple"sv));
  EXPECT_TRUE(set.contains("banana"));  // const char* flows through string_view too
  EXPECT_FALSE(set.contains("cherry"sv));
  EXPECT_NE(set.find("apple"sv), set.end());
  EXPECT_EQ(set.count("banana"sv), 1u);
}

}  // namespace
