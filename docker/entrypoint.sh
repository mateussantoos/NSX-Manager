#!/usr/bin/env bash
# Task runner for the NSX Manager container.
#
# Every task here is the SAME command CI runs - not an approximation of it. If
# `nsx-entrypoint lint` passes in the container, the `lint` job passes too,
# because they invoke the same scripts.
#
#   docker compose run --rm nsx <task>
#
# SPDX-License-Identifier: GPL-3.0-only

set -euo pipefail

usage() {
    cat <<'USAGE'
NSX Manager build environment

  switch             configure + build the Switch release (nro + forwarder + rcm)
  switch-debug       same, debug configuration
  rcm                build only the RCM payload (devkitARM)
  host               configure + build the host library and tests
  test               host build, then ctest
  asan               host build under ASan/UBSan, then ctest
  lint               every check in tools/lint, exactly as CI runs them
  archive            extraction against real hostile zip archives
  export             build the Switch targets and copy the .nro files to dist/
  format             rewrite sources in place with clang-format
  tidy               clang-tidy over the host compilation database
  docs               doxygen (warnings are errors)
  manifest           validate the update.json fixtures and a generated manifest
  release <tag>      package a release into dist/ the way release.yml does
  verify             lint + test + manifest + docs - the full pre-push gate
  all                verify, then the Switch build
  doctor             report what the container provides, and what is missing
  shell              an interactive shell with the toolchain on PATH
  help               this message

Anything else is executed verbatim, so `docker compose run --rm nsx cmake --version`
works too.
USAGE
}

banner() { printf '\n\033[1;36m==> %s\033[0m\n' "$*"; }

require_devkit() {
    [ -d "${DEVKITPRO:-/opt/devkitpro}/devkitA64" ] || {
        echo "devkitA64 missing from the image - rebuild it" >&2
        exit 1
    }
}

# The CA bundle is gitignored (only its hash is committed), so a fresh clone has
# no PEM and the Switch configure step would fail on it. Fetch-and-verify is
# idempotent and cheap, so just make sure it is there.
ensure_cacert() {
    if [ ! -f third_party/cacert/cacert.pem ]; then
        banner "Fetching the CA bundle (gitignored; hash is pinned)"
        tools/cacert/fetch.sh
    fi
}

ensure_submodules() {
    if [ ! -f third_party/borealis/library/borealis.mk ]; then
        banner "Initialising submodules"
        git submodule update --init --recursive
    fi
}

