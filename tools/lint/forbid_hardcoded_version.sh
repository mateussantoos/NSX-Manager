#!/bin/sh
# /VERSION is the only source of truth for the version number.
#
# The predecessor had FIVE copies that drifted apart:
#   Makefile:25                 APP_VERSION := 4.2.1
#   download.cpp:22             "NSX-Updater/4.2.1 (...)"      <- user agent
#   splash_page.cpp:183         "v4.2.1 - M.S. Edition"        <- footer
#   tools_tab.cpp:321           "v4.2.1"                       <- changelog badge
#   changelog_page.cpp:154      "v4.2.1"                       <- version list
# Bumping the version reliably missed at least one of them.
#
# Use nsx::core::version::kString instead. Always.
# SPDX-License-Identifier: GPL-3.0-only

set -eu
cd "$(git rev-parse --show-toplevel)"

[ -d src ] || { echo "forbid_hardcoded_version: no src/ yet - OK"; exit 0; }

HITS="$(grep -rnE '"v?[0-9]+\.[0-9]+\.[0-9]+' src apps/forwarder 2>/dev/null \
        | grep -v 'version\.hpp\.in' \
        | grep -v '^\s*//' \
        | grep -viE 'min_atmosphere|schema|example|e\.g\.' \
        || true)"

if [ -n "$HITS" ]; then
    echo "FORBIDDEN: hardcoded version literal(s) found." >&2
    echo "$HITS" >&2
    echo "" >&2
    echo "Use nsx::core::version::kString from the generated version.hpp." >&2
    exit 1
fi

echo "forbid_hardcoded_version: OK"
