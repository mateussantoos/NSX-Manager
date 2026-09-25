#!/bin/sh
# Fetch the Mozilla CA bundle and verify it against the committed hash.
#
# The bundle itself is gitignored; only its SHA-256 is committed. That way the
# trust anchor changes as a ONE-LINE reviewable diff instead of a 250 kB blob
# nobody reads - and a tampered download cannot slip in silently.
#
#   tools/cacert/fetch.sh            verify against the committed hash
#   tools/cacert/fetch.sh --update   accept a new bundle and rewrite the hash
# SPDX-License-Identifier: GPL-3.0-only

set -eu
cd "$(git rev-parse --show-toplevel)"

URL="https://curl.se/ca/cacert.pem"
DIR="third_party/cacert"
PEM="$DIR/cacert.pem"
SUM="$DIR/cacert.pem.sha256"
UPDATE=0
[ "${1:-}" = "--update" ] && UPDATE=1

mkdir -p "$DIR"
echo "Fetching $URL ..."
if command -v curl >/dev/null; then
    curl -fsSL --proto '=https' --tlsv1.2 "$URL" -o "$PEM.tmp"
elif command -v wget >/dev/null; then
    wget -q --https-only "$URL" -O "$PEM.tmp"
else
    echo "ERROR: need curl or wget" >&2; exit 1
fi

ACTUAL="$(sha256sum "$PEM.tmp" | cut -d' ' -f1)"

if [ "$UPDATE" -eq 1 ]; then
    printf '%s  cacert.pem\n' "$ACTUAL" > "$SUM"
    mv "$PEM.tmp" "$PEM"
    echo "Updated $SUM to $ACTUAL"
    echo "Commit it with:  build(deps): update CA bundle"
    exit 0
fi

if [ ! -f "$SUM" ]; then
    echo "ERROR: $SUM does not exist. Run '$0 --update' to pin the current bundle." >&2
    rm -f "$PEM.tmp"; exit 1
fi

EXPECTED="$(cut -d' ' -f1 < "$SUM")"
if [ "$ACTUAL" != "$EXPECTED" ]; then
    echo "ERROR: CA bundle hash mismatch - refusing to install it." >&2
    echo "  expected: $EXPECTED" >&2
    echo "  actual:   $ACTUAL" >&2
    echo "" >&2
    echo "If Mozilla published a new bundle, run '$0 --update' and review the diff." >&2
    rm -f "$PEM.tmp"; exit 1
fi

mv "$PEM.tmp" "$PEM"
echo "CA bundle verified: $ACTUAL"
