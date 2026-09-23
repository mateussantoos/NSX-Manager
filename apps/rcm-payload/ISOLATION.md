# Why this directory is walled off

**Do not share code between this directory and the rest of the repository - in either
direction.** Not a header, not a helper, not a build artefact reference.

## The reason

This payload is derived from [hekate/BDK](https://github.com/CTCaer/hekate) and is licensed
**GPL-2.0-only**. That is not an assumption - the file headers say so explicitly:

> `bdk/display/di.c` - *"under the terms and conditions of the GNU General Public License,
> **version 2**, as published by the Free Software Foundation"*

with **no** "or (at your option) any later version" clause.

The NSX Manager application links Borealis, which is GPL-3.0, so the application must be GPL-3.0.
**GPL-2.0-only and GPL-3.0 cannot be combined into a single program.** Linking them would make
the result undistributable.

## How the isolation works

This is a **separate program**, not a library:

| | Application | This payload |
|---|---|---|
| Processor | ARMv8 (aarch64) | ARM7 |
| Toolchain | devkitA64 | devkitARM |
| Build | CMake | this directory's own `Makefile` |
| Runtime | under Horizon OS | freestanding, before the OS |
| Load address | standard NRO | `IPL_LOAD_ADDR := 0x40008000` |

The application treats the built `app_rcm.bin` as **opaque data**: it is copied into romfs by
`cmake/NsxRcmPayload.cmake`, read back at runtime, and written to `/payload.bin` for the
bootloader. Nothing ever parses it, calls into it, or includes a header from it.

Two independent programs shipped in one archive is **mere aggregation**, which GPLv2 section 2
explicitly permits.

## Rules

1. Never add `#include "nsx/..."` to a file here.
2. Never include anything from here into `src/` or `apps/forwarder/`.
3. Never add an SPDX `GPL-3.0-only` line to a file here - these files keep their original
   GPL-2.0 headers untouched.
4. The `Makefile` stays as vendored. Our naming happens in `cmake/NsxRcmPayload.cmake`, not here.
5. Fixing a bug in this code means contributing to [hekate](https://github.com/CTCaer/hekate)
   under GPL-2.0. Upstream it.

`tools/lint/check_license_isolation.sh` enforces rules 1-3 in CI.

Full argument: [`docs/architecture/licensing.md`](../../docs/architecture/licensing.md) and
[ADR-0011](../../docs/adr/0011-license-gplv3-and-isolate-the-gplv2-only-rcm-payload.md).
