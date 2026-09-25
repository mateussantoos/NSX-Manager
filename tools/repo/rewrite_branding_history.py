#!/usr/bin/env python3
"""Rewrite history so the current branding assets are present in every commit.

WHAT THIS DOES
--------------
Injects `assets/bootlogo.png`, `assets/icon.jpg` and `assets/images/logo.png`
into the tree of every commit, with their current content. After running, the
logo appears to have been there from the first commit, and no commit anywhere
in history references the old artwork.

WHAT IT PRESERVES, EXACTLY
--------------------------
  * commit messages, byte for byte
  * author name, author email, author date
  * committer name, committer email, committer date
  * the shape of the commit graph
  * every other file in every tree

WHAT IT CANNOT PRESERVE
-----------------------
**Commit hashes change. All of them.** This is not a limitation of this script
or of git-filter-repo - it is what a content-addressed history means. A commit's
SHA is computed over its tree and its parents, so changing a blob changes the
tree hash, which changes that commit's hash, which changes every descendant's
hash. No tool can rewrite content and keep hashes.

Signatures would also be invalidated, for the same reason. This repository has
none (`git log --format=%G?` reports `N` for every commit), so nothing signed is
being broken here. If that ever changes, re-sign after rewriting.

SAFETY
------
Irreversible. Take a `git bundle` first. This script refuses to run if the
working tree is dirty or if a remote exists, because rewriting published history
breaks every clone.

    python3 tools/repo/rewrite_branding_history.py --dry-run
    python3 tools/repo/rewrite_branding_history.py --execute

SPDX-License-Identifier: GPL-3.0-only
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Files to force into every commit's tree, with their current on-disk content.
BRANDING = [
    "assets/bootlogo.png",
    "assets/icon.jpg",
    "assets/images/logo.png",
]

REGULAR_FILE = b"100644"


def git(*args: str) -> str:
    return subprocess.run(["git", "-C", str(ROOT), *args],
                          capture_output=True, text=True, check=True).stdout.strip()


def preflight() -> list[str]:
    """Reasons not to proceed."""
    problems = []

    dirty = git("status", "--porcelain")
    if dirty:
        problems.append(
            "working tree is dirty ({} path(s)) - commit or stash first, or the "
            "rewrite will silently exclude those changes".format(len(dirty.splitlines())))

    remotes = git("remote")
    if remotes:
        problems.append(
            "remote(s) configured ({}) - rewriting published history breaks every "
            "clone. Remove the remote or do this before publishing.".format(
                remotes.replace("\n", ", ")))

    for rel in BRANDING:
        if not (ROOT / rel).is_file():
            problems.append("missing asset: {}".format(rel))

    return problems


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--dry-run", action="store_true", help="report what would happen")
    g.add_argument("--execute", action="store_true", help="perform the rewrite")
    args = ap.parse_args()

    problems = preflight()
    if problems:
        print("Refusing to rewrite:", file=sys.stderr)
        for p in problems:
            print("  - {}".format(p), file=sys.stderr)
        return 1

    commits = int(git("rev-list", "--count", "HEAD"))
    head = git("rev-parse", "HEAD")

    print("repository : {}".format(ROOT))
    print("commits    : {}".format(commits))
    print("HEAD before: {}".format(head))
    print("injecting  :")
    for rel in BRANDING:
        print("    {:<28} {:>8} bytes".format(rel, (ROOT / rel).stat().st_size))

    if args.dry_run:
        print("\n--dry-run: nothing written. Every commit hash WILL change when executed.")
        return 0

    try:
        from git_filter_repo import Blob, FileChange, FilteringOptions, RepoFilter
    except ImportError:
        print("ERROR: git-filter-repo is not installed.  pip install git-filter-repo",
              file=sys.stderr)
        return 1

    payload = {rel: (ROOT / rel).read_bytes() for rel in BRANDING}
    blob_ids: dict[str, bytes] = {}

    def commit_callback(commit, metadata):
        # Insert the blobs once, on the first commit we see. They must enter the
        # fast-import stream before anything references them, and inserting on
        # every commit would push 26 identical copies through it.
        if not blob_ids:
            for rel, data in payload.items():
                blob = Blob(data)
                rf.insert(blob)
                blob_ids[rel] = blob.id

        # Drop any existing entry for a branding path, then add the current one.
        # Doing both means a commit that already carried the old icon ends up
        # with exactly one entry, holding the new content - rather than two.
        wanted = {p.encode() for p in BRANDING}
        commit.file_changes = [c for c in commit.file_changes if c.filename not in wanted]
        for rel in BRANDING:
            commit.file_changes.append(
                FileChange(b"M", rel.encode(), blob_ids[rel], REGULAR_FILE))

    print("\nrewriting...")

    opts = FilteringOptions.parse_args(["--force", "--quiet"], error_on_empty=False)
    rf = RepoFilter(opts, commit_callback=commit_callback)
    rf.run()

    print("done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
