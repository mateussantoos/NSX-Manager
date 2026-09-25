#!/bin/sh
# The application and the forwarder are two binaries that must agree on three
# paths, and nothing else makes them.
#
#   handoff   the app writes it, the forwarder reads it
#   target    the app names it in the handoff, the forwarder chainloads it
#   romfs     the app restores the forwarder from it, CMake stages it there
#
# A mismatch is invisible at build time and fails on hardware as "nothing
# happened" - the forwarder finds no handoff, chainloads the unchanged app, and
# the update silently never installs. That is close enough to the predecessor's
# defect to be worth a check: it had two divergent ad-hoc parsers for this same
# file (utils.cpp:555-575 wrote it, app-forwarder/source/main.cpp:26-46 read
# it) and nothing kept them agreeing.
# SPDX-License-Identifier: GPL-3.0-only

set -eu
cd "$(git rev-parse --show-toplevel)"

APP_CONFIG="src/nsx/domain/selfupdate/update_service.hpp"
FWD_MAIN="apps/forwarder/src/main.cpp"
ROMFS_CMAKE="cmake/NsxRomfs.cmake"

FAIL=0

for f in "$APP_CONFIG" "$FWD_MAIN" "$ROMFS_CMAKE"; do
    [ -f "$f" ] || { echo "check_path_agreement: $f not found"; exit 0; }
done

# Pull the first double-quoted string out of the line matching a pattern.
field() {
    grep -m1 -E "$2" "$1" 2>/dev/null | sed -n 's/.*"\([^"]*\)".*/\1/p'
}

# --- the handoff ----------------------------------------------------------
APP_STAGING="$(field "$APP_CONFIG" 'std::string stagingDir\{')"
APP_HANDOFF="${APP_STAGING}/handoff.json"
FWD_HANDOFF="$(field "$FWD_MAIN" 'define NSX_HANDOFF_PATH')"

if [ "$APP_HANDOFF" != "$FWD_HANDOFF" ]; then
    printf 'PATHS: handoff disagrees\n  app       %s (%s)\n  forwarder %s (%s)\n' \
        "$APP_HANDOFF" "$APP_CONFIG" "$FWD_HANDOFF" "$FWD_MAIN" >&2
    FAIL=1
fi

# --- the application binary ----------------------------------------------
APP_TARGET="$(field "$APP_CONFIG" 'std::string targetNro\{')"
FWD_TARGET="$(field "$FWD_MAIN" 'kTargetNro *=')"

if [ "$APP_TARGET" != "$FWD_TARGET" ]; then
    printf 'PATHS: target binary disagrees\n  app       %s (%s)\n  forwarder %s (%s)\n' \
        "$APP_TARGET" "$APP_CONFIG" "$FWD_TARGET" "$FWD_MAIN" >&2
    FAIL=1
fi

# --- where the forwarder is restored from --------------------------------
# The app reads `romfs:/<name>`; CMake stages it as `${NSX_ROMFS_DIR}/<name>`.
APP_SOURCE="$(field "$APP_CONFIG" 'std::string forwarderSource\{')"
SOURCE_NAME="${APP_SOURCE#romfs:/}"

if [ "$SOURCE_NAME" = "$APP_SOURCE" ]; then
    printf 'PATHS: forwarderSource is not a romfs: path (%s)\n' "$APP_SOURCE" >&2
    FAIL=1
elif ! grep -q "NSX_ROMFS_DIR}/${SOURCE_NAME}\"" "$ROMFS_CMAKE"; then
    printf 'PATHS: %s is read from romfs:/%s, but %s does not stage it there\n' \
        "$APP_CONFIG" "$SOURCE_NAME" "$ROMFS_CMAKE" >&2
    FAIL=1
fi

# --- the hbmenu repair entry lives beside the application -----------------
# hbmenu only lists /switch, so a repair entry anywhere else is not launchable.
APP_REPAIR="$(field "$APP_CONFIG" 'std::string repairEntryNro\{')"
case "$APP_REPAIR" in
    /switch/*) ;;
    *)
        printf 'PATHS: repairEntryNro must be under /switch to appear in hbmenu (got %s)\n' \
            "$APP_REPAIR" >&2
        FAIL=1
        ;;
esac

if [ "$FAIL" -ne 0 ]; then
    printf '\ncheck_path_agreement: see docs/reference/sd-layout.md\n' >&2
    exit 1
fi

echo "check_path_agreement: OK"
