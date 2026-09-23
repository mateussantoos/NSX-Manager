#!/usr/bin/env python3
"""Generate every branding derivative from the one master logo.

`assets/bootlogo.png` is the single source of truth for the project's visual
identity. Everything else - the NRO icon, the in-app logo - is derived from it
by this script, so a rebrand is "replace the master, re-run this" rather than
hunting for images by hand and missing one.

    python3 tools/assets/generate.py            # write the derivatives
    python3 tools/assets/generate.py --check    # verify they are up to date

`--check` runs in CI. It compares against `assets/derivatives.json`, which records the
master hash the derivatives were built from - it does NOT re-encode, because Pillow
output differs between versions and that would report a false "stale" on any machine
whose Pillow differs from whoever last generated.

SPDX-License-Identifier: GPL-3.0-only
"""
from __future__ import annotations

import argparse
import io
import json
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow is required.  pip install Pillow", file=sys.stderr)
    raise SystemExit(1)

ROOT = Path(__file__).resolve().parents[2]
MASTER = ROOT / "assets" / "bootlogo.png"

# Each derivative states WHY it is that size and format.
DERIVATIVES = [
    {
        "path": ROOT / "assets" / "icon.jpg",
        "size": (256, 256),
        "format": "JPEG",
        # devkitPro's nx_generate_nacp requires exactly 256x256 JPEG. Anything
        # else is rejected by nacptool, or renders wrong in the homebrew menu.
        "why": "NRO icon shown by hbmenu (nacptool requires 256x256 JPEG)",
        "opts": {"quality": 92, "optimize": True, "subsampling": 0},
    },
    {
        "path": ROOT / "assets" / "images" / "logo.png",
        "size": (512, 512),
        "format": "PNG",
        # Staged into romfs, so it is carried in every copy of the binary.
        # 512 is enough for the splash at 720p/1080p without bloating the NRO -
        # the 4500x4500 master would add half a megabyte for no visible gain.
        "why": "in-app splash and about screen (romfs)",
        "opts": {"optimize": True},
    },
]


def render(spec: dict) -> bytes:
    """Produce one derivative in memory."""
    with Image.open(MASTER) as src:
        img = src.copy()

    # LANCZOS for downscaling: this is a large reduction (4500 -> 256) and a
    # cheaper filter visibly aliases fine detail in a logo.
    img = img.resize(spec["size"], Image.Resampling.LANCZOS)

    if spec["format"] == "JPEG":
        # JPEG has no alpha. The master is fully opaque, so compositing onto
        # white changes nothing - but doing it explicitly means a future master
        # WITH transparency degrades predictably instead of turning black.
        if img.mode in ("RGBA", "LA", "P"):
            img = img.convert("RGBA")
            flat = Image.new("RGB", img.size, (255, 255, 255))
            flat.paste(img, mask=img.getchannel("A"))
            img = flat
        else:
            img = img.convert("RGB")

    buf = io.BytesIO()
    img.save(buf, spec["format"], **spec["opts"])
    return buf.getvalue()


LOCK = ROOT / "assets" / "derivatives.json"


def sha256(data: bytes) -> str:
    import hashlib
    return hashlib.sha256(data).hexdigest()


def describe(path: Path) -> dict:
    """Record what a derivative IS, independent of how it was encoded."""
    with Image.open(path) as im:
        size, fmt, mode = im.size, im.format, im.mode
    return {
        "sha256": sha256(path.read_bytes()),
        "bytes": path.stat().st_size,
        "width": size[0],
        "height": size[1],
        "format": fmt,
        "mode": mode,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true",
                    help="verify derivatives match the recorded master; write nothing")
    args = ap.parse_args()

    if not MASTER.is_file():
        print("ERROR: master logo missing: {}".format(MASTER), file=sys.stderr)
        return 1

    master_sha = sha256(MASTER.read_bytes())

    # ---------------------------------------------------------------- check --
    #
    # Deliberately does NOT re-encode. Pillow's PNG and JPEG output differs
    # between versions, so comparing freshly encoded bytes reports a false
    # "stale" on a machine whose Pillow differs from whoever last generated -
    # which is exactly what happened between a developer host and the container.
    #
    # Instead: compare against derivatives.json, which records the master hash
    # the derivatives were built from plus each derivative's own hash and
    # geometry. That catches both real failures - master changed without
    # regenerating, and a derivative edited by hand - and is identical in every
    # environment.
    if args.check:
        if not LOCK.is_file():
            print("MISSING: {} - run without --check to create it".format(
                LOCK.relative_to(ROOT)), file=sys.stderr)
            return 1

        lock = json.loads(LOCK.read_text(encoding="utf-8"))
        problems = 0

        if lock.get("master_sha256") != master_sha:
            print("STALE: {} changed since the derivatives were generated.".format(
                MASTER.relative_to(ROOT)), file=sys.stderr)
            print("       recorded {}".format(lock.get("master_sha256", "?")[:16]),
                  file=sys.stderr)
            print("       actual   {}".format(master_sha[:16]), file=sys.stderr)
            problems += 1

        for spec in DERIVATIVES:
            rel = str(spec["path"].relative_to(ROOT)).replace("\\", "/")
            recorded = lock.get("derivatives", {}).get(rel)

            if not spec["path"].is_file():
                print("MISSING: {}".format(rel), file=sys.stderr)
                problems += 1
                continue
            if recorded is None:
                print("UNRECORDED: {}".format(rel), file=sys.stderr)
                problems += 1
                continue

            actual = describe(spec["path"])
            if actual["sha256"] != recorded["sha256"]:
                print("MODIFIED: {} does not match derivatives.json".format(rel),
                      file=sys.stderr)
                problems += 1
            elif (actual["width"], actual["height"]) != tuple(spec["size"]) \
                    or actual["format"] != spec["format"]:
                print("WRONG SHAPE: {} is {}x{} {}, expected {}x{} {}".format(
                    rel, actual["width"], actual["height"], actual["format"],
                    spec["size"][0], spec["size"][1], spec["format"]), file=sys.stderr)
                problems += 1
            else:
                print("  ok     {:<28} {}x{} {}".format(
                    rel, actual["width"], actual["height"], actual["format"]))

        if problems:
            print("\n{} problem(s). Regenerate with:  "
                  "python3 tools/assets/generate.py".format(problems), file=sys.stderr)
            return 1
        print("assets: derivatives match the master")
        return 0

    # ------------------------------------------------------------- generate --
    lock = {"master": str(MASTER.relative_to(ROOT)).replace("\\", "/"),
            "master_sha256": master_sha,
            "derivatives": {}}

    for spec in DERIVATIVES:
        rel = str(spec["path"].relative_to(ROOT)).replace("\\", "/")
        data = render(spec)
        spec["path"].parent.mkdir(parents=True, exist_ok=True)
        spec["path"].write_bytes(data)
        info = describe(spec["path"])
        info["why"] = spec["why"]
        lock["derivatives"][rel] = info
        print("  wrote  {:<28} {}x{} {:<4} {:>7} bytes   {}".format(
            rel, spec["size"][0], spec["size"][1], spec["format"], len(data), spec["why"]))

    # Explicit LF: Path.write_text uses the platform newline, which on Windows
    # produces CRLF and trips tools/lint/check_encoding.sh.
    with io.open(LOCK, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(json.dumps(lock, indent=2, sort_keys=True) + "\n")
    print("  wrote  {:<28} record of what was generated".format(
        str(LOCK.relative_to(ROOT)).replace("\\", "/")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
