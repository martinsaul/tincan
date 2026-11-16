#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Tincan contributors

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOFT_LINE_LIMIT=400
HARD_LINE_LIMIT=800
MAX_FUNC_NLOC=60

check_file_sizes() {
    local status=0
    local file lines rel

    while IFS= read -r -d '' file; do
        lines="$(wc -l < "${file}" | tr -d ' ')"
        rel="${file#"${REPO_ROOT}/"}"
        if (( lines > HARD_LINE_LIMIT )); then
            echo "ERROR: ${rel} has ${lines} lines (hard limit: ${HARD_LINE_LIMIT})" >&2
            status=1
        elif (( lines > SOFT_LINE_LIMIT )); then
            echo "WARNING: ${rel} has ${lines} lines (soft target: ${SOFT_LINE_LIMIT})" >&2
        fi
    done < <(find "${REPO_ROOT}/src" "${REPO_ROOT}/tests" "${REPO_ROOT}/tools" \
        -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.sh' -o -name 'CMakeLists.txt' \) \
        -print0 | sort -z)

    return "${status}"
}

check_function_sizes() {
    if ! command -v lizard >/dev/null 2>&1; then
        echo "WARNING: lizard is not installed on PATH; skipping function length check." >&2
        return 0
    fi

    lizard -L "${MAX_FUNC_NLOC}" -w "${REPO_ROOT}/src" "${REPO_ROOT}/tests"
}

main() {
    local failed=0
    if ! check_file_sizes; then
        failed=1
    fi
    if ! check_function_sizes; then
        failed=1
    fi

    if (( failed != 0 )); then
        echo "Size check FAILED." >&2
        exit 1
    fi
    echo "Size check passed."
}

main "$@"
