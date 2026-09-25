#!/usr/bin/env python3
"""Verify that every locale agrees with en-US on keys and placeholders.

en-US is the source of truth (ADR-0012). A locale may not invent keys, may not
omit keys, and may not change the number of `{}` placeholders in a string -
a mismatched placeholder count is a formatting crash waiting for a translator.

This exists because the predecessor's i18n was fake: `resources/i18n/en-US/menus.json`
contained Portuguese, and eight of its twelve locale directories were
byte-identical copies of each other. `ja/menus.json` read
"O {} ({}) usa a licenca GPL-3.0". The app shipped twelve languages and spoke one.

SPDX-License-Identifier: GPL-3.0-only
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

SOURCE_LOCALE = "en-US"
PLACEHOLDER = re.compile(r"\{[^}]*\}")


def flatten(obj, prefix=""):
    """Flatten nested JSON into dotted keys -> string values."""
    out = {}
    if isinstance(obj, dict):
        for k, v in obj.items():
            out.update(flatten(v, f"{prefix}.{k}" if prefix else k))
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            out.update(flatten(v, f"{prefix}[{i}]"))
    else:
        out[prefix] = obj
    return out


def load_locale(d: Path) -> dict:
    strings = {}
    for f in sorted(d.glob("*.json")):
        try:
            data = json.loads(f.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, UnicodeDecodeError) as exc:
            print(f"ERROR: {f} is not valid UTF-8 JSON: {exc}", file=sys.stderr)
            raise SystemExit(1)
        for k, v in flatten(data, f.stem).items():
            strings[k] = v
    return strings


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    i18n = root / "assets" / "i18n"

    if not i18n.is_dir():
        print("check_i18n: no assets/i18n/ yet - OK")
        return 0

    src_dir = i18n / SOURCE_LOCALE
    if not src_dir.is_dir():
        print(f"ERROR: the source locale {SOURCE_LOCALE} is missing", file=sys.stderr)
        return 1

    source = load_locale(src_dir)
    if not source:
        print("check_i18n: en-US has no strings yet - OK")
        return 0

    failures = 0
    locales = sorted(p for p in i18n.iterdir() if p.is_dir() and p.name != SOURCE_LOCALE)

    for loc in locales:
        target = load_locale(loc)

        missing = sorted(set(source) - set(target))
        extra = sorted(set(target) - set(source))

        for k in missing:
            print(f"{loc.name}: MISSING key '{k}'", file=sys.stderr)
            failures += 1
        for k in extra:
            print(f"{loc.name}: EXTRA key '{k}' (not in {SOURCE_LOCALE})", file=sys.stderr)
            failures += 1

        for k in sorted(set(source) & set(target)):
            s, t = source[k], target[k]
            if not isinstance(s, str) or not isinstance(t, str):
                continue
            ns, nt = len(PLACEHOLDER.findall(s)), len(PLACEHOLDER.findall(t))
            if ns != nt:
                print(
                    f"{loc.name}: key '{k}' has {nt} placeholder(s), "
                    f"{SOURCE_LOCALE} has {ns}",
                    file=sys.stderr,
                )
                failures += 1

    if failures:
        print(f"\ncheck_i18n: {failures} problem(s) across {len(locales)} locale(s).",
              file=sys.stderr)
        print("See docs/contributing/coding-style.md and ADR-0012.", file=sys.stderr)
        return 1

    print(f"check_i18n: OK ({len(source)} keys, {len(locales)} translated locale(s))")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
