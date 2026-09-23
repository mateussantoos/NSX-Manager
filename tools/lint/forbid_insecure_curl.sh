#!/bin/sh
# TLS verification may never be disabled, and no plaintext URL may be used.
#
# The predecessor disabled BOTH peer and host verification on EVERY request:
#   download.cpp:140-141, 198-199, 353-354, 472-473
# and then extracted the unverified archives to the SD card root. Any network
# attacker could replace the CFW pack, the firmware, or the app itself.
#
# The real cause was that devkitPro's curl is mbedTLS-backed and has no system
# trust store, so verification failed and was turned off instead of fixed. We
# ship a CA bundle as a compile-time blob (ADR-0006). There is therefore no
# legitimate reason to ever set these to 0.
# SPDX-License-Identifier: GPL-3.0-only

set -eu
cd "$(git rev-parse --show-toplevel)"

FAIL=0
SEARCH_DIRS=""
for d in src apps/forwarder; do [ -d "$d" ] && SEARCH_DIRS="$SEARCH_DIRS $d"; done
[ -n "$SEARCH_DIRS" ] || { echo "forbid_insecure_curl: no sources yet - OK"; exit 0; }

check() {
    pattern="$1"; message="$2"
    hits="$(grep -rnE "$pattern" $SEARCH_DIRS 2>/dev/null | grep -v '^\s*//' || true)"
    if [ -n "$hits" ]; then
        printf 'FORBIDDEN: %s\n' "$message" >&2
        printf '%s\n\n' "$hits" >&2
        FAIL=1
    fi
}

check 'CURLOPT_SSL_VERIFYPEER[[:space:]]*,[[:space:]]*0' \
      'CURLOPT_SSL_VERIFYPEER must be 1L'
check 'CURLOPT_SSL_VERIFYHOST[[:space:]]*,[[:space:]]*0' \
      'CURLOPT_SSL_VERIFYHOST must be 2L'
check 'CURLOPT_SSL_VERIFYSTATUS[[:space:]]*,[[:space:]]*0' \
      'CURLOPT_SSL_VERIFYSTATUS must not be explicitly disabled'
check '"http://' \
      'plaintext http:// URL - all network access must be https'

if [ "$FAIL" -ne 0 ]; then
    echo "See docs/architecture/threat-model.md and ADR-0006." >&2
    exit 1
fi

echo "forbid_insecure_curl: OK"
