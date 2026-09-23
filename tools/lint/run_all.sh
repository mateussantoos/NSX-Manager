#!/bin/sh
# Run every lint the way CI does. One command before you push.
# SPDX-License-Identifier: GPL-3.0-only
set -u
cd "$(git rev-parse --show-toplevel)"

# Pick a Python that actually runs. On Windows, `python3` resolves to the
# Microsoft Store stub, which is present on PATH and fails when executed - so
# testing for existence is not enough, it has to be invoked. In the container
# `python3` is correct and `python` may not exist at all.
PYTHON="${PYTHON:-}"
if [ -z "$PYTHON" ]; then
    for cand in python3 python; do
        if command -v "$cand" >/dev/null 2>&1 && "$cand" -c 'import sys' >/dev/null 2>&1; then
            PYTHON="$cand"
            break
        fi
    done
fi
if [ -z "$PYTHON" ]; then
    echo "run_all: no working python interpreter found" >&2
    exit 1
fi

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
run "$PYTHON" tools/lint/check_i18n.py
run "$PYTHON" tools/assets/generate.py --check

printf '\n'
if [ -n "$FAILED" ]; then
    printf '\033[31mFAILED:%s\033[0m\n' "$FAILED"
    exit 1
fi
printf '\033[32mAll lints passed.\033[0m\n'
