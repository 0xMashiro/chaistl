// SPDX-License-Identifier: Apache-2.0

// Property tests: every heap policy is driven through long random operation
// sequences against trusted oracles (std::priority_queue / std::set), with
// the container's full invariant check (verify()) sampled along the way.
//
// The exception-contract sweeps at the bottom certify the policy concepts'
// exception table (policy/concepts.hpp): a comparator that throws on the
// k-th comparison is injected for every feasible k, and the contract —
// strong for extract/detach, absorb-always for insert/join — is asserted
// for each. Run under the ASan presets these also prove "no leak on any
// comparator-throw path".

#include <gtest/gtest.h>

#include <chaistl/containers/heap/policy/binary_heap.hpp>
#include <chaistl/containers/heap/policy/binomial_heap.hpp>
#include <chaistl/containers/heap/policy/d_ary_heap.hpp>
#include <chaistl/containers/heap/policy/fibonacci_heap.hpp>
#include <chaistl/containers/heap/policy/leftist_heap.hpp>
#include <chaistl/containers/heap/policy/pairing_heap.hpp>
#include <chaistl/containers/heap/policy/skew_heap.hpp>
#include <chaistl/containers/priority_queue.hpp>

#include <algorithm>
#include <functional>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using chaistl::heap_policy::binary_heap_policy;
using chaistl::heap_policy::binomial_heap_policy;
using chaistl::heap_policy::d_ary_heap_policy;
using chaistl::heap_policy::fibonacci_heap_policy;
using chaistl::heap_policy::leftist_heap_policy;
using chaistl::heap_policy::pairing_heap_policy;
using chaistl::heap_policy::skew_heap_policy;

template <class Policy>
class HeapPropertyTest : public ::testing::Test {
 protected:
  template <class T, class Compare = std::less<T>>
  using queue_t = chaistl::priority_queue<T, Compare, chaistl::allocator<T>, Policy>;
};

using PolicyTypes = ::testing::Types<binary_heap_policy,
                                     d_ary_heap_policy<4>,
                                     pairing_heap_policy,
                                     binomial_heap_policy,
                                     skew_heap_policy,
                                     leftist_heap_policy,
                                     fibonacci_heap_policy>;
TYPED_TEST_SUITE(HeapPropertyTest, PolicyTypes);

// Drains by repeated pop, collecting a multiset — content comparison that
// does not depend on the heap's internal arrangement.
template <class PQ>
std::multiset<int> drain_multiset(PQ& pq) {
  std::multiset<int> out;
  while (!pq.empty()) {
    out.insert(pq.top());
    pq.pop();
  }
  return out;
}

// ============================================================================
// Random-operation oracles
// ============================================================================

TYPED_TEST(HeapPropertyTest, MatchesStdPriorityQueueOracle) {
  using PQ = typename TestFixture::template queue_t<int>;
  std::mt19937 rng(20260611u);
  PQ pq;
  std::priority_queue<int> oracle;

  for (int step = 0; step < 3000; ++step) {
    if (oracle.empty() || rng() % 100 < 58) {
      const int value = static_cast<int>(rng() % 997);
      pq.push(value);
      oracle.push(value);
    } else {
      ASSERT_EQ(pq.top(), oracle.top());
      pq.pop();
      oracle.pop();
    }
    ASSERT_EQ(pq.size(), oracle.size());
    if (step % 16 == 0) {
      ASSERT_TRUE(pq.verify()) << "invariant broken at step " << step;
    }
  }
  while (!oracle.empty()) {
    ASSERT_EQ(pq.top(), oracle.top());
    pq.pop();
    oracle.pop();
  }
  EXPECT_TRUE(pq.empty());
}

TYPED_TEST(HeapPropertyTest, JoinEquivalentToPushingEverything) {
  using PQ = typename TestFixture::template queue_t<int>;
  if constexpr (PQ::uses_array_storage) {
    GTEST_SKIP() << "join() requires a node policy";
  } else {
    std::mt19937 rng(0x5EEDu);
    for (const auto& [left, right] : {std::pair{0, 1}, {1, 0}, {1, 1}, {7, 9}, {64, 1}, {200, 333}}) {
      PQ a;
      PQ b;
      std::vector<int> all;
      for (int i = 0; i < left; ++i) {
        const int v = static_cast<int>(rng() % 512);
        a.push(v);
        all.push_back(v);
      }
      for (int i = 0; i < right; ++i) {
        const int v = static_cast<int>(rng() % 512);
        b.push(v);
        all.push_back(v);
      }
      a.join(b);
      ASSERT_TRUE(b.empty());
      ASSERT_EQ(a.size(), all.size());
      ASSERT_TRUE(a.verify());

      std::ranges::sort(all, std::greater<int>{});
      for (const int expected : all) {
        ASSERT_EQ(a.top(), expected);
        a.pop();
      }
      ASSERT_TRUE(a.empty());
    }
  }
}

