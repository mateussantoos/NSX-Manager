#!/bin/sh
# Keep the GPL-2.0-only RCM payload a SEPARATE PROGRAM.
#
# apps/rcm-payload/ is hekate/BDK code licensed GPL-2.0 with NO "or any later
# version" clause (see apps/rcm-payload/bdk/display/di.c). Borealis is GPL-3.0,
# so the application must be GPL-3.0 - and GPL-2.0-only and GPL-3.0 cannot be
# combined into one program.
#
# The resolution is hard isolation: different toolchain, different Makefile,
# different load address, and NO shared source. The app treats the built payload
# as opaque data. This script is what keeps that true.
#
# See ADR-0011 and docs/architecture/licensing.md.
# SPDX-License-Identifier: GPL-3.0-only

set -eu
cd "$(git rev-parse --show-toplevel)"

FAIL=0

# --- 1. no includes crossing the boundary, in either direction --------------
if [ -d src ] || [ -d apps/forwarder ]; then
    hits="$(grep -rnE --include='*.c' --include='*.h' --include='*.cpp' --include='*.hpp'                 '#include.*(rcm-payload|bdk/|hekate)' src apps/forwarder 2>/dev/null || true)"
    if [ -n "$hits" ]; then
        echo "FORBIDDEN: application code includes from the GPL-2.0-only payload:" >&2
        echo "$hits" >&2
        FAIL=1
    fi
fi

if [ -d apps/rcm-payload ]; then
    # Source files only. ISOLATION.md documents this rule by quoting the
    # forbidden include, and scanning prose would make the doc trip its own check.
    hits="$(grep -rnE --include='*.c' --include='*.h' --include='*.cpp' --include='*.hpp'                 --include='*.S' --include='*.s'                 '#include.*"nsx/' apps/rcm-payload 2>/dev/null || true)"
    if [ -n "$hits" ]; then
        echo "FORBIDDEN: the GPL-2.0-only payload includes GPL-3.0 application code:" >&2
        echo "$hits" >&2
        FAIL=1
    fi

    if [ -f apps/rcm-payload/Makefile ] && [ ! -f apps/rcm-payload/LICENSE ]; then
        echo "FORBIDDEN: apps/rcm-payload/ must carry its own GPL-2.0-only LICENSE" >&2
        FAIL=1
    fi
fi

# --- 2. every first-party source declares its licence -----------------------
for dir in src apps/forwarder; do
    [ -d "$dir" ] || continue
    for f in $(find "$dir" -name '*.cpp' -o -name '*.hpp' -o -name '*.in' 2>/dev/null); do
        if ! head -n 5 "$f" | grep -q 'SPDX-License-Identifier: GPL-3.0-only'; then
            printf 'MISSING SPDX: %s\n' "$f" >&2
            FAIL=1
        fi
    done
done

if [ "$FAIL" -ne 0 ]; then
    echo "" >&2
    echo "See docs/architecture/licensing.md." >&2
    exit 1
fi

echo "check_license_isolation: OK"
