#!/usr/bin/env bash
# Report what the container actually provides.
#
# Run this first when something behaves differently in Docker than it does in
# CI - it answers "is the toolchain what I think it is?" in one screen.
#
#   docker compose run --rm nsx doctor
#
# SPDX-License-Identifier: GPL-3.0-only
set -uo pipefail

ok=0; bad=0
check() {
    local label="$1"; shift
    printf '  %-26s ' "$label"
    if command -v "$1" >/dev/null 2>&1; then
        "$@" 2>/dev/null | head -1
        ok=$((ok + 1))
    else
        printf '\033[31mMISSING\033[0m\n'
        bad=$((bad + 1))
    fi
}

printf '\n\033[1mSwitch toolchain (devkitA64)\033[0m\n'
check "aarch64 g++"      aarch64-none-elf-g++ --version
check "aarch64 gcc"      aarch64-none-elf-gcc --version

printf '\n\033[1mRCM payload toolchain (devkitARM)\033[0m\n'
check "arm gcc"          arm-none-eabi-gcc --version
check "arm objcopy"      arm-none-eabi-objcopy --version

printf '\n\033[1mHost toolchain\033[0m\n'
check "clang++"          clang++ --version
check "cmake"            cmake --version
check "ninja"            ninja --version

printf '\n\033[1mVerification tooling\033[0m\n'
check "clang-format"     clang-format --version
check "clang-tidy"       clang-tidy --version
check "doxygen"          doxygen --version
check "python3"          python3 --version
check "git"              git --version
check "zip"              zip -v

printf '\n\033[1mEnvironment\033[0m\n'
printf '  %-26s %s\n' "DEVKITPRO" "${DEVKITPRO:-<unset>}"
printf '  %-26s %s\n' "DEVKITARM" "${DEVKITARM:-<unset>}"
printf '  %-26s %s\n' "CC / CXX" "${CC:-<unset>} / ${CXX:-<unset>}"

printf '\n\033[1mIntegration points\033[0m\n'
for f in /opt/devkitpro/cmake/Switch.cmake /opt/devkitpro/libnx/switch_rules; do
    printf '  %-26s ' "$(basename "$f")"
    [ -f "$f" ] && echo present || { echo MISSING; bad=$((bad + 1)); }
done
printf '  %-26s ' "jsonschema"
python3 -c 'import jsonschema,sys; sys.stdout.write(jsonschema.__version__+"\n")' 2>/dev/null \
    || { printf '\033[31mMISSING\033[0m\n'; bad=$((bad + 1)); }

printf '\n\033[1mWorkspace\033[0m\n'
printf '  %-26s %s\n' "pwd" "$(pwd)"
printf '  %-26s %s\n' "repo mounted" "$([ -f VERSION ] && echo "yes (VERSION $(cat VERSION))" || echo NO)"
printf '  %-26s %s\n' "submodules" \
    "$([ -f third_party/borealis/library/borealis.mk ] && echo present || echo 'not initialised')"
printf '  %-26s %s\n' "CA bundle" \
    "$([ -f third_party/cacert/cacert.pem ] && echo present || echo 'not fetched (gitignored)')"
printf '  %-26s %s\n' "rcm payload" \
    "$([ -f apps/rcm-payload/Makefile ] && echo present || echo absent)"
printf '  %-26s %s\n' "icon" "$([ -f assets/icon.jpg ] && echo present || echo MISSING)"

printf '\n%d checks passed' "$ok"
[ "$bad" -gt 0 ] && printf ', \033[31m%d missing\033[0m' "$bad"
printf '\n\n'
[ "$bad" -eq 0 ]
