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
    # Vendored code is not ours to restyle. Excluding it keeps a dependency
    # bump from being blocked by a rule its upstream never agreed to.
    # apps/rcm-payload is vendored for the same reason (ISOLATION.md rule 4);
    # git normalises it to LF on commit regardless of the working copy.
    case "$f" in
        third_party/*|apps/rcm-payload/*) continue ;;
    esac
    case "$f" in
        *.png|*.jpg|*.jpeg|*.wav|*.nro|*.bin|*.zip|*.ttf) continue ;;
    esac
    [ -f "$f" ] || continue

    if ! python -c "import sys;open(sys.argv[1],encoding='utf-8').read()" "$f" 2>/dev/null; then
        printf 'ENCODING: %s is not valid UTF-8\n' "$f" >&2
        FAIL=1
    fi
    # Detect CR by removing it and seeing whether the file changed.
    #
    # NOT by a regex. This script is #!/bin/sh, which is dash on Debian, and
    # two obvious spellings both fail there:
    #   $'\r'          dash has no $'...' quoting; the pattern became "$r",
    #                  so any file containing $ref or $row "had CRLF".
    #   "$(printf '\r')"  command substitution strips the trailing CR, leaving an
    #                  empty pattern that matches everything.
    # Both were present in this file at different times. Comparison has no
    # quoting or shell-dialect hazard at all.
    if ! LC_ALL=C tr -d '\r' < "$f" | cmp -s - "$f"; then
        printf 'LINE ENDINGS: %s contains CR\n' "$f" >&2
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
