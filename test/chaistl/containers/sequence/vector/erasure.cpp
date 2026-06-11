// SPDX-License-Identifier: Apache-2.0

// References:
// - [vector.erasure], non-member erase operations:
//   https://eel.is/c++draft/vector.erasure
//   https://en.cppreference.com/w/cpp/container/vector/erase2
// - Test cases are written for chaistl and are not copied from upstream
//   standard-library tests.

#include <gtest/gtest.h>

#include <chaistl/containers/vector.hpp>

#include <algorithm>
#include <array>
#include <ranges>

TEST(VectorTest, NonMemberEraseOperations) {
  chaistl::vector<int> values{1, 2, 2, 3, 2, 4};

  EXPECT_EQ(chaistl::erase(values, 2), 3uz);
  EXPECT_TRUE(std::ranges::equal(values, std::array{1, 3, 4}));

  EXPECT_EQ(chaistl::erase_if(values,
                              [](int value) {
                                return value % 2 != 0;
                              }),
            2uz);
  EXPECT_TRUE(std::ranges::equal(values, std::array{4}));
}
