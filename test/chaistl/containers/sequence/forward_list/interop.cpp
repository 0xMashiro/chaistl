// SPDX-License-Identifier: Apache-2.0

// References:
// - std::forward_list interoperability:
//   https://en.cppreference.com/w/cpp/container/forward_list

#include <gtest/gtest.h>

#include <chaistl/containers/forward_list.hpp>

#include <algorithm>
#include <ranges>
#include <vector>

namespace {

using chaistl::forward_list;

TEST(ForwardListInterop, RangeForLoop) {
  forward_list<int> fl = {1, 2, 3, 4, 5};
  int sum = 0;
  for (int v : fl) {
    sum += v;
  }
  EXPECT_EQ(sum, 15);
}

TEST(ForwardListInterop, StdFind) {
  forward_list<int> fl = {1, 2, 3, 4, 5};
  auto it = std::find(fl.begin(), fl.end(), 3);
  EXPECT_NE(it, fl.end());
  EXPECT_EQ(*it, 3);

  it = std::find(fl.begin(), fl.end(), 99);
  EXPECT_EQ(it, fl.end());
}

TEST(ForwardListInterop, StdCopyToVector) {
  forward_list<int> fl = {1, 2, 3};
  std::vector<int> v;
  std::copy(fl.begin(), fl.end(), std::back_inserter(v));
  EXPECT_EQ(v, (std::vector<int>{1, 2, 3}));
}

TEST(ForwardListInterop, ConstIteration) {
  const forward_list<int> fl = {10, 20, 30};
  std::vector<int> v;
  for (auto it = fl.begin(); it != fl.end(); ++it) {
    v.push_back(*it);
  }
  EXPECT_EQ(v, (std::vector<int>{10, 20, 30}));
}

TEST(ForwardListInterop, RangesView) {
  forward_list<int> fl = {1, 2, 3, 4, 5, 6};
  auto even = fl | std::views::filter([](int x) {
                return x % 2 == 0;
              });
  std::vector<int> v(even.begin(), even.end());
  EXPECT_EQ(v, (std::vector<int>{2, 4, 6}));
}

TEST(ForwardListInterop, StdDistance) {
  forward_list<int> fl = {1, 2, 3, 4, 5};
  EXPECT_EQ(std::distance(fl.begin(), fl.end()), 5);
}

}  // namespace
