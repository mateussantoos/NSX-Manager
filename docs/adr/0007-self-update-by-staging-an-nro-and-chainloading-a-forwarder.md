---
status: "accepted"
date: 2026-09-23
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["update", "reliability"]
---

# 0007. Self-update by staging an NRO and chainloading a forwarder

## Context and Problem Statement

A running `.nro` cannot overwrite its own file. The predecessor solved this with a second binary:
stage the new build under `/config/`, copy a forwarder out of romfs, write a handoff file, call
`envSetNextLoad(forwarder)` and quit; the forwarder then renames the staged binary over the live
one and chainloads it (`utils.cpp:184-198`, `app-forwarder/source/main.cpp:58-88`).

The approach is sound. The implementation had three defects:

1. **The handoff path was relative.** `app-forwarder/source/main.cpp:28` does
   `std::ifstream file("forwarder.conf")`. It works only because hbmenu happens to `chdir` into
   the NRO's directory. When the open fails there is no error path - every key silently becomes
   an empty string, which is then passed to `rename("")` and `remove("")`.
2. **Two divergent parsers.** The writer at `utils.cpp:539-553` and the reader at
   `app-forwarder/source/main.cpp:26-46` are separate ad-hoc `KEY=value` implementations of the
   same format.
3. **No recovery.** Nothing counted attempts, nothing kept a backup, and nothing detected an
   interrupted swap on the next boot. Power loss mid-swap left an unbootable installation.

## Decision Drivers

* A failed update must never leave the user without a launchable application.
* The handoff format must have exactly one implementation.
* The swap must be recoverable after power loss.
* FatFs rename cannot overwrite an existing file, which constrains the ordering.

## Considered Options

* Keep the forwarder chainload approach, with an absolute JSON handoff, a backup and rollback
* Have the app write the new NRO to a versioned filename and update an hbmenu entry
* Ship only the zip and require the user to update manually

## Decision Outcome

Chosen option: **keep the forwarder chainload**, with all three defects fixed.

**Handoff:** JSON at the absolute path `/config/nsx-manager/staging/handoff.json`, carrying
`schema_version`, `from_version`, `to_version`, `staged_nro`, `staged_sha256`, `target_nro`,
`backup_nro`, `forwarder_nro`, `attempts` and `max_attempts`. Serialised and parsed by
`nsx::core::Handoff` - **one implementation, linked into both binaries**, and host-tested.

**The path is passed explicitly and is never relative:** the forwarder receives
`--handoff=/config/nsx-manager/staging/handoff.json` in `argv`, with a compile-time absolute
fallback (`NSX_HANDOFF_PATH`). A missing or malformed handoff makes the forwarder log and
chainload the existing target unchanged - it never guesses.

**Swap sequence**, ordered so the target is never absent for longer than one rename:

1. Parse the handoff; if `attempts >= max_attempts`, take the rollback branch.
2. Increment `attempts` and rewrite the handoff atomically (tmp file, fsync, rename).
3. Re-verify `staged_sha256` over `staged_nro`. On mismatch, abort without touching the target.
4. Copy `staged_nro` to `target_nro.new`, fsync.
5. Rename `target_nro` to `backup_nro`.
6. Rename `target_nro.new` to `target_nro`.
7. Verify the target exists with the expected size, then delete the backup, the staged file and
   the handoff.
8. `envSetNextLoad(target_nro)`.

Steps 5 and 6 are two renames because FatFs cannot overwrite. The sub-millisecond window where
the target is absent is covered by boot-time recovery **and** by shipping a second forwarder copy
at `/switch/nsx-manager/nsx-forwarder.nro`, which appears in hbmenu as **"NSX Manager (Repair)"**.
That is also why `nsx-forwarder-<version>.nro` is a published release asset.

### Consequences

* Good, because a user always has a launchable entry point, even if the application binary is
  gone.
* Good, because one handoff implementation cannot drift, and its state machine is host-tested
  against fixtures including `relative-path.json`, which encodes the old bug as a test.
* Good, because the staged binary's hash is re-verified by the forwarder, so corruption between
  download and swap is caught.
* Good, because the attempt counter bounds a crash loop instead of letting it repeat forever.
* Bad, because there are now two `.nro` files on the SD card and two entries in hbmenu. The
  second is named "(Repair)" so its purpose is evident.
* Bad, because the forwarder is a second binary to build, test and release.
* Neutral, because the flow still requires two application restarts. That is inherent to the
  platform.

## Pros and Cons of the Options

### Forwarder chainload with rollback

* Good, because the user never leaves the console to complete an update.
* Good, because the failure modes are enumerable and each has a defined recovery.
* Bad, because of the complexity described above, which is irreducible given the platform
  constraint.

### Versioned filename plus an hbmenu entry

* Good, because it avoids overwriting a running file entirely.
* Bad, because the SD card accumulates every version ever installed.
* Bad, because the hbmenu entry the user has muscle memory for would point at an old binary, or
  would need rewriting - which is the same overwrite problem moved elsewhere.

### Manual update only

* Good, because it is by far the simplest and safest thing to build.
* Bad, because it abandons the feature this project exists to fix. The predecessor effectively
  shipped this option by accident, and the result was that nobody ever updated.

## More Information

* [`docs/architecture/self-update-forwarder.md`](../architecture/self-update-forwarder.md)
* [`docs/reference/handoff-format.md`](../reference/handoff-format.md)
* [`tests/fixtures/handoff/`](../../tests/fixtures/handoff/)
* [ADR-0008](0008-ship-a-bare-nro-for-in-app-updates-and-a-zip-for-first-install.md)

Revisit if libnx or hbloader gains a supported self-replacement mechanism.
