---
status: "accepted"
date: 2026-09-23
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["testing", "architecture"]
---

# 0014. Split pure logic into a host-testable core library

## Context and Problem Statement

The predecessor has **zero tests**. That is not laziness - it is a structural consequence. Every
interesting function transitively depended on libnx, curl or Borealis, so nothing could be
compiled off-device, and Switch homebrew cannot run on CI hardware.

The cost is concrete. The version comparison at `main_frame.cpp:53-73` strips non-digits and
calls `std::stoi`. A five-line test would have caught that `4.2.1-hotfix2` becomes `4212` and
outranks `4.2.1`, and that a date-shaped tag overflows and throws. Neither was ever caught,
because the function lived in a file that included `switch.h`.

## Decision Drivers

* The logic most likely to be wrong is pure: version comparison, manifest parsing, path building,
  hashing, state machines.
* Tests must run in CI, in seconds, with no emulator and no hardware.
* One source list. A second hand-maintained file list would drift.
* Test failures must be cheap to diagnose.

## Considered Options

* A pure `core/` library compiled twice - once for Switch, once for the host - tested with doctest
* An on-device test harness NRO, run manually
* An emulator (Ryujinx/yuzu) in CI
* No tests, relying on manual device testing

## Decision Outcome

Chosen option: **keep `core/` free of platform dependencies and compile it a second time with the
host compiler**, tested with [doctest](https://github.com/doctest/doctest).

`cmake --preset host-debug` builds `nsx_core` from the **same `target_sources` list** the Switch
build uses, plus `tests/unit/*`, and runs under `ctest` on `ubuntu-latest` in seconds. A
`host-asan` preset adds ASan and UBSan. This is the direct payoff of
[ADR-0003](0003-use-a-layered-source-tree-with-co-located-headers.md)'s layering rule - it is
what `core` depends on nothing is *for*.

**Host-tested:** `semver` (parse, precedence, the exact cases from the predecessor's bug),
`update_policy`, `manifest` (against the golden fixtures), `handoff` (the swap state machine),
`sd_paths` and `traversal_guard` (the zip-slip surface), `sha256` (NIST vectors, chunk-boundary
splits), `text`, `json`, `mega_link` (pure key derivation, currently trapped inside a curl file).

Note `sha256` is a self-contained implementation rather than an mbedTLS call, specifically so
hashing is testable on the host and mbedTLS is needed only for TLS.

**Device-only:** all of `platform/`, `infra/http`, `infra/archive`, `infra/mega`, all of `ui/`,
the forwarder's rename sequence, the RCM payload. Covered by the numbered smoke checklist in
`docs/contributing/testing.md` plus an on-device self-test screen behind a debug flag.

**Fixtures are specifications.** `tests/fixtures/manifests/` documents exactly what a hostile or
broken `update.json` must produce, and `validate_manifest.py --check-fixtures` runs in CI - so
the parser contract cannot drift silently. The release workflow also runs the manifest it just
generated through that validator before publishing.

**Coverage** on `core/` only. Report-only until the module set settles around 0.3.0, then 80%
line and 70% branch. Never measured on `ui/`, which would only create pressure to write worthless
tests.

### Consequences

* Good, because the bugs that actually shipped in the predecessor are now the first tests written.
* Good, because the feedback loop is seconds on a laptop, not a build-copy-boot cycle.
* Good, because sanitizers can run, which is otherwise impossible on this platform.
* Good, because it makes the layering rule load-bearing rather than decorative: a dependency
  added to `core` breaks the host build immediately.
* Bad, because roughly half the codebase - UI and platform - stays untested by machines. That is
  inherent, and the smoke checklist is the honest mitigation rather than a pretence of coverage.
* Bad, because keeping `core` pure sometimes means an extra interface where a direct call would
  be shorter.
* Neutral, because `core` is compiled twice, so it must build under both GCC 13 on Linux and
  devkitA64. That has caught portability problems before it is a cost.

## Pros and Cons of the Options

### Host-compiled core library with doctest

* Good, because it needs no emulator, no hardware and no special CI runner.
* Good, because one source list serves both builds - nothing to keep in sync.
* Good, because doctest is a single vendored header: Catch2 v3 is a full CMake library roughly
  three times slower to compile, GoogleTest is heavier than a 14k-line homebrew warrants, and
  doctest's `TEST_CASE`/`CHECK` API matches Catch2 closely enough that migrating later is
  mechanical.
* Bad, because it constrains `core` to portable C++.

### On-device test harness NRO

* Good, because it can test the real platform layer, which is the part host tests cannot reach.
* Bad, because it cannot run in CI, so it only catches what someone remembers to run.
* Good enough as a **supplement**, which is the role the debug self-test screen is given.

### Emulator in CI

* Good, because it could in principle run the real binary end to end.
* Bad, because Switch emulators are not designed as CI targets - slow, flaky, and their
  homebrew/filesystem fidelity is exactly where our bugs live.
* Bad, because it would test against emulator behaviour, not console behaviour, so a pass would
  not mean much.

### No tests

* Good, because zero setup.
* Bad, because this is the predecessor's position, and the result is a version comparison that
  offers downgrades and a self-updater that never worked.

## More Information

* [`tests/CMakeLists.txt`](../../tests/CMakeLists.txt)
* [`tests/fixtures/manifests/README.md`](../../tests/fixtures/manifests/README.md)
* [`docs/contributing/testing.md`](../contributing/testing.md)
* [doctest](https://github.com/doctest/doctest)

Revisit if a practical way to run libnx code on CI appears, or if the coverage gate proves to
distort rather than improve the tests.
