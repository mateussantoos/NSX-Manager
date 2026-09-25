#!/bin/sh
# Enforce the dependency rule from ADR-0003 at the #include level.
#
#   core     -> (nothing but the C++ stdlib)
#   platform -> core
#   infra    -> core, platform
#   domain   -> core, infra
#   ui       -> core, domain
#   app      -> everything
#
# Compilers do not enforce layering; nothing stops a UI file from including a
# curl header. This does. It is the difference between "we agreed on layers"
# and "we have layers".
# SPDX-License-Identifier: GPL-3.0-only

set -eu
cd "$(git rev-parse --show-toplevel)"

SRC="src/nsx"
FAIL=0

allowed_for() {
    case "$1" in
        core)     echo "core" ;;
        platform) echo "core platform" ;;
        infra)    echo "core platform infra" ;;
        domain)   echo "core infra domain" ;;
        ui)       echo "core domain ui" ;;
        app)      echo "core platform infra domain ui app" ;;
        *)        echo "" ;;
    esac
}

[ -d "$SRC" ] || { echo "check_layering: $SRC not found"; exit 0; }

for file in $(find "$SRC" -name '*.cpp' -o -name '*.hpp' 2>/dev/null); do
    layer="$(printf '%s' "$file" | sed -E "s|^$SRC/([a-z_]+)/.*|\1|")"
    allowed="$(allowed_for "$layer")"
    [ -n "$allowed" ] || continue

    # Project includes look like:  #include "nsx/<layer>/<module>/<file>.hpp"
    grep -oE '^[[:space:]]*#include[[:space:]]+"nsx/[a-z_]+/' "$file" 2>/dev/null \
    | sed -E 's|.*"nsx/([a-z_]+)/|\1|' | sort -u \
    | while read -r dep; do
        [ -n "$dep" ] || continue
        if ! printf '%s\n' $allowed | grep -qx "$dep"; then
            printf 'LAYERING: %s (layer %s) may not include nsx/%s/\n' "$file" "$layer" "$dep" >&2
            echo 1 >> /tmp/nsx_layering_fail.$$
        fi
    done

    # core must not reach the platform at all.
    if [ "$layer" = "core" ]; then
        if grep -qE '^[[:space:]]*#include[[:space:]]+<(switch\.h|curl/|borealis)' "$file" 2>/dev/null; then
            printf 'LAYERING: %s is in core and must not include libnx, curl or borealis\n' "$file" >&2
            echo 1 >> /tmp/nsx_layering_fail.$$
        fi
    fi
done

# Licence isolation belongs to check_license_isolation.sh, but an include that
# crosses into the GPL-2.0 payload is also a layering violation - catch it here
# too, cheaply.
if [ -d "$SRC" ] && grep -rn 'rcm-payload' "$SRC" 2>/dev/null | grep -q '#include'; then
    echo "LAYERING: src/ must never include from apps/rcm-payload/ (see ADR-0011)" >&2
    echo 1 >> /tmp/nsx_layering_fail.$$
fi

if [ -f /tmp/nsx_layering_fail.$$ ]; then
    FAIL=$(wc -l < /tmp/nsx_layering_fail.$$)
    rm -f /tmp/nsx_layering_fail.$$
    printf '\ncheck_layering: %s violation(s). See docs/architecture/layering.md\n' "$FAIL" >&2
    exit 1
fi

echo "check_layering: OK"
