#!/usr/bin/env python3
"""Generate update.json, the manifest the in-app updater reads.

Published as a release asset under a STABLE filename, so
  https://github.com/<owner>/<repo>/releases/latest/download/update.json
always resolves to the newest one. That URL is a github.com web redirect to the
release CDN - NOT api.github.com - so it does not consume the unauthenticated
REST budget of 60 requests/hour/IP. The predecessor called the REST API twice on
every single launch and got 403s (commit 7b3cdf6); ETag caching would not have
fixed it, because an unauthenticated 304 still decrements the budget.

See ADR-0005, docs/reference/update-manifest.md, docs/reference/update.schema.json.

Usage:
  tools/release/gen_manifest.py --tag v0.1.0 --dist dist/ --out dist/update.json

SPDX-License-Identifier: GPL-3.0-only
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import re
import sys
from pathlib import Path

REPO = "mateussantoos/nsx-manager"
SCHEMA_VERSION = 1
SEMVER = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(-[0-9A-Za-z.-]+)?$")

# Which asset filenames map to which manifest `kind`. The client selects by
# `kind`, never by parsing the filename.
KINDS = [
    (re.compile(r"^nsx-manager-[^/]+\.nro$"), "app-nro"),
    (re.compile(r"^nsx-forwarder-[^/]+\.nro$"), "forwarder-nro"),
    (re.compile(r"^nsx-manager-[^/]+\.zip$"), "sd-overlay-zip"),
]


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def kind_for(name: str) -> str | None:
    for pattern, kind in KINDS:
        if pattern.match(name):
            return kind
    return None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--tag", required=True, help="release tag, e.g. v0.1.0")
    ap.add_argument("--dist", required=True, type=Path, help="directory holding the built assets")
    ap.add_argument("--out", required=True, type=Path, help="where to write update.json")
    ap.add_argument("--channel", default="stable", choices=["stable", "beta"])
    ap.add_argument("--mandatory", action="store_true",
                    help="block the main menu until the user updates")
    ap.add_argument("--min-supported", default=None,
                    help="oldest version that may update in-app (default: this version)")
    ap.add_argument("--min-atmosphere", default="1.7.0")
    ap.add_argument("--repo", default=REPO)
    args = ap.parse_args()

    if not args.tag.startswith("v"):
        print(f"ERROR: tag '{args.tag}' must start with 'v'", file=sys.stderr)
        return 1

    version = args.tag[1:]
    if not SEMVER.match(version):
        print(f"ERROR: '{version}' is not SemVer 2.0.0", file=sys.stderr)
        return 1

    min_supported = args.min_supported or version
    if not SEMVER.match(min_supported):
        print(f"ERROR: --min-supported '{min_supported}' is not SemVer 2.0.0", file=sys.stderr)
        return 1

    if not args.dist.is_dir():
        print(f"ERROR: {args.dist} is not a directory", file=sys.stderr)
        return 1

    base = f"https://github.com/{args.repo}/releases/download/{args.tag}"
    assets = []

    for path in sorted(args.dist.iterdir()):
        if not path.is_file():
            continue
        kind = kind_for(path.name)
        if kind is None:
            continue
        assets.append({
            "name": path.name,
            "kind": kind,
            "url": f"{base}/{path.name}",
            "size": path.stat().st_size,
            "sha256": sha256(path),
        })

    if not any(a["kind"] == "app-nro" for a in assets):
        print("ERROR: no app-nro asset found - the updater would have nothing to download.",
              file=sys.stderr)
        print(f"       looked in {args.dist} for nsx-manager-<version>.nro", file=sys.stderr)
        return 1

    manifest = {
        "schema_version": SCHEMA_VERSION,
        "product": "nsx-manager",
        "version": version,
        "tag": args.tag,
        "published_at": dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "channel": args.channel,
        "mandatory": bool(args.mandatory),
        "min_supported": min_supported,
        "min_atmosphere": args.min_atmosphere,
        "changelog_url": f"https://github.com/{args.repo}/releases/tag/{args.tag}",
        "release_notes_url": f"{base}/RELEASE_NOTES.md",
        "assets": assets,
    }

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    print(f"wrote {args.out} ({len(assets)} asset(s))")
    for a in assets:
        print(f"  {a['kind']:<16} {a['name']:<32} {a['size']:>10}  {a['sha256'][:16]}...")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
