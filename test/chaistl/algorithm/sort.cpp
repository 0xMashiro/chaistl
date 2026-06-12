// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <chaistl/algorithm/sort.hpp>
#include <chaistl/containers/deque.hpp>
#include <chaistl/containers/vector.hpp>

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <numeric>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace {

using sort_function = void (*)(std::vector<int>::iterator, std::vector<int>::iterator);

std::vector<int> sample_values() {
  return {9, 1, 5, 3, 7, 3, 2, 8, 6, 4, 0, 5};
}

class SortAlgorithmTest : public ::testing::TestWithParam<sort_function> {};

TEST_P(SortAlgorithmTest, SortsVectorWithDuplicates) {
  std::vector<int> values = sample_values();

  GetParam()(values.begin(), values.end());

  EXPECT_TRUE(std::ranges::is_sorted(values));
  EXPECT_TRUE(std::ranges::equal(values, std::array{0, 1, 2, 3, 3, 4, 5, 5, 6, 7, 8, 9}));
}

INSTANTIATE_TEST_SUITE_P(AllSorts,
                         SortAlgorithmTest,
                         ::testing::Values(&chaistl::insertion_sort<std::vector<int>::iterator>,
                                           &chaistl::binary_insertion_sort<std::vector<int>::iterator>,
                                           &chaistl::selection_sort<std::vector<int>::iterator>,
                                           &chaistl::bubble_sort<std::vector<int>::iterator>,
                                           &chaistl::cocktail_sort<std::vector<int>::iterator>,
                                           &chaistl::shell_sort<std::vector<int>::iterator>,
                                           &chaistl::heap_sort<std::vector<int>::iterator>,
                                           &chaistl::quick_sort<std::vector<int>::iterator>,
                                           &chaistl::qsort<std::vector<int>::iterator>,
                                           &chaistl::merge_sort<std::vector<int>::iterator>,
                                           &chaistl::bottom_up_merge_sort<std::vector<int>::iterator>,
                                           &chaistl::stable_sort<std::vector<int>::iterator>,
                                           &chaistl::intro_sort<std::vector<int>::iterator>,
                                           &chaistl::pdqsort<std::vector<int>::iterator>,
                                           &chaistl::sort<std::vector<int>::iterator>));

TEST(SortAlgorithmTest, SortSupportsCustomComparator) {
  chaistl::vector<int> values{1, 4, 2, 3};

  chaistl::sort(values.begin(), values.end(), std::greater<>{});

  EXPECT_TRUE(std::ranges::equal(values, std::array{4, 3, 2, 1}));
}

TEST(SortAlgorithmTest, SortWorksOnDequeRandomAccessIterators) {
  chaistl::deque<int> values{7, 2, 5, 1, 9, 3};

  chaistl::pdqsort(values.begin(), values.end());

  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3, 5, 7, 9}));
}

TEST(SortAlgorithmTest, SortWorksOnArrayIterators) {
  std::array<int, 6> values{6, 5, 4, 3, 2, 1};

  chaistl::intro_sort(values.begin(), values.end());

  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 2, 3, 4, 5, 6}));
}

TEST(SortAlgorithmTest, StableSortPreservesEquivalentOrder) {
  std::vector<std::pair<int, std::string>> values{{2, "first"}, {1, "one"}, {2, "second"}, {2, "third"}};

  chaistl::stable_sort(values.begin(), values.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.first < rhs.first;
  });

  ASSERT_EQ(values.size(), 4uz);
  EXPECT_EQ(values[0].second, "one");
  EXPECT_EQ(values[1].second, "first");
  EXPECT_EQ(values[2].second, "second");
  EXPECT_EQ(values[3].second, "third");
}

TEST(SortAlgorithmTest, BottomUpMergeSortPreservesEquivalentOrder) {
  std::vector<std::pair<int, std::string>> values{{2, "first"}, {1, "one"}, {2, "second"}, {2, "third"}};

  chaistl::bottom_up_merge_sort(values.begin(), values.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.first < rhs.first;
  });

  ASSERT_EQ(values.size(), 4uz);
  EXPECT_EQ(values[0].second, "one");
  EXPECT_EQ(values[1].second, "first");
  EXPECT_EQ(values[2].second, "second");
  EXPECT_EQ(values[3].second, "third");
}

TEST(SortAlgorithmTest, IntroSortSupportsMoveOnlyValues) {
  std::vector<std::unique_ptr<int>> values;
  values.push_back(std::make_unique<int>(3));
  values.push_back(std::make_unique<int>(1));
  values.push_back(std::make_unique<int>(2));

  chaistl::sort(values.begin(), values.end(), [](const auto& lhs, const auto& rhs) {
    return *lhs < *rhs;
  });

  EXPECT_EQ(*values[0], 1);
  EXPECT_EQ(*values[1], 2);
  EXPECT_EQ(*values[2], 3);
}

TEST(SortAlgorithmTest, PdqsortHandlesPatternedInputs) {
  std::vector<int> sorted(1024);
  std::iota(sorted.begin(), sorted.end(), 0);
  chaistl::pdqsort(sorted.begin(), sorted.end());
  EXPECT_TRUE(std::ranges::is_sorted(sorted));

  std::vector<int> organ_pipe;
  organ_pipe.reserve(1024);
  for (int value = 0; value != 512; ++value) {
    organ_pipe.push_back(value);
  }
  for (int value = 511; value >= 0; --value) {
    organ_pipe.push_back(value);
  }

  chaistl::pdqsort(organ_pipe.begin(), organ_pipe.end());

  EXPECT_TRUE(std::ranges::is_sorted(organ_pipe));
}

TEST(SortAlgorithmTest, SortHandlesManyEquivalentValues) {
  std::vector<int> values(4096);
  for (std::size_t index = 0; index != values.size(); ++index) {
    values[index] = static_cast<int>((index * 48271u + 17u) % 8u);
  }

  chaistl::sort(values.begin(), values.end());

  EXPECT_TRUE(std::ranges::is_sorted(values));
  EXPECT_EQ(std::ranges::count(values, 0), 512);
  EXPECT_EQ(std::ranges::count(values, 7), 512);
}

TEST(SortAlgorithmTest, PdqsortHandlesManyEquivalentValues) {
  std::vector<int> values(4096, 42);

  chaistl::pdqsort(values.begin(), values.end());

  EXPECT_TRUE(std::ranges::is_sorted(values));
  EXPECT_TRUE(std::ranges::all_of(values, [](int value) {
    return value == 42;
  }));
}

}  // namespace
