#!/bin/sh
# Point git at this repository's hooks and commit template.
#
# Uses core.hooksPath rather than copying into .git/hooks, so a hook change
# arrives with a normal `git pull` instead of needing everyone to re-run this.
# SPDX-License-Identifier: GPL-3.0-only

set -eu
REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

git config core.hooksPath tools/hooks
git config commit.template .gitmessage

chmod +x tools/hooks/commit-msg 2>/dev/null || true

printf 'Installed:\n'
printf '  core.hooksPath   = %s\n' "$(git config core.hooksPath)"
printf '  commit.template  = %s\n' "$(git config commit.template)"
printf '\nCommit messages are now validated locally. CI checks them again.\n'
printf 'To uninstall: git config --unset core.hooksPath && git config --unset commit.template\n'
