---
status: "accepted"
date: 2026-09-23
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["release", "versioning"]
---

# 0004. Adopt Semantic Versioning from 0.1.0 with v-prefixed tags

## Context and Problem Statement

The predecessor sat at `4.2.1`, a number inherited from the upstream `aio-switch-updater` fork
and unrelated to anything in that codebase. It appeared as a literal in **five** places that
drifted independently:

| Location | Copy |
|---|---|
| `Makefile:25` | `APP_VERSION := 4.2.1` |
| `download.cpp:22` | `"NSX-Updater/4.2.1 (Nintendo Switch; Atmosphere)"` |
| `splash_page.cpp:183` | `"v4.2.1 - M.S. Edition"` |
| `tools_tab.cpp:321` | `"v4.2.1"` |
| `changelog_page.cpp:154` | `"v4.2.1"` |

There were no git tags at all, so nothing connected a released binary to a commit.

Worse, the comparison logic stripped every non-digit character and compared the result as one
integer (`main_frame.cpp:53-73`): `"v4.2.1"` became `421`. A hotfix tagged `4.2.1-hotfix2` became
`4212` and was offered as an *upgrade* over `4.2.1`. A date-style tag produced more digits than
an `int` holds, and the unguarded `std::stoi` threw `std::out_of_range`, terminating the app.

## Decision Drivers

* An ordering that is correct, including for pre-releases.
* Exactly one place a version number exists.
* A version string that ties a binary to a commit and a release.
* Honesty about maturity while the rewrite is incomplete.

## Considered Options

* SemVer 2.0.0 starting at `0.1.0`, tags `vX.Y.Z`
* SemVer continuing the line at `5.0.0`
* CalVer (`2026.09.1`)

## Decision Outcome

Chosen option: **SemVer 2.0.0 starting at `0.1.0`**, with annotated tags `v<major>.<minor>.<patch>`.

* `/VERSION` at the repository root holds the bare version, no `v`. It is the only source of
  truth; `cmake/NsxVersion.cmake` reads and validates it, and `configure_file` projects it into
  `nsx::core::version::kString`, the NACP, and the curl User-Agent.
* Tags carry the `v` prefix - `softprops/action-gh-release`, `git-cliff`, GitHub's compare UI and
  `on: push: tags` globs all expect it by default.
* `0.x` means explicitly pre-release: the SD layout and public API may change in any minor.
  `1.0.0` is cut against the parity checklist in
  [`PROJECT_SCOPE.md` section 19](../PROJECT_SCOPE.md#19-road-to-100---parity-checklist), not on
  a date.
* The client implements SemVer section 11 precedence properly, including that a pre-release ranks
  **below** the corresponding release.

### Consequences

* Good, because `4.2.1-hotfix2 < 4.2.1` and `1.0.0-rc.1 < 1.0.0` are now correct by definition
  rather than by accident.
* Good, because `tools/lint/forbid_hardcoded_version.sh` fails CI on any version literal under
  `src/`, so the five-copy problem cannot recur.
* Good, because `tools/release/check_version.sh` refuses to release when the tag and `/VERSION`
  disagree, making "the tag is the version" an invariant.
* Good, because `0.x` sets honest expectations while the rewrite is incomplete.
* **Bad, because the version number goes backwards from the user's perspective** - someone
  running "NSX Updater 4.2.1" sees "NSX Manager 0.1.0" and may read it as a downgrade. Accepted
  and mitigated: it is a different product with a different binary name and different SD paths,
  and the old self-updater never worked, so no automatic path exists to be confused by. The
  release notes and the installation guide say so explicitly.
* Neutral, because pre-1.0 means no stability promise. That is accurate, not a cost.

## Pros and Cons of the Options

### SemVer from 0.1.0

* Good, because the number describes *this* codebase rather than inheriting a fork's history.
* Good, because `0.x` communicates instability in a way users and tooling both understand.
* Bad, because of the perceived downgrade described above.

### SemVer continuing at 5.0.0

* Good, because the number keeps rising, so continuity is preserved for users.
* Bad, because `5.0.0` would claim a stability the rewrite does not have on day one - the
  opposite of what `0.x` communicates.
* Bad, because it implies a lineage in the code, and there is none: no source is carried over.

### CalVer

* Good, because a release date is immediately obvious.
* Bad, because it carries no compatibility information, and the manifest has a `min_supported`
  field that is meaningless without one.
* Bad, because date-shaped versions are exactly what overflowed the predecessor's comparison.

## More Information

* [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html)
* [`cmake/NsxVersion.cmake`](../../cmake/NsxVersion.cmake)
* [`tools/release/check_version.sh`](../../tools/release/check_version.sh)
* [`docs/contributing/releasing.md`](../contributing/releasing.md)

Revisit at `1.0.0` to confirm the pre-release policy, and if a maintenance branch for an older
minor ever becomes necessary.
