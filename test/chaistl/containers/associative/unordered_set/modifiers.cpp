// SPDX-License-Identifier: Apache-2.0

// References:
// - std::unordered_set modifiers:
//   https://en.cppreference.com/w/cpp/container/unordered_set
//   https://eel.is/c++draft/unord.req
// - Iterator/reference invalidation: rehash invalidates iterators but not
//   references or pointers to elements ([unord.req.general]).

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/unordered_set.hpp>

#include <cstddef>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace {

using ::testing::UnorderedElementsAre;

struct MoveOnly {
  int value = 0;

  explicit MoveOnly(int v) : value(v) {}
  MoveOnly(MoveOnly&&) = default;
  MoveOnly& operator=(MoveOnly&&) = default;
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly& operator=(const MoveOnly&) = delete;

  bool operator==(const MoveOnly& other) const { return value == other.value; }
};

struct MoveOnlyHash {
  std::size_t operator()(const MoveOnly& m) const { return std::hash<int>{}(m.value); }
};

TEST(UnorderedSetModifiers, InsertUnique) {
  chaistl::unordered_set<int> set;

  auto [it, inserted] = set.insert(1);

  EXPECT_TRUE(inserted);
  EXPECT_EQ(*it, 1);
  EXPECT_EQ(set.size(), 1u);
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetModifiers, InsertDuplicateReturnsExisting) {
  chaistl::unordered_set<int> set{1};
  const auto existing = set.find(1);

  auto [it, inserted] = set.insert(1);

  EXPECT_FALSE(inserted);
  EXPECT_EQ(it, existing);
  EXPECT_EQ(set.size(), 1u);
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetModifiers, InsertMoveOnly) {
  chaistl::unordered_set<MoveOnly, MoveOnlyHash> set;

  auto [it, inserted] = set.insert(MoveOnly(7));

  EXPECT_TRUE(inserted);
  EXPECT_EQ(it->value, 7);
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetModifiers, InsertHintIsAcceptedAndIgnored) {
  chaistl::unordered_set<int> set{1};

  auto it = set.insert(set.begin(), 2);

  EXPECT_EQ(*it, 2);
  EXPECT_THAT(set, UnorderedElementsAre(1, 2));
}

TEST(UnorderedSetModifiers, InsertRangeAndInitializerList) {
  chaistl::unordered_set<int> set;
  const int values[] = {1, 2, 2, 3};

  set.insert(std::begin(values), std::end(values));
  set.insert({3, 4});

  EXPECT_THAT(set, UnorderedElementsAre(1, 2, 3, 4));
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetModifiers, EmplaceConstructsInPlace) {
  chaistl::unordered_set<std::string> set;

  auto [it, inserted] = set.emplace(3u, 'a');  // std::string(3, 'a')

  EXPECT_TRUE(inserted);
  EXPECT_EQ(*it, "aaa");
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetModifiers, EmplaceDuplicateLeavesSetUnchanged) {
  chaistl::unordered_set<int> set{5};

  auto [it, inserted] = set.emplace(5);

  EXPECT_FALSE(inserted);
  EXPECT_EQ(*it, 5);
  EXPECT_EQ(set.size(), 1u);
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetModifiers, EmplaceHintIsAcceptedAndIgnored) {
  chaistl::unordered_set<int> set;

  auto it = set.emplace_hint(set.end(), 42);

  EXPECT_EQ(*it, 42);
}

TEST(UnorderedSetModifiers, EraseByIteratorReturnsNext) {
  chaistl::unordered_set<int> set{1, 2, 3, 4, 5};

  // Drain the whole container through the returned iterator: the canonical
  // erase-while-iterating pattern must terminate and visit every element.
  std::size_t erased = 0;
  for (auto it = set.begin(); it != set.end();) {
    it = set.erase(it);
    ++erased;
    EXPECT_TRUE(set.validate());
  }

  EXPECT_EQ(erased, 5u);
  EXPECT_TRUE(set.empty());
}

