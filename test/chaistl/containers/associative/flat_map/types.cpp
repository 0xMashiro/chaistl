// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_map.hpp>

#include <algorithm>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using ::testing::ElementsAre;

// Mapped type whose move constructor throws once a configurable number of
// moves is reached. Used to sweep every possible throw point of a bulk insert.
struct counted_throwing_move {
  static inline int budget = -1;  // -1 disables throwing
  int value = 0;

  counted_throwing_move() = default;
  explicit counted_throwing_move(int v) : value(v) {}
  counted_throwing_move(const counted_throwing_move& other) : value(other.value) {}
  counted_throwing_move(counted_throwing_move&& other) : value(other.value) {
    if (budget >= 0 && --budget < 0) {
      throw std::runtime_error("move budget exhausted");
    }
  }
  counted_throwing_move& operator=(const counted_throwing_move& other) = default;
  counted_throwing_move& operator=(counted_throwing_move&& other) {
    value = other.value;
    if (budget >= 0 && --budget < 0) {
      throw std::runtime_error("move budget exhausted");
    }
    return *this;
  }
};

TEST(FlatMapTypes, MoveOnlyMappedType) {
  chaistl::flat_map<int, std::unique_ptr<int>> m;

  m.try_emplace(2, std::make_unique<int>(20));
  m.try_emplace(1, std::make_unique<int>(10));
  m.insert({3, std::make_unique<int>(30)});

  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 3));
  EXPECT_EQ(*m.at(2), 20);

  m.insert_or_assign(2, std::make_unique<int>(22));
  EXPECT_EQ(*m.at(2), 22);

  auto storage = std::move(m).extract();
  EXPECT_EQ(*storage.values[1], 22);
  EXPECT_TRUE(m.empty());
}

TEST(FlatMapTypes, MoveOnlyMappedBulkInsert) {
  chaistl::flat_map<int, std::unique_ptr<int>> m;
  m.try_emplace(5, std::make_unique<int>(50));

  std::vector<std::pair<int, std::unique_ptr<int>>> incoming;
  incoming.emplace_back(2, std::make_unique<int>(20));
  incoming.emplace_back(8, std::make_unique<int>(80));

  m.insert_range(std::views::as_rvalue(incoming));

  EXPECT_THAT(m.keys(), ElementsAre(2, 5, 8));
  EXPECT_EQ(*m.at(8), 80);
  EXPECT_TRUE(m.validate());
}

// Sweep every throw point: whatever step of the bulk insert throws, the
// invariant must hold and the content must be either the original or cleared.
TEST(FlatMapTypes, ThrowingMoveSweepKeepsInvariant) {
  for (int budget = 0; budget < 64; ++budget) {
    chaistl::flat_map<int, counted_throwing_move> m;
    m.try_emplace(1, counted_throwing_move(10));
    m.try_emplace(5, counted_throwing_move(50));

    const std::vector<std::pair<int, counted_throwing_move>> incoming{
        {4, counted_throwing_move(40)}, {2, counted_throwing_move(20)}, {4, counted_throwing_move(44)}};

    counted_throwing_move::budget = budget;
    bool threw = false;
    try {
      m.insert(incoming.begin(), incoming.end());
    } catch (const std::runtime_error&) {
      threw = true;
    }
    counted_throwing_move::budget = -1;

    ASSERT_TRUE(m.validate()) << "budget=" << budget;
    if (threw) {
      const bool rolled_back = m.size() == 2 && m.contains(1) && m.contains(5);
      ASSERT_TRUE(rolled_back || m.empty()) << "budget=" << budget << " size=" << m.size();
    } else {
      ASSERT_EQ(m.size(), 4uz) << "budget=" << budget;
      ASSERT_EQ(m.at(4).value, 40) << "budget=" << budget;
      break;  // enough budget to finish: later sweeps are identical
    }
  }
}

TEST(FlatMapTypes, StatefulComparatorPropagatesOnSwap) {
  struct offset_less {
    int id = 0;
    bool operator()(int lhs, int rhs) const { return lhs < rhs; }
  };

  chaistl::flat_map<int, int, offset_less> a({{1, 10}}, offset_less{1});
  chaistl::flat_map<int, int, offset_less> b({{2, 20}}, offset_less{2});

  a.swap(b);

  EXPECT_EQ(a.key_comp().id, 2);
  EXPECT_EQ(b.key_comp().id, 1);
  EXPECT_TRUE(a.contains(2));
  EXPECT_TRUE(b.contains(1));
}

TEST(FlatMapTypes, ProxyIteratorWorksWithRangesAlgorithms) {
  chaistl::flat_map<int, std::string> m{{1, "one"}, {2, "two"}, {3, "three"}};

  const auto found = std::ranges::find_if(m, [](const auto& entry) {
    return entry.second == "two";
  });
  ASSERT_NE(found, m.end());
  EXPECT_EQ(found->first, 2);

  const auto count = std::ranges::count_if(m, [](const auto& entry) {
    return entry.first > 1;
  });
  EXPECT_EQ(count, 2);

  EXPECT_TRUE(std::ranges::is_sorted(m, {}, [](const auto& entry) {
    return entry.first;
  }));

  // Mutation through the proxy reference must reach the underlying storage.
  std::ranges::for_each(m, [](auto entry) {
    entry.second += "!";
  });
  EXPECT_EQ(m.at(1), "one!");
}

}  // namespace
