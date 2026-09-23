---
status: "accepted"
date: 2026-09-23
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["legal", "architecture"]
---

# 0011. License GPLv3 and isolate the GPL-2.0-only RCM payload

## Context and Problem Statement

Two components pull the project in incompatible directions.

**Borealis is GPL-3.0.** Linking it means the application must be GPL-3.0.

**The RCM payload is hekate/BDK-derived and GPL-2.0-*only*.** This is not an assumption - the
headers are explicit. `app-rcm/bdk/display/di.c:5-7` and `bootloader/main.c:6-8` read:

> under the terms and conditions of the GNU General Public License, **version 2**, as published
> by the Free Software Foundation

with **no** "or (at your option) any later version" clause.

GPL-2.0-only and GPL-3.0 are mutually incompatible for linking. This is a real legal constraint,
not a formality, and the predecessor never addressed it.

## Decision Drivers

* Ship legally, and be able to explain why in one page.
* Do not fork or relicense upstream code we do not own.
* Keep the constraint enforceable rather than remembered.
* Keep the reboot-to-payload feature, which users rely on.

## Considered Options

* GPL-3.0 application with the payload as a hard-isolated separate program
* Relicense the whole project GPL-2.0-only
* Drop the RCM payload entirely
* Replace Borealis with a non-GPL UI framework

## Decision Outcome

Chosen option: **GPL-3.0-only application, with the RCM payload kept as a separate program** -
which is already how the code behaves, now made explicit and enforced.

The payload is:

* a **freestanding ARM7 program**, not aarch64 application code;
* built by a **different toolchain** (devkitARM, not devkitA64);
* built from **its own Makefile**, with its own linker script and load address;
* never `#include`d, never linked, and sharing no source with the application.

The application treats the built `nsx_rcm.bin` as **opaque data**: read from romfs, written to
`/payload.bin`, handed to the bootloader on reboot.

Two independent programs distributed in one archive is **mere aggregation**, which GPLv2 section
2 explicitly permits:

> In addition, mere aggregation of another work not based on the Program with the Program (or
> with a work based on the Program) on a volume of a storage or distribution medium does not
> bring the other work under the scope of this License.

**Layout:**

| Path | Licence |
|---|---|
| `/LICENSE` | GPL-3.0-only - the application |
| `/apps/rcm-payload/LICENSE` | GPL-2.0-only - BDK/hekate, naehrwert and CTCaer |
| `/third_party/borealis/LICENSE` | GPL-3.0, upstream |
| `/third_party/zipper/LICENSE` | MIT, upstream |
| `/NOTICE` | aggregated attributions |

Every first-party file carries `SPDX-License-Identifier: GPL-3.0-only`. Every payload file keeps
its original GPL-2.0 header **untouched**.

### Consequences

* Good, because the distribution is defensible and the argument is written down where a reviewer
  can check it.
* Good, because `tools/lint/check_license_isolation.sh` fails CI on an include crossing the
  boundary in either direction, on a payload directory without its own LICENSE, or on a
  first-party file missing its SPDX line. The isolation cannot erode quietly.
* Good, because the separation is also good architecture - the payload is genuinely a different
  program for a different processor.
* Bad, because the application can never share a helper with the payload, so any duplicated
  logic must stay duplicated. Accepted; the overlap is near zero in practice.
* Bad, because the constraint must be explained to every contributor who wonders why
  `apps/rcm-payload/` is walled off. That is what this ADR and
  `docs/architecture/licensing.md` are for.
* Neutral, because upstreaming a payload fix means contributing to hekate under GPL-2.0.

## Pros and Cons of the Options

### GPL-3.0 application, isolated payload

* Good, because nothing upstream is forked or relicensed.
* Good, because it matches what the code already does, so there is no restructuring cost.
* Bad, because of the permanent no-code-sharing rule.

### Relicense everything GPL-2.0-only

* Good, because it would remove the incompatibility at a stroke.
* Bad, because **we cannot**: Borealis is GPL-3.0 and we do not own it. This option is not
  available, only imaginable.

### Drop the RCM payload

* Good, because the licensing problem disappears completely.
* Bad, because reboot-to-payload and the clean-install merge are features users depend on.
* Bad, because it would not actually help: the payload is not the problem - combining it would
  be. Isolation already solves that.

### Replace Borealis with a non-GPL framework

* Good, because a permissively licensed UI would let the application be MIT or similar, removing
  every GPL question.
* Bad, because no comparable Switch-native gamepad UI toolkit exists (ADR-0010), so this would
  mean writing one.
* Bad, because it optimises for a licensing convenience at the cost of the single largest chunk
  of functionality we get for free.

## More Information

* [`docs/architecture/licensing.md`](../architecture/licensing.md) - per-component table
* [`NOTICE`](../../NOTICE)
* [`tools/lint/check_license_isolation.sh`](../../tools/lint/check_license_isolation.sh)
* [hekate](https://github.com/CTCaer/hekate)

Revisit if hekate ever relicenses to GPL-2.0-or-later, which would make the incompatibility
disappear - though the architectural separation would still be worth keeping.
