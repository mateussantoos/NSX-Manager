# Architecture overview

NSX Manager is a Nintendo Switch homebrew application: a single `.nro` launched from the homebrew
menu, written in C++20 against devkitA64 and libnx, with a [Borealis](../adr/0010-use-the-borealis-fork-as-the-ui-framework.md)
user interface.

This page is the map. Follow the links for detail.

## The shape

Six layers, dependencies pointing **downward only**.

```
                      +---------------------+
                      |        app          |  composition root; owns main()
                      +----------+----------+
                                 |
                      +----------v----------+
                      |         ui          |  Borealis views. No I/O, no rules.
                      +----------+----------+
                                 |
                      +----------v----------+
                      |       domain        |  use-cases; orchestration
                      +----------+----------+
                                 |
                      +----------v----------+
                      |        infra        |  HTTP, GitHub, archives, storage
                      +----------+----------+
                                 |
                      +----------v----------+
                      |      platform       |  thin libnx wrappers
                      +----------+----------+
                                 |
                      +----------v----------+
                      |        core         |  pure C++20. Host-compilable.
                      +---------------------+
```

The rule is enforced, not merely agreed: [`tools/lint/check_layering.sh`](../../tools/lint/check_layering.sh)
reads every `#include "nsx/..."` and fails CI on an upward or sideways edge. See
[`layering.md`](layering.md).

**Why the bottom layer matters most.** `core` has no libnx, no curl and no Borealis, so it
compiles with an ordinary host compiler. That is what makes version comparison, manifest parsing,
path construction and hashing testable in seconds on a laptop - and those are exactly the places
the predecessor's bugs lived. See [ADR-0014](../adr/0014-split-pure-logic-into-a-host-testable-core-library.md).

## Three binaries

| Binary | Toolchain | Built by | Role |
|---|---|---|---|
| `nsx-manager.nro` | devkitA64 | this CMake tree | The application |
| `nsx-forwarder.nro` | devkitA64 | this CMake tree | Completes the self-update swap; also the **repair** entry point |
| `nsx_rcm.bin` | devkitARM | its own Makefile | RCM payload. **GPL-2.0-only, never linked.** |

The third is walled off for licensing reasons that are real, not procedural - see
[`licensing.md`](licensing.md) and [ADR-0011](../adr/0011-license-gplv3-and-isolate-the-gplv2-only-rcm-payload.md).

## What happens at startup

1. `app/main.cpp` initialises libnx services and mounts romfs.
2. `app/container.cpp` wires concrete implementations into the interfaces `domain` expects - the
   only place in the codebase that knows every layer.
3. `app/bootstrap.cpp` runs the startup sequence:
   - check for an interrupted update (`staging/handoff.json`) and recover if needed;
   - delete stale `*.part` files;
   - load settings and the i18n catalogue;
   - if the manifest cache is older than six hours and the network is up, refresh it in the
     background.
4. The UI shell opens. **The startup path never blocks on the network** and never calls
   `api.github.com`.

## How an update happens

Summarised here; the full sequence and failure matrix are in
[`update-pipeline.md`](update-pipeline.md).

```
fetch update.json  ->  compare SemVer  ->  download .nro to *.part
                                              |
                                        hash during write
                                              |
                              size + sha256 match?  --no-->  discard, report
                                              | yes
                                       rename into staging
                                              |
                                      write handoff.json
                                              |
                              envSetNextLoad(forwarder), quit
                                              |
                          forwarder: re-verify, backup, swap, verify
                                              |
                                    chainload the new binary
```

Two properties matter and are non-negotiable:

* **Nothing unverified is ever executed, extracted or renamed into place.**
* **A failure leaves a bootable installation** - the previous binary is kept until the new one is
  confirmed, and the repair NRO is always present.

## Where the interesting code lives

| Concern | Module |
|---|---|
| Version comparison | `core/version/semver` |
| "Should we update?" | `core/update/update_policy` |
| Manifest parsing | `core/update/manifest` |
| The swap state machine | `core/update/handoff` |
| Path safety | `core/paths/traversal_guard` |
| Integrity | `core/hash/sha256` |
| TLS configuration | `infra/http/tls_config` |
| Manifest fetch, cache, backoff | `infra/github` |
| Update orchestration | `domain/selfupdate` |

## Reading order

1. [`layering.md`](layering.md) - the dependency rule and why it is enforced
2. [`source-tree.md`](source-tree.md) - every directory, and where the predecessor's god-files went
3. [`build-system.md`](build-system.md) - CMake targets, presets, version single-sourcing
4. [`update-pipeline.md`](update-pipeline.md) - the update path end to end
5. [`self-update-forwarder.md`](self-update-forwarder.md) - the swap and its recovery states
6. [`threat-model.md`](threat-model.md) - trust boundaries
7. [`licensing.md`](licensing.md) - the GPL-2.0/GPL-3.0 problem and its resolution
8. [`../adr/README.md`](../adr/README.md) - why each of the above is the way it is