task="${1:-help}"
[ $# -gt 0 ] && shift || true

case "$task" in
    switch|switch-release)
        require_devkit; ensure_submodules; ensure_cacert
        banner "Configuring switch-release"
        cmake --preset switch-release
        banner "Building switch-release"
        cmake --build --preset switch-release "$@"
        banner "Artefacts"
        find build/switch-release -name '*.nro' -o -name 'nsx_rcm.bin' | sort
        ;;

    switch-debug)
        require_devkit; ensure_submodules; ensure_cacert
        cmake --preset switch-debug
        cmake --build --preset switch-debug "$@"
        ;;

    rcm)
        banner "Building the RCM payload (devkitARM, GPL-2.0-only, isolated)"
        make -C apps/rcm-payload "$@"
        ls -l apps/rcm-payload/output/
        ;;

    host)
        banner "Configuring host-debug"
        cmake --preset host-debug
        banner "Building host-debug"
        cmake --build --preset host-debug "$@"
        ;;

    test)
        banner "Host build"
        cmake --preset host-debug
        cmake --build --preset host-debug
        banner "ctest"
        ctest --preset host-debug --output-on-failure "$@"
        ;;

    asan)
        banner "Host build with ASan + UBSan"
        cmake --preset host-asan
        cmake --build --preset host-asan
        banner "ctest under sanitizers"
        ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1:abort_on_error=1}" \
        UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}" \
            ctest --preset host-asan --output-on-failure "$@"
        ;;

    export)
        # The build tree lives in a NAMED VOLUME, not the bind mount (see
        # docker-compose.yml), so build/ is not reachable from the host at all -
        # `cp` from there in a one-off container writes into that same volume
        # and vanishes with it. This copies into dist/, which IS the bind mount.
        "$0" switch
        banner "Exporting to dist/"
        mkdir -p dist
        for artefact in             build/switch-release/src/nsx/app/nsx-manager.nro             build/switch-release/apps/forwarder/nsx-forwarder.nro             build/switch-release/apps/ui-probe/nsx-ui-probe.nro             build/switch-release/romfs/nsx_rcm.bin
        do
            if [ -f "$artefact" ]; then
                cp "$artefact" "dist/$(basename "$artefact")"
            else
                echo "  missing: $artefact" >&2
            fi
        done
        # Sizes and digests so a copy to the SD card can be checked rather than
        # assumed - a truncated NRO fails on the console with no explanation.
        (cd dist && ls -l ./*.nro ./*.bin 2>/dev/null | sed 's|^|  |')
        echo
        (cd dist && sha256sum ./*.nro ./*.bin 2>/dev/null | sed 's|^|  |')
        cat <<'WHERE'

  Copy to the SD card:
    dist/nsx-manager.nro    -> /switch/nsx-manager/nsx-manager.nro
    dist/nsx-forwarder.nro  -> /switch/nsx-manager/nsx-forwarder.nro   ("NSX Manager (Repair)")
    dist/nsx-ui-probe.nro   -> /switch/nsx-ui-probe.nro                (throwaway; delete after)

  nsx_rcm.bin ships inside nsx-manager.nro's romfs - it does not go on the card.
  Note: `release` rebuilds dist/ from scratch and will remove these.
WHERE
        ;;

    archive)
        banner "Extraction against real archives"
        bash tests/integration/zip_extraction/run.sh
        ;;

    lint)
        banner "Lint"
        tools/lint/run_all.sh
        ;;

    format)
        banner "Formatting sources in place"
        found=$(find src apps/forwarder -name '*.cpp' -o -name '*.hpp' 2>/dev/null || true)
        if [ -z "$found" ]; then
            echo "no first-party sources yet"
        else
            echo "$found" | xargs clang-format -i --style=file
            echo "$found" | wc -l | xargs printf '%s file(s) formatted\n'
        fi
        ;;

    tidy)
        banner "clang-tidy"
        cmake --preset host-debug >/dev/null
        find src -name '*.cpp' -print0 2>/dev/null \
            | xargs -0 -r clang-tidy -p build/host-debug --quiet
        ;;

    docs)
        banner "Doxygen (warnings are errors)"
        cd docs && doxygen Doxyfile
        ;;

    manifest)
        banner "Manifest fixtures"
        python3 tools/release/validate_manifest.py --check-fixtures
        banner "Generated manifest round-trip"
        tmp=$(mktemp -d)
        head -c 4096 /dev/urandom > "$tmp/nsx-manager-0.0.0-ci.nro"
        head -c 1024 /dev/urandom > "$tmp/nsx-forwarder-0.0.0-ci.nro"
        python3 tools/release/gen_manifest.py --tag v0.0.0-ci --dist "$tmp" --out "$tmp/update.json"
        python3 tools/release/validate_manifest.py "$tmp/update.json"
        rm -rf "$tmp"
        ;;

    release)
        tag="${1:?usage: release <tag>   e.g. release v0.1.0}"
        require_devkit; ensure_submodules; ensure_cacert
        banner "Guard: tag must match VERSION"
        tools/release/check_version.sh "$tag"
        version="${tag#v}"
        banner "Building switch-release"
        cmake --preset switch-release
        cmake --build --preset switch-release
        banner "Packaging"
        rm -rf dist staging && mkdir -p dist staging/switch/nsx-manager
        cp "$(find build/switch-release -name 'nsx-manager.nro' | head -1)" \
           "dist/nsx-manager-${version}.nro"
        cp "$(find build/switch-release -name 'nsx-forwarder.nro' | head -1)" \
           "dist/nsx-forwarder-${version}.nro"
        cp "dist/nsx-manager-${version}.nro" staging/switch/nsx-manager/nsx-manager.nro
        (cd staging && zip -qr "../dist/nsx-manager-${version}.zip" switch)
        rm -rf staging
        banner "Manifest and checksums"
        python3 tools/release/gen_manifest.py --tag "$tag" --dist dist --out dist/update.json
        python3 tools/release/validate_manifest.py dist/update.json
        (cd dist && sha256sum ./*.nro ./*.zip > SHA256SUMS && cat SHA256SUMS)
        ;;

    verify)
        "$0" lint
        "$0" test
        "$0" archive
        "$0" manifest
        "$0" docs
        printf '\n\033[1;32mAll verification passed.\033[0m\n'
        ;;

    all)
        "$0" verify
        "$0" switch
        ;;

    doctor)
        exec bash /workspace/docker/doctor.sh
        ;;

    shell|sh|bash)
        exec bash "$@"
        ;;

    help|-h|--help)
        usage
        ;;

    *)
        exec "$task" "$@"
        ;;
esac
