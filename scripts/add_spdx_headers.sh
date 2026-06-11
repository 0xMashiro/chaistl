#!/usr/bin/env bash
# add_spdx_headers.sh — add Apache-2.0 SPDX headers to chaistl C++ files.
#
# Usage:
#   ./scripts/add_spdx_headers.sh
#
# Scans .hpp and .cpp files under:
#   - include/
#   - test/
#   - benchmark/
#
# Files that already contain an SPDX-License-Identifier marker anywhere are
# left unchanged.

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "${REPO_ROOT}"

SPDX_LINE="// SPDX-License-Identifier: Apache-2.0"
roots=(include test benchmark)
modified=0

while IFS= read -r -d '' file; do
  if grep -q 'SPDX-License-Identifier' "${file}"; then
    continue
  fi

  tmp="$(mktemp)"
  {
    printf '%s\n\n' "${SPDX_LINE}"
    cat "${file}"
  } >"${tmp}"
  cat "${tmp}" >"${file}"
  rm -f "${tmp}"

  modified=$((modified + 1))
done < <(find "${roots[@]}" -type f \( -name '*.hpp' -o -name '*.cpp' \) -print0 | sort -z)

echo ":: added SPDX headers to ${modified} files"
