# Benchmarking Guide

chaistl benchmarks are not correctness tests. They are workload probes that
explain where a container, storage helper, or policy choice spends time.

## Goals

- Compare `chaistl::*` containers with the closest `std::*` baseline.
- Compare policy variants that share one public container shape.
- Keep benchmark names stable enough for historical comparison.
- Exercise realistic operations, not only micro-operations that are easy to
  write.

## Include Policy

Benchmark files that demonstrate public containers should include the same
headers users include:

```cpp
#include <chaistl/containers/vector.hpp>
#include <chaistl/containers/avl_set.hpp>
```

Internal headers are allowed only when the benchmark target is explicitly an
internal mechanism, such as `chaistl::detail::construction_transaction` or
`uninitialized_storage_builder`.

## Benchmark Matrix

Think in four axes:

| Axis | Examples |
|------|----------|
| Container family | `vector`, `list`, `forward_list`, `set`, `priority_queue` |
| Policy | rb/avl/treap tree, binary/binomial/pairing heap |
| Workload | construction, growth, lookup, erase, splice, merge, churn |
| Value type | `int`, `std::string`, tracked value, move-heavy value, large value |

A new benchmark should make the intended point clear from its name:

```text
chaistl::vector<int>/push_back_growth/65536
chaistl::forward_list<std::string>/splice_after_all/4096
set_avl/lookup_hit/65536
order_statistic_set/find_by_order/4096
chaistl::dynamic_bitset/find_next_sparse/65536
chaistl::priority_deque/pop_minmax_random/65536
```

## Policy Benchmarks

Policy is a grouping concept, not one shared implementation model. Keep
benchmark names and CMake filters split by policy family:

| Family | Example filter target |
|--------|------------------------|
| Tree balance policy | `chaistl_benchmark_tree_policy_smoke` |
| Heap representation policy | `chaistl_benchmark_heap_policy_smoke` |
| Overflow policy | Add an overflow-specific target when ring-buffer benchmarks exist |

`chaistl_benchmark_policy_smoke` is the aggregate smoke target for all policy
families that currently have benchmark coverage.

Policy benchmarks should use public policy aliases when they exist:

```cpp
using rb_set = chaistl::set<std::int64_t>;
using avl_set = chaistl::avl_set<std::int64_t>;
using treap_set = chaistl::treap_set<std::int64_t>;
using size_balanced_set = chaistl::size_balanced_set<std::int64_t>;
using weight_balanced_set = chaistl::weight_balanced_set<std::int64_t>;
```

Use `detail::..._policy` directly only for internal policy-protocol benchmarks
where the benchmark is about the policy contract itself.

## Current Coverage

The benchmark suite currently covers:

- Sequence containers: `array`, `deque`, `forward_list`, `list`, `vector`
- Tree policy comparison: rb/avl/splay/treap/size-balanced/weight-balanced
  `set`, `skip_list_set`, plus `std::set`
- Tree augmentation: `find_by_order` and `order_of_key` across
  `order_statistic_set`, `size_balanced_set`, `weight_balanced_set`, and
  linear `std::set` baselines
- Heap adapters and policies: `std::priority_queue` baseline plus
  binary/d-ary/pairing/binomial/skew/leftist/fibonacci policies
- Bit structures: fixed `bitset` count/xor against `std::bitset`, dynamic
  bitset count/xor/sparse scan, and `priority_deque` double-ended removal
  against a `std::multiset` baseline
- Memory helpers: allocator, construction transaction, storage builder,
  uninitialized algorithms

Known gaps:

- Associative mapped containers: `map`, `multimap`, `multiset`
- Additional extension containers as they are promoted into the public API
- Heterogeneous lookup, node handles, merge/extract workloads

## Running Benchmarks

Configure and build:

```sh
cmake --preset gcc-benchmark
cmake --build --preset gcc-benchmark --parallel 2
```

List registered benchmarks without running them:

```sh
build/gcc-benchmark/benchmark/chaistl_benchmarks --benchmark_list_tests
```

The CMake target wrapper is preferred for routine checks:

```sh
cmake --build --preset gcc-benchmark --target chaistl_benchmark_list
```

Smoke profiles keep matrix growth manageable. They use Google Benchmark's
dry-run mode to verify registration and execution paths without collecting
performance numbers:

```sh
cmake --build --preset gcc-benchmark --target chaistl_benchmark_smoke
cmake --build --preset gcc-benchmark --target chaistl_benchmark_sequence_smoke
cmake --build --preset gcc-benchmark --target chaistl_benchmark_policy_smoke
cmake --build --preset gcc-benchmark --target chaistl_benchmark_tree_policy_smoke
cmake --build --preset gcc-benchmark --target chaistl_benchmark_heap_policy_smoke
cmake --build --preset gcc-benchmark --target chaistl_benchmark_bit_structure_smoke
```

Run a focused subset:

```sh
build/gcc-benchmark/benchmark/chaistl_benchmarks \
  --benchmark_filter='forward_list<int>/default_construct' \
  --benchmark_min_time=0.01s
```

For serious numbers, run on an idle machine, pin CPU frequency when possible,
and treat ASLR/load-average warnings as reasons to rerun before drawing
conclusions.
