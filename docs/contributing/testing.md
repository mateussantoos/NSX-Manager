# Testing

Switch homebrew cannot run on CI hardware, so the logic worth testing is deliberately built not
to need hardware.

Decision: [ADR-0014](../adr/0014-split-pure-logic-into-a-host-testable-core-library.md).

## Host tests

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug --output-on-failure
```

Seconds, no console, no emulator. `nsx_core` is compiled by the host compiler from the **same**
source list the Switch build uses, so there is no second file list to drift.

Sanitizers:

```sh
cmake --preset host-asan && ctest --preset host-asan --output-on-failure
```

> **The `host-asan` preset does not work on Windows.** clang's AddressSanitizer
> does not support the MSVC dynamic debug CRT, so the binary aborts inside
> `ucrtbased.dll` during CRT startup - before `main()`, with no frames in our
> code. It is a toolchain incompatibility, not a finding.
>
> Sanitizers therefore run in CI on Linux (the `sanitizers` job), which is where
> the check is meaningful. On Windows, use `host-debug` locally and let CI do the
> sanitized run.

## What is host-tested

Everything in `core/`. One test binary per module.

| Suite | Covers |
|---|---|
| `semver` | Parsing (strict and tolerant), `v` prefix, leading zeros, overflow, SemVer section 11 precedence, build metadata ignored, malformed input |
| `update_policy` | `(installed, manifest) -> UpdateDecision`; the `mandatory` flag; the `min_supported` gate; an unreadable manifest |
| `manifest` | The golden fixtures; asset selection by `kind`; hex validation; unknown fields tolerated; a future `schema_version` refused |
| `handoff` | Round-trip, attempt increment, missing fields, absolute-path validation, every recovery state |
| `sd_paths`, `traversal_guard` | Path builders; rejection of `..`, absolute entries, backslashes, NUL, drive prefixes |
| `sha256` | NIST vectors, empty input, multi-megabyte streaming, chunk-boundary splits |
| `text`, `json` | Formatting helpers; safe typed getters with wrong-type tolerance |
| `mega_link` | Key and IV derivation against known vectors |

### Write the regression first

Several of these exist because the predecessor shipped the bug. When you fix something, add the
case that would have caught it. For example:

```cpp
TEST_CASE("a prerelease never outranks its release") {
    // predecessor: digit-stripping made "4.2.1-hotfix2" -> 4212 > 421, so it
    // offered a DOWNGRADE as an update (main_frame.cpp:53-73)
    CHECK(compare(*tryParseSemVer("4.2.1-hotfix2"), *tryParseSemVer("4.2.1")) < 0);
}

TEST_CASE("a date-shaped tag does not throw") {
    // predecessor: std::stoi("42120260923") threw std::out_of_range, uncaught
    CHECK_NOTHROW(tryParseSemVer("v4.2.1 (2026-09-23)"));
}
```

## Fixtures are specifications

[`tests/fixtures/manifests/`](../../tests/fixtures/manifests/) documents what a hostile or broken
`update.json` must produce - one rejection reason per file. Same for
[`tests/fixtures/handoff/`](../../tests/fixtures/handoff/).

```sh
python3 tools/release/validate_manifest.py --check-fixtures
```

This runs in CI, so the parser contract cannot drift silently. The release workflow also runs the
manifest it just generated through the same validator before publishing.

## What cannot be host-tested

`platform/`, `infra/http`, `infra/archive`, `infra/mega`, all of `ui/`, the forwarder's rename
sequence, and the RCM payload. These are covered by the checklist below plus an on-device
self-test screen behind a debug flag.

This is stated honestly rather than papered over: roughly half the codebase is not covered by
machines, and the checklist is the mitigation.

## Pre-release smoke checklist

Run before tagging. Record the results in the release pull request.

### Installation

1. Fresh install from the zip on an empty SD card. App launches.
2. Reinstall over an existing install. **Settings survive.**
3. Launch in applet mode. No crash; memory warning shown if applicable.

### Update path - the important part

4. Update from N-1 to N. Completes and relaunches on the new version.
5. Cut the network mid-download. Resumes or fails cleanly; no `.part` left behind.
6. Corrupt the staged NRO by hand. **Swap refuses; the old version still launches.**
7. Set `attempts` to `max_attempts` in `handoff.json`. **Rollback runs, old version restored.**
8. Power off during the download. Next launch cleans up and offers again.
9. Power off between the two renames (`NSX_DEBUG_CRASH_AT_SWAP=1`). **Recovery restores.**
10. Delete `nsx-manager.nro`. Launch **NSX Manager (Repair)** from hbmenu. It recovers.
11. Set the console clock to 2010. Update check fails with **"set your console clock"**, not a
    silent insecure fallback.

### Hardware coverage

12. Erista. 13. Mariko. 14. exFAT SD. 15. FAT32 SD. 16. Low free space - refused before the first
byte.

### Content

17. Install a CFW pack. 18. Download firmware; Daybreak handoff works. 19. Install cheats.
20. Back up and restore a save. 21. Reboot to payload.

### Interface

22. Every tab opens. 23. Touch works on every screen. 24. The language switch takes effect.
25. No string appears untranslated in `pt-BR`.

## Coverage

Measured on `core/` only, with `gcovr`. Report-only until the module set settles around 0.3.0,
then 80% line and 70% branch enforced.

Never measured on `ui/` or `platform/`. A coverage target on code that needs a console to run
only creates pressure to write tests that assert nothing.
