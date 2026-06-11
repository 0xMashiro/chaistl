# Build, Test & Benchmark

## Presets

| Preset | Compiler | Standard | Purpose |
|--------|----------|----------|---------|
| `clang-debug` | Clang | C++23 | Default development build |
| `gcc-debug` | GCC | C++23 | GCC compatibility |
| `clang-cxx26-debug` | Clang | C++26 | Experimental C++26 features |
| `gcc-cxx26-debug` | GCC | C++26 | Experimental C++26 features |
| `clang-coverage` | Clang | C++23 | Source-based code coverage |
| `clang-benchmark` | Clang | C++23 | Release-mode benchmarks |
| `gcc-benchmark` | GCC | C++23 | GCC release-mode benchmarks |

### Build and test with Clang

```sh
cmake --preset clang-debug
cmake --build --preset clang-debug
ctest --preset clang-debug
```

For a faster edit-compile-test loop:

```sh
ctest --preset clang-quick
```

### Build and test with GCC

```sh
cmake --preset gcc-debug
cmake --build --preset gcc-debug
ctest --preset gcc-debug
```

Focused test profiles:

```sh
ctest --preset gcc-quick       # smoke, concepts, iterators, memory, algorithms
ctest --preset gcc-containers  # all container tests
ctest --preset gcc-policy      # all policy-family tests
```

For a specific policy family, keep the preset broad and select the CTest label:

```sh
ctest --preset gcc-debug -L tree_policy
ctest --preset gcc-debug -L heap_policy
ctest --preset gcc-debug -L overflow_policy
```

### Experimental C++26 presets

```sh
cmake --preset clang-cxx26-debug
cmake --build --preset clang-cxx26-debug
ctest --preset clang-cxx26-debug
```

```sh
cmake --preset gcc-cxx26-debug
cmake --build --preset gcc-cxx26-debug
ctest --preset gcc-cxx26-debug
```

## Coverage

`chaistl` uses Clang source-based code coverage (`-fprofile-instr-generate`
+ `-fcoverage-mapping`). The workflow is:

```sh
# 1. Configure and build
cmake --preset clang-coverage
cmake --build --preset clang-coverage

# 2. Run tests with profiling (LLVM_PROFILE_FILE must be set)
LLVM_PROFILE_FILE="build/clang-coverage/cov_%p.profraw" ctest --preset clang-coverage

# 3. Merge raw profiles
llvm-profdata-22 merge build/clang-coverage/cov_*.profraw \
  -o build/clang-coverage/coverage.profdata

# 4. Generate report for a specific file
llvm-cov-22 report build/clang-coverage/test/chaistl_tests \
  -instr-profile=build/clang-coverage/coverage.profdata \
  include/chaistl/containers/sequence/forward_list.hpp

# 5. (Optional) HTML report
llvm-cov-22 show build/clang-coverage/test/chaistl_tests \
  -instr-profile=build/clang-coverage/coverage.profdata \
  -format=html -output-dir=build/clang-coverage/coverage_html \
  include/chaistl/
```

A convenience script is available:

```sh
./scripts/coverage.sh include/chaistl/containers/sequence/forward_list.hpp
```

### Coverage targets

- Coverage is a **gap-finding tool**, not a proof of correctness.
- 100% line coverage is not a goal. 100% **branch** coverage for the exception
  safety paths (the rollback branches in `if constexpr`) is.
- Focus the report on the component you changed — avoid scanning the whole
  library on every run.

## Benchmarks

Build benchmarks in release mode:

```sh
cmake --preset clang-benchmark
cmake --build --preset clang-benchmark --target chaistl_benchmarks
```

List registered benchmark cases without running them:

```sh
cmake --build --preset clang-benchmark --target chaistl_benchmark_list
```

Run smoke-sized benchmark profiles:

```sh
cmake --build --preset clang-benchmark --target chaistl_benchmark_smoke
cmake --build --preset clang-benchmark --target chaistl_benchmark_sequence_smoke
cmake --build --preset clang-benchmark --target chaistl_benchmark_policy_smoke
cmake --build --preset clang-benchmark --target chaistl_benchmark_tree_policy_smoke
```

These smoke targets use Google Benchmark dry-run mode. They are build checks,
not performance reports.

The split between CMake test presets, CTest labels, and Google Benchmark
filters follows the upstream tool model: `CMakePresets.json` owns repeatable
project-wide workflows, labels select focused CTest subsets, and benchmark
filters select small registration/execution checks without changing the
benchmark source matrix. The generic policy profiles are aggregators; concrete
policy families use explicit CTest labels such as `tree_policy`, `heap_policy`,
and `overflow_policy` instead of adding one preset per compiler/family pair.

Run a benchmark report:

```sh
./build/clang-benchmark/benchmark/chaistl_benchmarks \
  --benchmark_repetitions=5 \
  --benchmark_report_aggregates_only=true \
  --benchmark_out=build/clang-benchmark/vector.json \
  --benchmark_out_format=json
```

Benchmark results are diagnostic data. They should be used to understand
implementation tradeoffs, not as marketing claims.
