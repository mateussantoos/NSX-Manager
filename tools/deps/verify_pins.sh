#!/bin/sh
# Every submodule must sit on the commit third_party/PINS.md records.
#
# The predecessor's .gitmodules declared three submodules while the sources were
# committed directly into the repository, and one declared path (aiosu-rcm) did
# not exist at all. `git submodule update --init` failed, local patches were
# indistinguishable from upstream, and upstream fixes could not be pulled.
# See ADR-0009.
# SPDX-License-Identifier: GPL-3.0-only

set -eu
cd "$(git rev-parse --show-toplevel)"

PINS="third_party/PINS.md"
FAIL=0

[ -f .gitmodules ] || { echo "verify_pins: no .gitmodules - OK"; exit 0; }

# 1. Every declared path must exist on disk (the aiosu-rcm failure mode).
git config -f .gitmodules --get-regexp '^submodule\..*\.path$' | while read -r _ path; do
    if [ ! -d "$path" ]; then
        printf 'MISSING: .gitmodules declares %s, which is not present.\n' "$path" >&2
        printf '         Run: git submodule update --init --recursive\n' >&2
        echo 1 >> /tmp/nsx_pins_fail.$$
    fi
done

# 2. No submodule path may also be tracked as ordinary files (the worst of both worlds).
git config -f .gitmodules --get-regexp '^submodule\..*\.path$' | while read -r _ path; do
    if [ -n "$(git ls-files "$path" | grep -v "^$path$" || true)" ]; then
        printf 'CONFLICT: %s is a submodule but also has tracked files.\n' "$path" >&2
        echo 1 >> /tmp/nsx_pins_fail.$$
    fi
done

# 3. The checked-out SHA must match PINS.md.
if [ -f "$PINS" ] && [ -n "$(git submodule status 2>/dev/null || true)" ]; then
    git submodule status | while read -r sha path _; do
        sha="${sha#[-+U]}"
        short="$(printf '%s' "$sha" | cut -c1-12)"
        if grep -q "TODO" "$PINS" && ! grep -q "$short" "$PINS"; then
            printf 'UNPINNED: %s is at %s but PINS.md has no entry.\n' "$path" "$short" >&2
            printf '          Record it in %s in the same commit.\n' "$PINS" >&2
            echo 1 >> /tmp/nsx_pins_fail.$$
        fi
    done
fi

if [ -f /tmp/nsx_pins_fail.$$ ]; then
    FAIL=$(wc -l < /tmp/nsx_pins_fail.$$); rm -f /tmp/nsx_pins_fail.$$
    printf '\nverify_pins: %s problem(s).\n' "$FAIL" >&2
    exit 1
fi
echo "verify_pins: OK"
