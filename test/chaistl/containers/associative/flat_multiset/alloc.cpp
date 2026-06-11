// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_multiset.hpp>

#include <cstddef>
#include <iterator>
#include <memory>
#include <vector>

namespace {

using ::testing::ElementsAre;

template <class T>
struct tagged_allocator {
  using value_type = T;

  int id = 0;

  tagged_allocator() = default;
  explicit tagged_allocator(int identifier) : id(identifier) {}
  template <class U>
  tagged_allocator(const tagged_allocator<U>& other) : id(other.id) {}

  T* allocate(std::size_t n) { return std::allocator<T>{}.allocate(n); }
  void deallocate(T* ptr, std::size_t n) { std::allocator<T>{}.deallocate(ptr, n); }

  friend bool operator==(const tagged_allocator&, const tagged_allocator&) = default;
};

using set_type = chaistl::flat_multiset<int, std::less<int>, chaistl::vector<int, tagged_allocator<int>>>;

TEST(FlatMultisetAlloc, AllocatorExtendedDefaultConstruction) {
  set_type s(tagged_allocator<int>(7));

  EXPECT_EQ(s.keys().get_allocator().id, 7);
  EXPECT_TRUE(s.empty());
}

TEST(FlatMultisetAlloc, AllocatorExtendedIteratorConstruction) {
  std::vector<int> source{3, 1, 3, 2, 3};

  set_type s(source.begin(), source.end(), tagged_allocator<int>(3));

  EXPECT_EQ(s.keys().get_allocator().id, 3);
  EXPECT_THAT(s, ElementsAre(1, 2, 3, 3, 3));
  EXPECT_EQ(s.count(3), 3uz);
  auto [first, last] = s.equal_range(3);
  EXPECT_EQ(std::distance(first, last), 3);
}

TEST(FlatMultisetAlloc, AllocatorExtendedCopyConstruction) {
  set_type original({1, 2, 2, 3}, tagged_allocator<int>(1));

  set_type copy(original, tagged_allocator<int>(9));

  EXPECT_EQ(copy.keys().get_allocator().id, 9);
  EXPECT_TRUE(copy.contains(2));
  EXPECT_EQ(copy.count(2), 2uz);
  EXPECT_EQ(original.size(), 4uz);
}

TEST(FlatMultisetAlloc, AllocatorExtendedSortedEquivalentContainer) {
  chaistl::vector<int, tagged_allocator<int>> keys{1, 2, 2, 3};

  set_type s(chaistl::sorted_equivalent, keys, tagged_allocator<int>(5));

  EXPECT_EQ(s.keys().get_allocator().id, 5);
  EXPECT_TRUE(s.validate());
  EXPECT_EQ(s.count(2), 2uz);
}

TEST(FlatMultisetAlloc, BatchInsertKeepsExistingEquivalentKeysFirst) {
  set_type s({1, 2, 2, 5}, tagged_allocator<int>(4));
  const std::vector<int> incoming{4, 2, 3, 2};

  s.insert(incoming.begin(), incoming.end());

  EXPECT_THAT(s, ElementsAre(1, 2, 2, 2, 2, 3, 4, 5));
  EXPECT_EQ(s.count(2), 4uz);
  auto [first, last] = s.equal_range(2);
  EXPECT_EQ(std::distance(first, last), 4);
  EXPECT_TRUE(s.validate());
}

}  // namespace
