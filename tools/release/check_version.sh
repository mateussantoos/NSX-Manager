#!/bin/sh
# Refuse to release when the tag and /VERSION disagree.
#
# This is the guard that makes "the tag is the version" true rather than
# aspirational. Run first in release.yml, before anything is built.
#
#   tools/release/check_version.sh v0.1.0
#   GITHUB_REF_NAME=v0.1.0 tools/release/check_version.sh
# SPDX-License-Identifier: GPL-3.0-only

set -eu
cd "$(git rev-parse --show-toplevel)"

TAG="${1:-${GITHUB_REF_NAME:-}}"
[ -n "$TAG" ] || { echo "usage: $0 <tag>   (or set GITHUB_REF_NAME)" >&2; exit 2; }

FILE_VERSION="$(head -n1 VERSION | tr -d '[:space:]')"
TAG_VERSION="${TAG#v}"

if [ "$TAG" = "$TAG_VERSION" ]; then
    echo "ERROR: tag '$TAG' must start with 'v' (e.g. v$FILE_VERSION)" >&2
    exit 1
fi

if [ "$FILE_VERSION" != "$TAG_VERSION" ]; then
    echo "ERROR: version mismatch" >&2
    echo "  VERSION file : $FILE_VERSION" >&2
    echo "  git tag      : $TAG (-> $TAG_VERSION)" >&2
    echo "" >&2
    echo "Bump VERSION and commit it before tagging. tools/release/tag.sh does both." >&2
    exit 1
fi

if ! printf '%s' "$FILE_VERSION" \
     | grep -qE '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-[0-9A-Za-z.-]+)?$'; then
    echo "ERROR: '$FILE_VERSION' is not SemVer 2.0.0" >&2
    exit 1
fi

echo "check_version: OK ($TAG == VERSION $FILE_VERSION)"
