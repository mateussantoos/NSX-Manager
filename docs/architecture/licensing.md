# Licensing

NSX Manager combines GPL-3.0 and GPL-2.0-only code. Those two licences are **incompatible for
linking**. This page explains the problem, the resolution, and the rule that keeps it true.

Decision: [ADR-0011](../adr/0011-license-gplv3-and-isolate-the-gplv2-only-rcm-payload.md).

## The problem

**Borealis is GPL-3.0.** We link it, so the application must be GPL-3.0.

**hekate/BDK is GPL-2.0-*only*.** This is not an inference. The source headers say so:

> `apps/rcm-payload/bdk/display/di.c:5-7` and `bootloader/main.c:6-8` -
> *"under the terms and conditions of the GNU General Public License, **version 2**, as published
> by the Free Software Foundation"*

with **no** "or (at your option) any later version" clause. Without that clause, GPL-2.0-only code
cannot be relicensed forward to GPL-3.0, and GPL-3.0 code cannot be combined into a GPL-2.0-only
work.

The predecessor shipped both in one archive and never addressed this.

## The resolution: they are two programs

The RCM payload is not a library we link. It is a **separate program**:

| | Application | RCM payload |
|---|---|---|
| Processor | ARMv8 (aarch64) | ARM7 |
| Toolchain | devkitA64 | devkitARM |
| Build | CMake | its own Makefile |
| Runtime | under Horizon OS | freestanding, before the OS |
| Load address | standard NRO | `IPL_LOAD_ADDR` |
| Entry | `main()` under libnx | its own reset vector |

They share **no source, no headers, and no link step**. The application treats the built
`nsx_rcm.bin` as **opaque data**: read from `romfs:/`, written to `/payload.bin`, handed to the
bootloader across a reboot. It never parses it, never calls into it, never includes it.

Two independent programs distributed together is **mere aggregation**, which GPLv2 section 2
explicitly permits:

> In addition, mere aggregation of another work not based on the Program with the Program (or
> with a work based on the Program) on a volume of a storage or distribution medium does not
> bring the other work under the scope of this License.

## The rule

> **No source, header, or build artefact may be shared between `src/`, `apps/forwarder/` and
> `apps/rcm-payload/`.**

This is not a style preference. Violating it would make the combined work legally
undistributable.

`tools/lint/check_license_isolation.sh` runs in CI and fails on:

* any `#include` from `src/` or `apps/forwarder/` reaching into `apps/rcm-payload/`, `bdk/` or
  `hekate`;
* any `#include "nsx/..."` inside `apps/rcm-payload/`;
* `apps/rcm-payload/` having a Makefile but no `LICENSE` of its own;
* any first-party source file missing its `SPDX-License-Identifier: GPL-3.0-only` line.

## Component table

| Component | Licence | How it is used |
|---|---|---|
| NSX Manager (`src/`, `apps/forwarder/`, `tools/`, `cmake/`) | **GPL-3.0-only** | First-party |
| Borealis | GPL-3.0 | Linked. This is what fixes the application's licence. |
| zipper | MIT | Linked |
| nlohmann/json | MIT | Header only |
| doctest | MIT | Host tests only; never shipped |
| Mozilla CA bundle | MPL-2.0 | Embedded as data |
| libnx, devkitPro | ISC / zlib | Toolchain; not redistributed |
| **hekate / BDK** (`apps/rcm-payload/`) | **GPL-2.0-only** | **Separate program. Never linked.** |

Full attributions: [`NOTICE`](../../NOTICE).

## File layout

```
/LICENSE                        GPL-3.0-only    the application
/apps/rcm-payload/LICENSE       GPL-2.0-only    BDK/hekate, naehrwert and CTCaer
/third_party/*/LICENSE          upstream        as shipped
/NOTICE                         aggregated attributions
```

Every first-party file begins with:

```cpp
// SPDX-License-Identifier: GPL-3.0-only
```

Every payload file keeps its original GPL-2.0 header **untouched**. Do not "tidy" them.

## If you are contributing

* Adding a dependency? Check its licence is GPL-3.0-compatible, record it in
  `third_party/PINS.md` and `NOTICE`.
* Tempted to share a helper with the payload? **You cannot.** Duplicate it, or it does not go in.
* Fixing something in the payload? That is a contribution to hekate, under GPL-2.0. Upstream it.
* Adding a first-party file? It needs the SPDX line, or CI fails.

## If you are redistributing

You may redistribute NSX Manager under GPL-3.0-only, provided you also pass on the RCM payload
under GPL-2.0-only with its own notices intact, keep the two as separate programs, and provide
source for both. `NOTICE` and this page are the record of which is which.
