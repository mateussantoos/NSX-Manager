#!/bin/sh
# Cut a release: bump VERSION, commit, and create the annotated tag.
#
# Tags are created ONLY by this script, and only on main, so the invariant
# "tag == VERSION == what CI built" holds by construction rather than by
# remembering. Pushing the tag is left to you - that is the point of no return.
#
#   tools/release/tag.sh 0.2.0
# SPDX-License-Identifier: GPL-3.0-only

set -eu
cd "$(git rev-parse --show-toplevel)"

NEW="${1:-}"
[ -n "$NEW" ] || { echo "usage: $0 <version>   e.g. $0 0.2.0" >&2; exit 2; }
NEW="${NEW#v}"

if ! printf '%s' "$NEW" \
     | grep -qE '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(-[0-9A-Za-z.-]+)?$'; then
    echo "ERROR: '$NEW' is not SemVer 2.0.0 (e.g. 0.2.0 or 1.0.0-rc.1)" >&2
    exit 1
fi

BRANCH="$(git rev-parse --abbrev-ref HEAD)"
[ "$BRANCH" = "main" ] || { echo "ERROR: releases are cut from main, you are on '$BRANCH'" >&2; exit 1; }

[ -z "$(git status --porcelain --ignore-submodules=dirty)" ] || { echo "ERROR: working tree is dirty" >&2; git status --short --ignore-submodules=dirty >&2; exit 1; }

if git rev-parse "v$NEW" >/dev/null 2>&1; then
    echo "ERROR: tag v$NEW already exists. Versions are never reused." >&2
    exit 1
fi

OLD="$(head -n1 VERSION | tr -d '[:space:]')"
printf '%s\n' "$NEW" > VERSION

tools/release/check_version.sh "v$NEW"

git add VERSION
git commit -m "chore(release): bump version to $NEW"
git tag -a "v$NEW" -m "NSX Manager $NEW"

printf '\n  %s -> %s, tag v%s created locally.\n\n' "$OLD" "$NEW" "$NEW"
printf '  Review, then publish:\n'
printf '    git push origin main\n'
printf '    git push origin v%s      <- this is what triggers release.yml\n\n' "$NEW"
printf '  To undo before pushing:\n'
printf '    git tag -d v%s && git reset --hard HEAD~1\n\n' "$NEW"
