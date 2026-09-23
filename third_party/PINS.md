# Dependency pins

Every submodule is pinned to an explicit commit. `tools/deps/verify_pins.sh` fails CI when a
checked-out submodule does not match the SHA recorded here, so a drifting dependency cannot
reach `main` unnoticed.

This file exists because the predecessor's `.gitmodules` declared `lib/zipper`, `lib/borealis`
and `aiosu-rcm` while the sources were actually **committed directly** into the repository, and
the third path did not exist at all (the real directory was `app-rcm`). The result was the worst
of both worlds: `git submodule update --init` failed, local patches were indistinguishable from
upstream, and upstream fixes could not be pulled. See ADR-0009.

## Pinned submodules

| Path | Upstream | Pinned commit | Licence | Why this fork |
|---|---|---|---|---|
| `third_party/borealis` | `HamletDuFromage/borealis` | _TODO: set on first `git submodule add`_ | GPL-3.0 | Switch-specific fixes not merged upstream; the predecessor used the same fork |
| `third_party/zipper` | `HamletDuFromage/zipper` | _TODO: set on first `git submodule add`_ | MIT | minizip wrapper matching the Borealis fork |

## Vendored single headers

Not submodules - copied in deliberately, because a whole repository for one header is not worth
the clone cost or the submodule ceremony.

| Path | Upstream | Version | Licence |
|---|---|---|---|
| `third_party/nlohmann/json.hpp` | `nlohmann/json` | _TODO_ | MIT |
| `third_party/doctest/doctest.h` | `doctest/doctest` | _TODO_ | MIT |

## Pinned data

| Path | Source | Retrieved | Licence |
|---|---|---|---|
| `third_party/cacert/cacert.pem` | <https://curl.se/ca/cacert.pem> | _TODO_ | MPL-2.0 |

`cacert.pem` is **gitignored on purpose**: only its SHA-256 is committed, in
`third_party/cacert/cacert.pem.sha256`. `tools/cacert/fetch.sh` downloads it and refuses to
proceed on a hash mismatch, so the trust anchor is reviewed as a one-line diff rather than as a
250 kB blob nobody reads.

## Updating a pin

1. `cd third_party/<dep> && git fetch && git checkout <new-sha>`
2. Build and run the device smoke checklist in `docs/contributing/testing.md`.
3. Update the table above **and** commit the submodule pointer in the same commit.
4. Use `build(deps): bump <dep> to <short-sha>`.

`.github/workflows/deps.yml` opens this as a pull request weekly. It never merges on its own -
a dependency bump always gets a human and a device test.
