#!/usr/bin/env python3
"""Generate every branding derivative from the one master logo.

`assets/bootlogo.png` is the single source of truth for the project's visual
identity. Everything else - the NRO icon, the in-app logo - is derived from it
by this script, so a rebrand is "replace the master, re-run this" rather than
hunting for images by hand and missing one.

    python3 tools/assets/generate.py            # write the derivatives
    python3 tools/assets/generate.py --check    # verify they are up to date

`--check` runs in CI: it regenerates in memory and compares, so a master that
changed without its derivatives being refreshed fails the build instead of
shipping a half-rebranded application.

SPDX-License-Identifier: GPL-3.0-only
"""
from __future__ import annotations

import argparse
import io
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


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--check", action="store_true",
                    help="verify derivatives match the master; write nothing")
    args = ap.parse_args()

    if not MASTER.is_file():
        print("ERROR: master logo missing: {}".format(MASTER), file=sys.stderr)
        return 1

    stale = 0
    for spec in DERIVATIVES:
        rel = spec["path"].relative_to(ROOT)
        data = render(spec)

        if args.check:
            if not spec["path"].is_file():
                print("MISSING: {}".format(rel), file=sys.stderr)
                stale += 1
            elif spec["path"].read_bytes() != data:
                print("STALE:   {} does not match the master".format(rel), file=sys.stderr)
                stale += 1
            else:
                print("  ok     {:<28} {}x{} {}".format(
                    str(rel), spec["size"][0], spec["size"][1], spec["format"]))
            continue

        spec["path"].parent.mkdir(parents=True, exist_ok=True)
        spec["path"].write_bytes(data)
        print("  wrote  {:<28} {}x{} {:<4} {:>7} bytes   {}".format(
            str(rel), spec["size"][0], spec["size"][1], spec["format"],
            len(data), spec["why"]))

    if args.check and stale:
        print("\n{} derivative(s) out of date. Run:  "
              "python3 tools/assets/generate.py".format(stale), file=sys.stderr)
        return 1
    if args.check:
        print("assets: derivatives match the master")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
