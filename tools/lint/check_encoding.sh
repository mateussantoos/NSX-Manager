#!/bin/sh
# Every tracked text file must be valid UTF-8 with LF endings.
#
# The predecessor's README.md was UTF-8 bytes rendered as Latin-1 ("ultima
# geracao" came out as mojibake) because nothing enforced this. .gitattributes
# and .editorconfig declare the rule; this is what checks it.
# SPDX-License-Identifier: GPL-3.0-only
set -eu
cd "$(git rev-parse --show-toplevel)"
FAIL=0

for f in $(git ls-files); do
    case "$f" in
        *.png|*.jpg|*.jpeg|*.wav|*.nro|*.bin|*.zip|*.ttf) continue ;;
    esac
    [ -f "$f" ] || continue

    if ! python -c "import sys;open(sys.argv[1],encoding='utf-8').read()" "$f" 2>/dev/null; then
        printf 'ENCODING: %s is not valid UTF-8\n' "$f" >&2
        FAIL=1
    fi
    if grep -qU $'\r' "$f" 2>/dev/null; then
        printf 'LINE ENDINGS: %s contains CRLF\n' "$f" >&2
        FAIL=1
    fi
    # A UTF-8 BOM breaks shell scripts and some C++ compilers.
    if head -c 3 "$f" | od -An -tx1 2>/dev/null | grep -q 'ef bb bf'; then
        printf 'BOM: %s starts with a UTF-8 BOM\n' "$f" >&2
        FAIL=1
    fi
done

[ "$FAIL" -eq 0 ] || exit 1
echo "check_encoding: OK"
