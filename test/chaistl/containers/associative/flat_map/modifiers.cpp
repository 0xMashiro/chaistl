// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_map.hpp>

#include <string>
#include <utility>
#include <vector>

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(FlatMapModifiers, InsertReturnsPositionAndInsertedFlag) {
  chaistl::flat_map<int, std::string> m{{1, "one"}, {3, "three"}};

  auto [it1, inserted1] = m.insert({2, "two"});
  EXPECT_TRUE(inserted1);
  EXPECT_EQ(it1->first, 2);
  EXPECT_EQ(it1->second, "two");

  auto [it2, inserted2] = m.insert({2, "duplicate"});
  EXPECT_FALSE(inserted2);
  EXPECT_EQ(it2->second, "two");
  EXPECT_TRUE(m.validate());
}

TEST(FlatMapModifiers, HintInsertUsesValidHint) {
  chaistl::flat_map<int, std::string> m{{1, "one"}, {3, "three"}};

  auto it = m.insert(m.end(), {5, "five"});

  EXPECT_EQ(it->first, 5);
  EXPECT_THAT(m.keys(), ElementsAre(1, 3, 5));
  EXPECT_TRUE(m.validate());
}

TEST(FlatMapModifiers, EmplaceTryEmplaceAndInsertOrAssign) {
  chaistl::flat_map<int, std::string> m;

  auto [emplace_it, emplace_inserted] = m.emplace(2, "two");
  EXPECT_TRUE(emplace_inserted);
  EXPECT_EQ(emplace_it->second, "two");

  auto [try_it, try_inserted] = m.try_emplace(1, "one");
  EXPECT_TRUE(try_inserted);
  EXPECT_EQ(try_it->second, "one");

  auto [duplicate_it, duplicate_inserted] = m.try_emplace(1, "duplicate");
  EXPECT_FALSE(duplicate_inserted);
  EXPECT_EQ(duplicate_it->second, "one");

  auto [assign_it, assign_inserted] = m.insert_or_assign(1, "updated");
  EXPECT_FALSE(assign_inserted);
  EXPECT_EQ(assign_it->second, "updated");
  EXPECT_THAT(m.keys(), ElementsAre(1, 2));
  EXPECT_TRUE(m.validate());
}

TEST(FlatMapModifiers, HintedTryEmplaceAndInsertOrAssign) {
  chaistl::flat_map<int, std::string> m{{1, "one"}, {4, "four"}};

  auto try_it = m.try_emplace(m.lower_bound(4), 2, "two");
  EXPECT_EQ(try_it->first, 2);
  EXPECT_EQ(try_it->second, "two");

  auto duplicate_it = m.try_emplace(m.begin(), 1, "duplicate");
  EXPECT_EQ(duplicate_it->second, "one");

  auto assign_it = m.insert_or_assign(m.lower_bound(4), 3, "three");
  EXPECT_EQ(assign_it->second, "three");

  auto reassign_it = m.insert_or_assign(m.end(), 4, "FOUR");
  EXPECT_EQ(reassign_it->second, "FOUR");

  // A wrong hint must still produce the correct result.
  auto wrong_hint_it = m.try_emplace(m.end(), 0, "zero");
  EXPECT_EQ(wrong_hint_it->first, 0);

  EXPECT_THAT(m.keys(), ElementsAre(0, 1, 2, 3, 4));
  EXPECT_TRUE(m.validate());
}

TEST(FlatMapModifiers, InsertRangePreservesFirstValueForDuplicateKeys) {
  chaistl::flat_map<int, std::string> m{{1, "one"}};
  const std::vector<std::pair<int, std::string>> values{{3, "three"}, {2, "two"}, {3, "duplicate"}};

  m.insert_range(values);

  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 3));
  EXPECT_EQ(m.at(3), "three");
  EXPECT_TRUE(m.validate());
}

TEST(FlatMapModifiers, SortedUniqueInsertRange) {
  chaistl::flat_map<int, std::string> m{{1, "one"}};
  const std::vector<std::pair<int, std::string>> values{{2, "two"}, {3, "three"}};

  m.insert(chaistl::sorted_unique, values.begin(), values.end());

  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 3));
  EXPECT_EQ(m.at(3), "three");
  EXPECT_TRUE(m.validate());
}

TEST(FlatMapModifiers, SortedUniqueInsertRangeObject) {
  chaistl::flat_map<int, std::string> m{{1, "one"}};
  const std::vector<std::pair<int, std::string>> values{{2, "two"}, {3, "three"}};

  m.insert_range(chaistl::sorted_unique, values);

  EXPECT_THAT(m.keys(), ElementsAre(1, 2, 3));
  EXPECT_EQ(m.at(3), "three");
  EXPECT_TRUE(m.validate());
}

TEST(FlatMapModifiers, EraseByPositionRangeAndKey) {
  chaistl::flat_map<int, std::string> m{{1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}};

  auto next = m.erase(m.find(2));
  EXPECT_EQ(next->first, 3);
  EXPECT_EQ(m.erase(4), 1uz);
  EXPECT_EQ(m.erase(99), 0uz);
  m.erase(m.begin(), m.end());

  EXPECT_THAT(m, IsEmpty());
  EXPECT_TRUE(m.validate());
}

TEST(FlatMapModifiers, ExtractAndReplace) {
  chaistl::flat_map<int, std::string> m{{1, "one"}, {2, "two"}};

  auto storage = std::move(m).extract();
  EXPECT_THAT(m, IsEmpty());
  EXPECT_THAT(storage.keys, ElementsAre(1, 2));
  EXPECT_THAT(storage.values, ElementsAre("one", "two"));

  storage.keys = {3, 4};
  storage.values = {"three", "four"};
  m.replace(std::move(storage));

  EXPECT_THAT(m.keys(), ElementsAre(3, 4));
  EXPECT_EQ(m.at(4), "four");
  EXPECT_TRUE(m.validate());

  chaistl::vector<int> keys{5, 6};
  chaistl::vector<std::string> values{"five", "six"};
  m.replace(std::move(keys), std::move(values));

  EXPECT_THAT(m.keys(), ElementsAre(5, 6));
  EXPECT_EQ(m.at(6), "six");
  EXPECT_TRUE(m.validate());
}

TEST(FlatMapModifiers, EraseIfRemovesMatchingElements) {
  chaistl::flat_map<int, std::string> m{{1, "one"}, {2, "two"}, {3, "three"}};

  const auto removed = erase_if(m, [](const auto& value) {
    return value.first % 2 == 1;
  });

  EXPECT_EQ(removed, 2uz);
  EXPECT_THAT(m.keys(), ElementsAre(2));
}

TEST(FlatMapModifiers, ComparisonOperators) {
  const chaistl::flat_map<int, std::string> lhs{{1, "one"}, {2, "two"}};
  const chaistl::flat_map<int, std::string> same{{1, "one"}, {2, "two"}};
  const chaistl::flat_map<int, std::string> greater{{1, "one"}, {3, "three"}};

  EXPECT_EQ(lhs, same);
  EXPECT_LT(lhs, greater);
}

}  // namespace
