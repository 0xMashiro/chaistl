#!/usr/bin/env bash
# format.sh — format chaistl C++ source files with clang-format.
#
# Usage:
#   ./scripts/format.sh [--check|--diff] [paths...]
#
#   If paths are omitted, scans tracked C++ files under:
#     - include/
#     - test/
#     - benchmark/
#     - tools/
#
#   If paths are provided, they may be files or directories. Only tracked C++
#   files are selected, so generated build output and vendored reference trees
#   are not formatted accidentally.
#
# Options:
#   --check   Verify formatting without modifying files. Suitable for CI.
#   --diff    Print formatting changes as a diff without modifying files.
#
# Environment:
#   CLANG_FORMAT   clang-format executable to use. Defaults to clang-format.
#
# Examples:
#   ./scripts/format.sh
#   ./scripts/format.sh --check
#   CLANG_FORMAT=clang-format-18 ./scripts/format.sh include test

set -euo pipefail

usage() {
  sed -n '2,35p' "$0" | sed 's/^# \{0,1\}//'
}

MODE="format"
case "${1:-}" in
  --check)
    MODE="check"
    shift
    ;;
  --diff)
    MODE="diff"
    shift
    ;;
  -h|--help)
    usage
    exit 0
    ;;
esac

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
if ! command -v "${CLANG_FORMAT}" >/dev/null 2>&1; then
  echo "error: ${CLANG_FORMAT} not found on PATH" >&2
  exit 127
fi

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "${REPO_ROOT}"

default_roots=(include test benchmark tools)
pathspecs=("$@")
if [ ${#pathspecs[@]} -eq 0 ]; then
  pathspecs=("${default_roots[@]}")
fi

is_cpp_file() {
  case "$1" in
    *.h|*.hpp|*.cc|*.cpp|*.cxx) return 0 ;;
    *) return 1 ;;
  esac
}

files=()
while IFS= read -r -d '' file; do
  if [ -f "${file}" ] && is_cpp_file "${file}"; then
    files+=("${file}")
  fi
done < <(git ls-files -z -- "${pathspecs[@]}")

if [ ${#files[@]} -eq 0 ]; then
  echo ":: no tracked C++ files matched"
  exit 0
fi

case "${MODE}" in
  check)
    echo ":: checking ${#files[@]} files with ${CLANG_FORMAT}..."
    "${CLANG_FORMAT}" --dry-run --Werror "${files[@]}"
    ;;
  diff)
    echo ":: diffing ${#files[@]} files with ${CLANG_FORMAT}..."
    tmpdir="$(mktemp -d)"
    trap 'rm -rf "${tmpdir}"' EXIT

    changed=0
    for file in "${files[@]}"; do
      mkdir -p "${tmpdir}/$(dirname "${file}")"
      "${CLANG_FORMAT}" "${file}" >"${tmpdir}/${file}"
      if ! diff -u --label "a/${file}" --label "b/${file}" "${file}" "${tmpdir}/${file}"; then
        changed=1
      fi
    done
    exit "${changed}"
    ;;
  format)
    echo ":: formatting ${#files[@]} files with ${CLANG_FORMAT}..."
    "${CLANG_FORMAT}" -i "${files[@]}"
    ;;
esac
