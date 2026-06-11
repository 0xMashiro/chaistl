// SPDX-License-Identifier: Apache-2.0

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chaistl/containers/flat_set.hpp>

#include <cstddef>
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

using set_type = chaistl::flat_set<int, std::less<int>, chaistl::vector<int, tagged_allocator<int>>>;

TEST(FlatSetAlloc, AllocatorExtendedDefaultConstruction) {
  set_type s(tagged_allocator<int>(7));

  EXPECT_EQ(s.keys().get_allocator().id, 7);
  EXPECT_TRUE(s.empty());
}

TEST(FlatSetAlloc, AllocatorExtendedIteratorConstruction) {
  std::vector<int> source{3, 1, 3, 2};

  set_type s(source.begin(), source.end(), tagged_allocator<int>(3));

  EXPECT_EQ(s.keys().get_allocator().id, 3);
  EXPECT_THAT(s, ElementsAre(1, 2, 3));
}

TEST(FlatSetAlloc, AllocatorExtendedCopyConstruction) {
  set_type original({1, 2, 3}, tagged_allocator<int>(1));

  set_type copy(original, tagged_allocator<int>(9));

  EXPECT_EQ(copy.keys().get_allocator().id, 9);
  EXPECT_TRUE(copy.contains(2));
  EXPECT_EQ(original.size(), 3uz);
}

TEST(FlatSetAlloc, AllocatorExtendedSortedUniqueContainer) {
  chaistl::vector<int, tagged_allocator<int>> keys{1, 2, 3};

  set_type s(chaistl::sorted_unique, keys, tagged_allocator<int>(5));

  EXPECT_EQ(s.keys().get_allocator().id, 5);
  EXPECT_TRUE(s.validate());
}

}  // namespace