TYPED_TEST(HeapPropertyTest, CopyDrainsIdenticallyToOriginal) {
  using PQ = typename TestFixture::template queue_t<int>;
  std::mt19937 rng(0xABCDu);
  PQ original;
  for (int i = 0; i < 500; ++i) {
    original.push(static_cast<int>(rng() % 100));  // plenty of duplicates
  }
  PQ copy(original);
  ASSERT_EQ(copy.size(), original.size());
  ASSERT_TRUE(copy.verify());
  while (!original.empty()) {
    ASSERT_EQ(copy.top(), original.top());
    copy.pop();
    original.pop();
  }
  EXPECT_TRUE(copy.empty());
}

// ============================================================================
// Handle-mix oracle — every mutable policy
//
// Unique values let a pop be mirrored exactly in the handle map, so all four
// mutating operations (push / pop / erase(it) / modify(it, v)) interleave.
// For binomial this is the heavy hammer on swap_with_parent: every eighth
// step re-checks links, exact Bₖ shape (degree slots), order, and counts.
// ============================================================================

template <class Policy>
class MutableHeapPropertyTest : public ::testing::Test {
 protected:
  template <class T, class Compare = std::less<T>>
  using queue_t = chaistl::priority_queue<T, Compare, chaistl::allocator<T>, Policy>;
};

using MutablePolicyTypes = ::testing::Types<pairing_heap_policy, binomial_heap_policy>;
TYPED_TEST_SUITE(MutableHeapPropertyTest, MutablePolicyTypes);

TYPED_TEST(MutableHeapPropertyTest, HandleOperationsMatchSetOracle) {
  using PQ = typename TestFixture::template queue_t<int>;
  std::mt19937 rng(0xC0FFEEu);
  PQ pq;
  std::set<int> oracle;
  std::unordered_map<int, typename PQ::point_iterator> handles;

  const auto fresh_value = [&] {
    while (true) {
      const int v = static_cast<int>(rng() % 1'000'000);
      if (!oracle.contains(v)) {
        return v;
      }
    }
  };
  const auto random_present_value = [&] {
    auto it = oracle.begin();
    std::advance(it, static_cast<long>(rng() % oracle.size()));
    return *it;
  };

  for (int step = 0; step < 1500; ++step) {
    const unsigned op = rng() % 100;
    if (oracle.empty() || op < 45) {
      const int v = fresh_value();
      handles.emplace(v, pq.push(v));
      oracle.insert(v);
    } else if (op < 65) {
      const int v = *oracle.rbegin();  // pop removes the maximum
      ASSERT_EQ(pq.top(), v);
      pq.pop();
      handles.erase(v);
      oracle.erase(std::prev(oracle.end()));
    } else if (op < 85) {
      const int v = random_present_value();
      pq.erase(handles.at(v));
      handles.erase(v);
      oracle.erase(v);
    } else {
      const int old_value = random_present_value();
      const int new_value = fresh_value();
      auto handle = handles.at(old_value);
      pq.modify(handle, new_value);
      ASSERT_EQ(*handle, new_value);  // same node, new value
      handles.erase(old_value);
      handles.emplace(new_value, handle);
      oracle.erase(old_value);
      oracle.insert(new_value);
    }

    ASSERT_EQ(pq.size(), oracle.size());
    if (!oracle.empty()) {
      ASSERT_EQ(pq.top(), *oracle.rbegin());
    }
    if (step % 8 == 0) {
      ASSERT_TRUE(pq.verify()) << "invariant broken at step " << step;
    }
  }

  for (auto it = oracle.rbegin(); it != oracle.rend(); ++it) {
    ASSERT_EQ(pq.top(), *it);
    pq.pop();
  }
  EXPECT_TRUE(pq.empty());
}

// ============================================================================
// Exception-contract sweeps
//
// ThrowAfter throws on the k-th comparison after arming. Sweeping k over
// every feasible value hits each comparison site of an operation once; the
// assertions encode the contract row by row. ASan presets turn any missed
// rollback into a leak report.
// ============================================================================

struct ThrowAfter {
  int* budget;  // armed when > 0; the comparison that decrements it to 0 throws

  bool operator()(int a, int b) const {
    if (*budget > 0 && --*budget == 0) {
      throw std::runtime_error("comparator throw");
    }
    return a < b;
  }
};

template <class Policy>
using throwing_queue = chaistl::priority_queue<int, ThrowAfter, chaistl::allocator<int>, Policy>;

