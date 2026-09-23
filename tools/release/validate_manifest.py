#!/usr/bin/env python3
"""Validate update.json against docs/reference/update.schema.json plus the rules
a JSON Schema cannot express.

Run in the release workflow BEFORE publishing. You cannot ship a manifest the
client refuses to read - and the client refuses far more than the schema does:
plaintext URLs, URLs pointing outside the release, names that escape a
directory, and hashes that are not 64 hex characters.

Also runnable against the fixtures, which is how the rules stay honest:
  tools/release/validate_manifest.py --check-fixtures

SPDX-License-Identifier: GPL-3.0-only
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCHEMA_PATH = ROOT / "docs" / "reference" / "update.schema.json"

SEMVER = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(-[0-9A-Za-z.-]+)?$")
SHA256 = re.compile(r"^[0-9a-f]{64}$")
ISO_UTC = re.compile(r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$")
SUPPORTED_SCHEMA = 1
KNOWN_KINDS = {"app-nro", "forwarder-nro", "sd-overlay-zip"}


def semantic_checks(m: dict, errors: list) -> None:
    """The rules that matter to the client but that a schema cannot state."""
    sv = m.get("schema_version")
    if sv != SUPPORTED_SCHEMA:
        errors.append(
            "schema_version is {!r}; this client understands {}. A newer manifest "
            "must make the app say 'update manually', not guess.".format(sv, SUPPORTED_SCHEMA)
        )
        return

    for field in ("version", "min_supported"):
        val = m.get(field)
        if not isinstance(val, str) or not SEMVER.match(val):
            errors.append("{}: {!r} is not SemVer 2.0.0".format(field, val))

    tag = m.get("tag")
    if not isinstance(tag, str) or not tag.startswith("v"):
        errors.append("tag: {!r} must be a string starting with 'v'".format(tag))
    elif isinstance(m.get("version"), str) and tag[1:] != m["version"]:
        errors.append("tag {!r} does not match version {!r}".format(tag, m["version"]))

    if not isinstance(m.get("mandatory"), bool):
        errors.append("mandatory: {!r} must be a boolean".format(m.get("mandatory")))

    if m.get("channel") not in ("stable", "beta"):
        errors.append("channel: {!r} must be 'stable' or 'beta'".format(m.get("channel")))

    pub = m.get("published_at")
    if not isinstance(pub, str) or not ISO_UTC.match(pub):
        errors.append("published_at: {!r} must be ISO-8601 UTC".format(pub))

    assets = m.get("assets")
    if not isinstance(assets, list):
        errors.append("assets: must be a list, got {}".format(type(assets).__name__))
        return

    seen_app = False
    for i, a in enumerate(assets):
        where = "assets[{}]".format(i)
        if not isinstance(a, dict):
            errors.append("{}: must be an object".format(where))
            continue

        name = a.get("name")
        if not isinstance(name, str) or not name:
            errors.append("{}.name: must be a non-empty string".format(where))
        elif "/" in name or "\\" in name or ".." in name or name.startswith("."):
            errors.append(
                "{}.name: {!r} must be a bare filename. A name that can traverse "
                "directories must never reach a filesystem call.".format(where, name)
            )

        url = a.get("url")
        if not isinstance(url, str):
            errors.append("{}.url: must be a string".format(where))
        elif not url.startswith("https://"):
            errors.append("{}.url: {!r} must use https".format(where, url))
        elif not url.startswith("https://github.com/"):
            errors.append("{}.url: {!r} must point at a GitHub release asset".format(where, url))

        sha = a.get("sha256")
        if not isinstance(sha, str) or not SHA256.match(sha):
            errors.append(
                "{}.sha256: must be 64 lowercase hex characters. An asset without a "
                "verifiable digest must never be downloadable.".format(where)
            )

        size = a.get("size")
        if not isinstance(size, int) or isinstance(size, bool) or size <= 0:
            errors.append("{}.size: must be a positive integer, got {!r}".format(where, size))

        kind = a.get("kind")
        if kind not in KNOWN_KINDS:
            errors.append("{}.kind: {!r} is not one of {}".format(where, kind, sorted(KNOWN_KINDS)))
        elif kind == "app-nro":
            seen_app = True

    if not seen_app:
        errors.append("no asset of kind 'app-nro' - the updater has nothing to download")


def validate(path: Path) -> list:
    errors = []
    try:
        raw = path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as exc:
        return ["cannot read: {}".format(exc)]

    try:
        manifest = json.loads(raw)
    except json.JSONDecodeError as exc:
        return ["not valid JSON: {}".format(exc)]

    if not isinstance(manifest, dict):
        return ["top level must be an object"]

    # Schema validation when jsonschema is available; semantic checks always.
    if SCHEMA_PATH.is_file():
        try:
            import jsonschema
        except ImportError:
            print("note: jsonschema not installed - semantic checks only", file=sys.stderr)
        else:
            schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
            validator = jsonschema.Draft202012Validator(schema)
            for e in sorted(validator.iter_errors(manifest), key=lambda e: list(e.path)):
                loc = "/".join(str(p) for p in e.path) or "<root>"
                errors.append("schema: {}: {}".format(loc, e.message))

    semantic_checks(manifest, errors)
    return errors


def check_fixtures() -> int:
    """Every fixture must behave the way tests/fixtures/manifests/README.md says."""
    fx = ROOT / "tests" / "fixtures" / "manifests"
    expectations = {
        "valid.json": True,
        "truncated.json": False,
        "future-schema.json": False,
        "bad-schema.json": False,
        "missing-sha.json": False,
        "hostile-paths.json": False,
    }
    failures = 0
    for name in sorted(expectations):
        should_pass = expectations[name]
        path = fx / name
        if not path.is_file():
            print("MISSING FIXTURE {}".format(name), file=sys.stderr)
            failures += 1
            continue
        errors = validate(path)
        passed = not errors
        mark = "ok " if passed == should_pass else "FAIL"
        verb = "accepted" if passed else "rejected"
        extra = "" if passed else " ({} reason(s))".format(len(errors))
        print("  [{}] {:<22} {}{}".format(mark, name, verb, extra))
        if passed != should_pass:
            failures += 1
            for e in errors[:3]:
                print("         {}".format(e), file=sys.stderr)
    if failures:
        print("\ncheck-fixtures: {} fixture(s) did not behave as documented.".format(failures),
              file=sys.stderr)
        return 1
    print("check-fixtures: all fixtures behave as documented")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("manifest", nargs="?", type=Path)
    ap.add_argument("--check-fixtures", action="store_true")
    args = ap.parse_args()

    if args.check_fixtures:
        return check_fixtures()
    if args.manifest is None:
        ap.error("pass a manifest path, or --check-fixtures")

    errors = validate(args.manifest)
    if errors:
        print("INVALID: {}".format(args.manifest), file=sys.stderr)
        for e in errors:
            print("  - {}".format(e), file=sys.stderr)
        return 1
    print("valid: {}".format(args.manifest))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
