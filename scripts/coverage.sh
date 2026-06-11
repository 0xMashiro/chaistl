#!/usr/bin/env bash
# coverage.sh — generate a line-coverage report for one or more chaistl
# source files.  Runs the tests, merges raw profile data, and produces a
# per-file region/line/branch table.
#
# Usage:
#   ./scripts/coverage.sh [paths...]
#
#   If paths are omitted, reports on all of include/chaistl/.
#
#   The script requires:
#     - a working clang-coverage build (cmake --preset clang-coverage)
#     - llvm-profdata-22 and llvm-cov-22 on PATH
#
# Examples:
#   ./scripts/coverage.sh include/chaistl/containers/sequence/forward_list.hpp
#   ./scripts/coverage.sh include/chaistl/containers/sequence/list.hpp
#   ./scripts/coverage.sh                              # everything

set -euo pipefail

BUILD_DIR="build/clang-coverage"
TEST_BIN="${BUILD_DIR}/test/chaistl_tests"
PROFRAW_GLOB="${BUILD_DIR}/cov_*.profraw"
PROFDATA="${BUILD_DIR}/coverage.profdata"

SOURCES=("$@")
if [ ${#SOURCES[@]} -eq 0 ]; then
  SOURCES=("include/chaistl/")
fi

# 1. Build (incremental — quick if already up to date)
echo ":: building coverage binary..."
cmake --build --preset clang-coverage --target chaistl_tests 2>&1 | tail -2

# 2. Run tests with profiling
echo ":: running tests with coverage profiling..."
rm -f ${PROFRAW_GLOB}
LLVM_PROFILE_FILE="${BUILD_DIR}/cov_%p.profraw" ${TEST_BIN} 2>&1 | tail -3

# 3. Merge raw profiles
echo ":: merging profiles..."
llvm-profdata-22 merge ${PROFRAW_GLOB} -o "${PROFDATA}"

# 4. Report per file
for src in "${SOURCES[@]}"; do
  echo ""
  echo "================================================================"
  echo " Coverage: ${src}"
  echo "================================================================"
  llvm-cov-22 report "${TEST_BIN}" \
    -instr-profile="${PROFDATA}" \
    "${src}"
done