TEST(HeapExceptionContractTest, ExtractTopIsStrong) {
  const std::multiset<int> content{1, 2, 3, 5, 7, 8, 9};
  const auto run = [&](auto policy_tag, int k) {
    using PQ = throwing_queue<decltype(policy_tag)>;
    int budget = 0;
    PQ pq(ThrowAfter{&budget});
    for (const int v : content) {
      pq.push(v);
    }
    budget = k;
    bool threw = false;
    try {
      pq.pop();
    } catch (const std::runtime_error&) {
      threw = true;
    }
    budget = 0;
    if (threw) {
      // Strong: the pop did not happen. The heap may be in the documented
      // relaxed-but-valid form, which verify() accepts.
      ASSERT_EQ(pq.size(), content.size());
      ASSERT_TRUE(pq.verify());
      ASSERT_EQ(drain_multiset(pq), content);
    } else {
      ASSERT_EQ(pq.size(), content.size() - 1);
      auto expected = content;
      expected.erase(std::prev(expected.end()));
      ASSERT_EQ(drain_multiset(pq), expected);
    }
  };
  for (int k = 1; k <= 24; ++k) {
    run(pairing_heap_policy{}, k);
    run(binomial_heap_policy{}, k);
    run(fibonacci_heap_policy{}, k);
  }
}

TEST(HeapExceptionContractTest, DetachIsStrong) {
  // Two shapes per policy: descending pushes make the first element dominant
  // (a wide pairing root / the B₃ root), ascending pushes bury it (a deep
  // chain leaf / the deepest binomial leaf). The binomial bubble itself is
  // comparison-free, so every injected throw lands in pairing's child melds
  // or in the final carry pass — and must leave the heap untouched.
  const auto run = [](auto policy_tag, bool ascending, int k) {
    using PQ = throwing_queue<decltype(policy_tag)>;
    int budget = 0;
    PQ pq(ThrowAfter{&budget});
    const int first = ascending ? 1 : 8;
    auto handle = pq.push(first);
    if (ascending) {
      for (int v = 2; v <= 8; ++v) {
        pq.push(v);
      }
    } else {
      for (int v = 7; v >= 1; --v) {
        pq.push(v);
      }
    }
    budget = k;
    bool threw = false;
    try {
      pq.erase(handle);
    } catch (const std::runtime_error&) {
      threw = true;
    }
    budget = 0;
    std::multiset<int> expected{1, 2, 3, 4, 5, 6, 7, 8};
    if (threw) {
      ASSERT_EQ(pq.size(), 8uz);
      ASSERT_TRUE(pq.verify());  // strong: nothing left the heap, order intact
      ASSERT_EQ(drain_multiset(pq), expected);
    } else {
      ASSERT_EQ(pq.size(), 7uz);
      expected.erase(expected.find(first));
      ASSERT_EQ(drain_multiset(pq), expected);
    }
  };
  for (int k = 1; k <= 12; ++k) {
    for (const bool ascending : {false, true}) {
      run(pairing_heap_policy{}, ascending, k);
      run(binomial_heap_policy{}, ascending, k);
    }
  }
}

TEST(HeapExceptionContractTest, InsertAbsorbsEvenOnThrow) {
  const auto run = [&](auto policy_tag, int k) {
    using PQ = throwing_queue<decltype(policy_tag)>;
    int budget = 0;
    PQ pq(ThrowAfter{&budget});
    for (const int v : {5, 1, 9, 3, 7, 2, 8}) {
      pq.push(v);
    }
    budget = k;
    try {
      pq.push(6);
    } catch (const std::runtime_error&) {
    }
    budget = 0;
    // Inserted in both outcomes: ownership transfers unconditionally. After
    // a throwing comparator the order is unspecified, so only size, content,
    // and (via ASan) memory safety are asserted.
    ASSERT_EQ(pq.size(), 8uz);
    ASSERT_EQ(drain_multiset(pq), (std::multiset<int>{1, 2, 3, 5, 6, 7, 8, 9}));
  };
  for (int k = 1; k <= 6; ++k) {
    run(pairing_heap_policy{}, k);
    run(binomial_heap_policy{}, k);
  }
}

TEST(HeapExceptionContractTest, JoinAbsorbsEvenOnThrow) {
  const auto run = [&](auto policy_tag, int k) {
    using PQ = throwing_queue<decltype(policy_tag)>;
    int budget = 0;
    PQ a(ThrowAfter{&budget});
    PQ b(ThrowAfter{&budget});
    for (const int v : {4, 9, 1}) {
      a.push(v);
    }
    for (const int v : {6, 2, 8, 3}) {
      b.push(v);
    }
    budget = k;
    try {
      a.join(b);
    } catch (const std::runtime_error&) {
    }
    budget = 0;
    ASSERT_TRUE(b.empty());  // absorbed in both outcomes
    ASSERT_EQ(a.size(), 7uz);
    ASSERT_EQ(drain_multiset(a), (std::multiset<int>{1, 2, 3, 4, 6, 8, 9}));
  };
  for (int k = 1; k <= 8; ++k) {
    run(pairing_heap_policy{}, k);
    run(binomial_heap_policy{}, k);
  }
}

}  // namespace