TEST(UnorderedSetModifiers, EraseByKey) {
  chaistl::unordered_set<int> set{1, 2};

  EXPECT_EQ(set.erase(1), 1u);
  EXPECT_EQ(set.erase(99), 0u);
  EXPECT_THAT(set, UnorderedElementsAre(2));
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetModifiers, EraseIteratorRange) {
  chaistl::unordered_set<int> set{1, 2, 3, 4};

  auto first = set.begin();
  auto last = first;
  ++last;
  ++last;
  auto result = set.erase(first, last);

  EXPECT_EQ(result, set.begin());
  EXPECT_EQ(set.size(), 2u);
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetModifiers, ClearDestroysElementsAndKeepsWorking) {
  chaistl::unordered_set<int> set{1, 2, 3};
  const auto buckets_before = set.bucket_count();

  set.clear();

  EXPECT_TRUE(set.empty());
  EXPECT_EQ(set.begin(), set.end());
  // Documented implementation behavior: clear() retains the bucket array.
  EXPECT_EQ(set.bucket_count(), buckets_before);

  set.insert(7);
  EXPECT_THAT(set, UnorderedElementsAre(7));
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetModifiers, SwapExchangesContents) {
  chaistl::unordered_set<int> a{1, 2};
  chaistl::unordered_set<int> b{3};

  a.swap(b);
  EXPECT_THAT(a, UnorderedElementsAre(3));
  EXPECT_THAT(b, UnorderedElementsAre(1, 2));

  swap(a, b);  // free function
  EXPECT_THAT(a, UnorderedElementsAre(1, 2));
  EXPECT_THAT(b, UnorderedElementsAre(3));
  EXPECT_TRUE(a.validate());
  EXPECT_TRUE(b.validate());
}

TEST(UnorderedSetModifiers, SwapPreservesIterators) {
  chaistl::unordered_set<int> a{1, 2};
  chaistl::unordered_set<int> b{3};
  const auto it = a.find(2);  // refers to an element of a

  a.swap(b);

  // [container.reqmts]: iterators to elements keep referring to the same
  // elements, which now belong to the other container.
  EXPECT_EQ(*it, 2);
  EXPECT_EQ(it, b.find(2));
}

TEST(UnorderedSetModifiers, GrowthDoesNotInvalidateReferences) {
  chaistl::unordered_set<int> set;
  set.insert(0);
  const int* stable = &*set.find(0);

  for (int value = 1; value < 1000; ++value) {
    set.insert(value);  // forces several rehashes
  }

  // Rehash invalidates iterators but never references or pointers.
  EXPECT_EQ(*stable, 0);
  EXPECT_EQ(stable, &*set.find(0));
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetModifiers, InsertRange) {
  chaistl::unordered_set<int> set{1};
  const std::vector<int> values{1, 2, 3};

  set.insert_range(values);
  EXPECT_THAT(set, UnorderedElementsAre(1, 2, 3));

  set.insert_range(values | std::views::transform([](int v) {
                     return v + 10;
                   }));
  EXPECT_THAT(set, UnorderedElementsAre(1, 2, 3, 11, 12, 13));
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetModifiers, InsertRangeOfConvertibleElements) {
  // Convertible-but-not-value_type elements must take the construct-first
  // path: the key-first path would bind its key reference to a conversion
  // temporary (regression guard for the dangling-key hazard).
  chaistl::unordered_set<std::string> set;
  const std::vector<const char*> words{"a", "bb", "a"};

  set.insert_range(words);

  EXPECT_THAT(set, UnorderedElementsAre("a", "bb"));
  EXPECT_TRUE(set.validate());
}

TEST(UnorderedSetModifiers, EraseIf) {
  chaistl::unordered_set<int> set{1, 2, 3, 4, 5, 6};

  const auto removed = chaistl::erase_if(set, [](int value) {
    return value % 2 == 0;
  });

  EXPECT_EQ(removed, 3u);
  EXPECT_THAT(set, UnorderedElementsAre(1, 3, 5));
  EXPECT_TRUE(set.validate());
}

}  // namespace
