#!/bin/sh
# Keep docs/adr/README.md honest.
#
# An ADR index that silently drifts from the files beside it is worse than no
# index - it tells you a decision was made and then hides it. This fails when:
#   * an ADR file is missing from the index table
#   * the index lists an ADR that does not exist
#   * the index status disagrees with the file's front-matter
#   * a superseded-by link is not reciprocal
# SPDX-License-Identifier: GPL-3.0-only

set -eu
cd "$(git rev-parse --show-toplevel)"

ADR_DIR="docs/adr"
INDEX="$ADR_DIR/README.md"
FAIL=0

[ -d "$ADR_DIR" ] || { echo "check_adr_index: no docs/adr/ yet - OK"; exit 0; }
[ -f "$INDEX" ] || { echo "check_adr_index: $INDEX is missing" >&2; exit 1; }

for f in "$ADR_DIR"/[0-9][0-9][0-9][0-9]-*.md; do
    [ -e "$f" ] || continue
    base="$(basename "$f")"
    num="$(printf '%s' "$base" | cut -c1-4)"

    if ! grep -q "$base" "$INDEX"; then
        printf 'ADR %s is not linked from %s\n' "$base" "$INDEX" >&2
        FAIL=1
    fi

    status="$(sed -n 's/^status:[[:space:]]*"\{0,1\}\([a-z]*\)"\{0,1\}[[:space:]]*$/\1/p' "$f" | head -n1)"
    if [ -z "$status" ]; then
        printf 'ADR %s has no status in its front-matter\n' "$base" >&2
        FAIL=1
        continue
    fi
    case "$status" in
        proposed|accepted|rejected|deprecated|superseded) ;;
        *) printf 'ADR %s has invalid status "%s"\n' "$base" "$status" >&2; FAIL=1 ;;
    esac

    # The index row for this ADR must carry the same status word.
    row="$(grep "$base" "$INDEX" | head -n1)"
    if ! printf '%s' "$row" | grep -qi "$status"; then
        printf 'ADR %s: file says "%s" but the index row does not\n' "$base" "$status" >&2
        printf '  row: %s\n' "$row" >&2
        FAIL=1
    fi

    # superseded-by must be reciprocal.
    sb="$(sed -n 's/^superseded-by:[[:space:]]*\[\{0,1\}"\{0,1\}\(ADR-[0-9]*\)".*/\1/p' "$f" | head -n1)"
    if [ -n "$sb" ]; then
        target_num="$(printf '%s' "$sb" | sed 's/ADR-//')"
        target="$(ls "$ADR_DIR/$target_num"-*.md 2>/dev/null | head -n1 || true)"
        if [ -z "$target" ]; then
            printf 'ADR %s is superseded by %s, which does not exist\n' "$base" "$sb" >&2
            FAIL=1
        elif ! grep -q "ADR-$num" "$target"; then
            printf 'ADR %s says %s supersedes it, but %s does not say so back\n' \
                   "$base" "$sb" "$(basename "$target")" >&2
            FAIL=1
        fi
    fi
done

# Index rows pointing at files that do not exist.
for linked in $(grep -oE '[0-9]{4}-[a-z0-9-]+\.md' "$INDEX" | sort -u); do
    [ -f "$ADR_DIR/$linked" ] || { printf 'index links %s, which does not exist\n' "$linked" >&2; FAIL=1; }
done

[ "$FAIL" -eq 0 ] || { echo "" >&2; echo "check_adr_index: index and files disagree." >&2; exit 1; }
echo "check_adr_index: OK"
