#!/bin/sh
# Run every lint the way CI does. One command before you push.
# SPDX-License-Identifier: GPL-3.0-only
set -u
cd "$(git rev-parse --show-toplevel)"

FAILED=""
run() {
    printf '\n\033[1m==> %s\033[0m\n' "$1"
    if "$@"; then :; else FAILED="$FAILED $1"; fi
}

run tools/lint/check_encoding.sh
run tools/lint/check_format.sh
run tools/lint/check_layering.sh
run tools/lint/forbid_hardcoded_version.sh
run tools/lint/forbid_insecure_curl.sh
run tools/lint/check_license_isolation.sh
run tools/lint/check_adr_index.sh
run python3 tools/lint/check_i18n.py
run python3 tools/assets/generate.py --check

printf '\n'
if [ -n "$FAILED" ]; then
    printf '\033[31mFAILED:%s\033[0m\n' "$FAILED"
    exit 1
fi
printf '\033[32mAll lints passed.\033[0m\n'
