---
status: "accepted"
date: 2026-09-23
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["release", "security", "update"]
---

# 0008. Ship a bare NRO for in-app updates and a zip for first install

## Context and Problem Statement

The predecessor's self-update downloaded a zip and extracted it (`update_tab.cpp:43-52`,
`utils.cpp:184`). Its content updates went further and extracted archives directly to the SD card
root after `chdir("/")` (`utils.cpp:173`).

Archive extraction on a path derived from remote data is the single largest attack surface in
this application. It brings in a zip parser, path traversal (zip-slip), partial-extraction states
where some files are new and some are old, and free-space failures halfway through. None of that
is needed to replace one file.

But a zip is genuinely the right format for a *first* installation, where a human extracts it to
their SD card root.

## Decision Drivers

* Minimise the code that touches unverified remote bytes.
* An interrupted update must leave a recoverable state.
* First-time installation must stay trivial for a non-technical user.
* Reinstalling must never destroy user settings.

## Considered Options

* Publish both: a bare `.nro` used by the updater, and a `.zip` for first install
* Publish only a `.zip` and extract it in-app
* Publish only a bare `.nro` and have users place it manually

## Decision Outcome

Chosen option: **publish both, with distinct roles**.

| Asset | Consumer | Why |
|---|---|---|
| `nsx-manager-<version>.nro` | the in-app updater | One file. No archive parser, no traversal surface, no partial state. Its SHA-256 is pinned in the manifest and the swap is a rename. |
| `nsx-manager-<version>.zip` | a human, once | Extracts at the SD root. Contains exactly `switch/nsx-manager/nsx-manager.nro` and **nothing else**. |

The zip deliberately carries **no** `config/` entries: `/config/nsx-manager/` is created by the
app at first run, so reinstalling or extracting over an existing installation can never clobber
settings, themes or preserve lists.

`gen_manifest.py` maps these filename patterns to the manifest `kind` (`app-nro`,
`forwarder-nro`, `sd-overlay-zip`) that the client selects by. The client **never parses a
filename** to decide what something is - renaming an asset is a breaking change to the pipeline.

### Consequences

* Good, because the update path has no archive code in it at all. Zip-slip is not mitigated in
  the update path; it is absent from it.
* Good, because a truncated or tampered download is one file to discard, with no half-applied
  state.
* Good, because the bare `.nro` is what makes the forwarder's swap a single rename
  ([ADR-0007](0007-self-update-by-staging-an-nro-and-chainloading-a-forwarder.md)).
* Good, because a user can verify the download by hand against `SHA256SUMS`.
* Bad, because the release carries the same binary twice, roughly doubling release size. At
  around 13 MB this is irrelevant.
* Bad, because two assets must stay in sync. `release.yml` builds the zip *from* the same `.nro`
  it publishes, so they cannot disagree.
* Neutral, because archive extraction still exists for **content** (CFW packs, firmware). That
  code is traversal-guarded and hash-verified, but it is not on the self-update path, so the
  blast radius of a bug in it excludes the application's own binary.

## Pros and Cons of the Options

### Publish both

* Good, because each consumer gets the format that suits it.
* Bad, because of the duplicated bytes.

### Only a zip, extracted in-app

* Good, because one artefact, and it matches what most homebrew ships.
* Bad, because it forces an unzip implementation onto the most security-sensitive path in the
  application for no benefit - the archive contains exactly one file.
* Bad, because extraction has no atomic point: an interruption leaves the installation in an
  undefined state.

### Only a bare .nro

* Good, because it is the smallest possible release.
* Bad, because a first-time user must create `/switch/nsx-manager/` and place the file correctly.
  Every misplacement becomes a support request.

## More Information

* [`docs/reference/update-manifest.md`](../reference/update-manifest.md)
* [`docs/architecture/update-pipeline.md`](../architecture/update-pipeline.md)
* [`docs/user/installation.md`](../user/installation.md)

Revisit if the application ever needs to ship additional runtime files outside the NRO, which
would make the update path an overlay rather than a single-file replacement.
