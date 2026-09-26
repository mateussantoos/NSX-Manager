---
status: "accepted"
date: 2026-09-26
deciders: ["@mateussantoos"]
supersedes: ["ADR-0009"]
superseded-by: []
tags: ["build", "deps"]
---

# 0019. Consolidate vendored dependencies in root repository

## Context and Problem Statement

ADR-0009 established git submodules for `third_party/borealis` and `third_party/zipper`.
However, managing nested submodules introduced recurring developer friction, multi-repository synchronization
overhead, nested git metadata drift in automated CI environments, and potential detachment during headless Docker builds.
Furthermore, extensive custom patches were applied directly to Borealis (such as the ProgressDisplay calculation fix,
sidebar layout geometry adjustments, and Lucide vector glyph extensions) that diverged from upstream.

## Decision Drivers

* Single repository integrity: a single `git clone` must fetch all sources without requiring recursive submodule initialization.
* Hermetic container builds: Docker builds must not rely on external git network access or nested git index pointers.
* Direct maintenance of first-party fork modifications and zero-debt dependency de-bloating.

## Considered Options

* Maintain external forks and git submodules (ADR-0009).
* Complete in-tree vendoring permanently consolidated into the root repository index.
* CMake FetchContent downloading tarballs during build configuration.

## Decision Outcome

Chosen option: **complete in-tree vendoring permanently consolidated into the root repository index**, superseding ADR-0009.

All `.gitmodules` definitions and nested `.git` directories inside `third_party/` are purged.
Dependencies (`third_party/borealis`, `third_party/zipper`, `third_party/nlohmann`, and `third_party/doctest`)
are tracked as regular trees directly in the primary `nsx-manager` Git tree.
Unnecessary dependency bloat (such as desktop test suites, documentation, and external CI configurations inside `fmt`)
is purged to reduce repository footprint.

### Consequences

* Positive: Cloning is atomic and instantaneous without recursive submodule steps.
* Positive: Docker container builds are fully offline and decoupled from external repository availability.
* Positive: Custom patches to UI components and build glue are versioned atomically alongside the application code.
* Negative: Pulling upstream updates requires manual patch application or tree importing rather than `git submodule update`.
