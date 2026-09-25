#!/usr/bin/env python3
"""Turn the pinned Mozilla CA bundle into a compile-time C++ blob.

devkitPro's switch-curl is built against mbedTLS, which has NO system trust
store. That absence is why the predecessor disabled certificate verification
entirely (download.cpp:140-141, 198-199, 353-354, 472-473) rather than
supplying an anchor. We supply one, compiled in.

A blob rather than a file in romfs is deliberate: a file introduces a
"what if it is missing?" branch, and that branch is exactly where an insecure
fallback gets added later. There is no branch. See ADR-0006.

Usage:
  tools/cacert/pem_to_header.py --pem third_party/cacert/cacert.pem \
                                --out build/generated/nsx/infra/http/ca_bundle.hpp

SPDX-License-Identifier: GPL-3.0-only
"""
from __future__ import annotations

import argparse
import hashlib
import re
import sys
from pathlib import Path

HEADER = '''// SPDX-License-Identifier: GPL-3.0-only
//
// GENERATED FILE - do not edit. Produced by tools/cacert/pem_to_header.py from
// third_party/cacert/cacert.pem (pinned by cacert.pem.sha256).
//
// Source : https://curl.se/ca/cacert.pem  (Mozilla CA store, MPL-2.0)
// SHA-256: {sha}
// Certs  : {count}
// Bytes  : {size}

#pragma once

#include <string_view>

/// @brief Trust anchors for TLS peer verification.
namespace nsx::infra {{

/// @brief The pinned Mozilla CA bundle, PEM encoded.
/// @details Passed to libcurl via @c CURLOPT_CAINFO_BLOB. devkitPro's curl is
///          mbedTLS-backed and has no system trust store, so this blob is the
///          only trust anchor the application has. It is embedded rather than
///          loaded from romfs so there is no "certificates missing" code path
///          that could invite an insecure fallback.
/// @see ADR-0006, docs/architecture/threat-model.md
inline constexpr std::string_view kCaBundlePem =
{body};

/// @brief SHA-256 of the bundle this binary was built with, lowercase hex.
/// @details Logged at startup so a support report identifies the exact trust
///          anchor in use.
inline constexpr std::string_view kCaBundleSha256 = "{sha}";

/// @brief Number of root certificates in the bundle.
inline constexpr int kCaBundleCertCount = {count};

}}  // namespace nsx::infra
'''


def escape(line: str) -> str:
    """Escape one PEM line as a C++ string literal."""
    out = line.replace("\\", "\\\\").replace('"', '\\"')
    return '    "{}\\n"'.format(out)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--pem", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()

    if not args.pem.is_file():
        print("ERROR: {} not found. Run tools/cacert/fetch.sh first.".format(args.pem),
              file=sys.stderr)
        return 1

    raw = args.pem.read_bytes()
    # Mozilla bundle comments carry non-ASCII (CA names such as
    # "Autoridad de Certificacion Firmaprofesional"). Those are stripped
    # below; only base64 PEM blocks are embedded, and those are asserted
    # to be ASCII before they reach the header.
    text = raw.decode("utf-8", errors="strict")

    count = text.count("-----BEGIN CERTIFICATE-----")
    if count == 0:
        print("ERROR: {} contains no certificates".format(args.pem), file=sys.stderr)
        return 1

    sha = hashlib.sha256(raw).hexdigest()

    # Strip comments and blank lines: only the PEM blocks are needed, and the
    # commentary in Mozilla's bundle is roughly a third of the file.
    keep, inside = [], False
    for line in text.splitlines():
        s = line.strip()
        if s == "-----BEGIN CERTIFICATE-----":
            inside = True
        if inside:
            keep.append(s)
        if s == "-----END CERTIFICATE-----":
            inside = False

    body = "\n".join(escape(l) for l in keep)

    # Self-check: every emitted line must be a complete C++ string literal
    # ending in an ESCAPE SEQUENCE, not a real newline. Getting this wrong
    # produces a header that fails to compile with "missing terminating
    # quote", which is a slow way to learn about it.
    lit = re.compile(r'^    "[^"\\]*\\n"$')
    for k, emitted in enumerate(body.split(chr(10))):
        if not lit.match(emitted):
            print("ERROR: generated line {} is not a valid literal: {!r}"
                  .format(k + 1, emitted), file=sys.stderr)
            return 1
    try:
        payload = ("\n".join(keep) + "\n").encode("ascii")
    except UnicodeEncodeError as exc:
        print("ERROR: a PEM block contains non-ASCII: {}".format(exc),
              file=sys.stderr)
        return 1

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(
        HEADER.format(sha=sha, count=count, size=len(payload), body=body),
        encoding="utf-8", newline="\n")

    print("ca_bundle.hpp: {} certs, {} bytes embedded (from {} on disk), sha256 {}"
          .format(count, len(payload), len(raw), sha[:16]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
