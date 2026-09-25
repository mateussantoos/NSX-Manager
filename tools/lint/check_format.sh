#!/bin/sh
# clang-format check. Fails with a diff rather than silently reformatting.
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(git rev-parse --show-toplevel)"

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
command -v "$CLANG_FORMAT" >/dev/null || {
    echo "check_format: $CLANG_FORMAT not found - skipping (CI provides it)"; exit 0; }

FILES="$(find src apps/forwarder -name '*.cpp' -o -name '*.hpp' -o -name '*.c' -o -name '*.h' 2>/dev/null || true)"
[ -n "$FILES" ] || { echo "check_format: no sources yet - OK"; exit 0; }

FAIL=0
for f in $FILES; do
    if ! "$CLANG_FORMAT" --dry-run --Werror "$f" 2>/dev/null; then
        printf 'FORMAT: %s\n' "$f" >&2
        "$CLANG_FORMAT" "$f" | diff -u "$f" - | head -40 >&2 || true
        FAIL=1
    fi
done

[ "$FAIL" -eq 0 ] || { echo "\nRun: clang-format -i <file>   (config: .clang-format)" >&2; exit 1; }
echo "check_format: OK"
