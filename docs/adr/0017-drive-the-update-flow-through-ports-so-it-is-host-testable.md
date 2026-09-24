---
status: "accepted"
date: 2026-09-24
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["testing", "architecture", "update"]
---

# 0017. Drive the update flow through ports so it is host-testable

## Context and Problem Statement

[ADR-0014](0014-split-pure-logic-into-a-host-testable-core-library.md) made `core/` host-testable
and stopped there, on the reasoning that everything above it needs a console. That reasoning holds
for `platform/` and `ui/`. It does not hold for the thing the 0.2.x milestone actually adds.

`domain/selfupdate` is a **sequence**, and the sequence is where the risk lives. Each rule it
applies is already tested in isolation - `planManifestFetch`, `parseManifest`, `decideUpdate`,
`afterFailure` - but none of those tests can catch the orchestration defects that matter: caching
a document the client cannot parse, so every launch for the next six hours re-reads the same
failure from disk; writing a handoff before confirming there is a forwarder able to act on it;
reporting "up to date" when the check merely failed, which is the predecessor's defining bug
(`download.cpp:493-500`).

Those are ordering mistakes. Leaving them to the pre-release device checklist means discovering
them by bricking an installation, one slow manual run at a time.

## Decision Drivers

* The orchestration is the part most likely to be wrong, and the part hardest to reach on hardware.
* Failure states - a torn cache, an exhausted backoff, a missing forwarder - must be reachable in a
  test. On a console most of them require a power cut at a chosen instant.
* One source list per layer. A second hand-maintained file list would drift, which is the defect
  `Makefile:84`'s wildcard already demonstrates in the predecessor.
* No test-only code paths in shipped binaries. A `#ifdef TESTING` branch is a branch nobody runs.

## Considered Options

* **Ports in `domain`, adapters beside them, and a portable source subset per layer**
* Test the orchestration on-device only, via the smoke checklist
* Link the real `CurlClient` in tests and point it at a local HTTP server
* `#ifdef` a mock mode into the production classes

## Decision Outcome

Chosen option: **define the update flow's dependencies as abstract ports and split each layer's
source list into a portable subset and a device subset.**

`UpdateService` takes `Clock`, `FileStore` and `HttpGateway` by reference. It names no concrete
type, so it compiles with a host compiler. The concrete adapters - `SdFileStore`, `CurlGateway` -
live beside it in `domain/selfupdate/` and are listed in `NSX_DOMAIN_DEVICE_SOURCES`, which only
the Switch build adds.

The ports are expressed in the vocabulary `infra/http` already uses (`Response`,
`ExpectedArtifact`, `HttpError`) rather than a parallel set of types. `CurlGateway` is therefore a
forwarding shim with nothing to get wrong, and every security decision stays in `CurlClient` where
there is one place to audit it (ADR-0016).

`infra` splits the same way: `tls_config.cpp` is pure enum-and-string handling and joins the host
build, so the error vocabulary the UI renders is available to a test.

### Consequences

* Good: the whole update flow is exercised in `tests/unit/selfupdate_test.cpp` - cache hit, 304,
  mirror fallback, backoff, digest retry, insufficient space, missing forwarder - with no console.
* Good: the loop closes. The last test feeds the handoff `UpdateService` writes into
  `decideRecovery`, the function the forwarder actually runs, so producer and consumer are checked
  against each other rather than against an assumption.
* Good: the adapters are small enough to read in full, which is what makes "untested" acceptable
  for them specifically.
* Bad: two source lists per split layer. Mitigated by `NSX_<LAYER>_SOURCES` being derived from the
  subsets rather than typed twice, so a file can be forgotten but never duplicated.
* Bad: the ports are one more indirection to follow when reading the flow.
* Neutral: `SdFileStore` repeats the file helpers `apps/forwarder/src/swap.cpp` also has. That is
  deliberate and stays - the forwarder links `nsx_core` alone so the repair binary has the
  smallest dependency surface that can be built, and sharing this code would widen it.

### Confirmation

`cmake --preset host-debug` builds `nsx_core`, the portable subsets of `nsx_infra` and
`nsx_domain`, and the suites; `ctest` runs them. The Switch build adds the device subsets, and the
`switch` container task fails if either adapter stops compiling or linking.

The guards this ADR exists to protect were confirmed by mutation: reverting the
"never cache an unreadable manifest" check, the forwarder-before-handoff ordering, the digest
retry bound, the future-timestamp cache rule and the backoff overflow guard each turned a suite
red.

## More Information

* [`docs/architecture/update-pipeline.md`](../architecture/update-pipeline.md) - the sequence and
  its failure matrix
* [ADR-0007](0007-self-update-by-staging-an-nro-and-chainloading-a-forwarder.md) - what the handoff
  this service writes is for
* [ADR-0014](0014-split-pure-logic-into-a-host-testable-core-library.md) - the boundary this
  extends rather than replaces
