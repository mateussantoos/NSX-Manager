---
status: "accepted"
date: 2026-09-23
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["build", "deps"]
---

# 0009. Vendor dependencies as pinned git submodules

## Context and Problem Statement

The predecessor's `.gitmodules` declared three submodules - `lib/zipper`, `lib/borealis` and
`aiosu-rcm` - while:

* the sources for the first two were **committed directly** into the repository as ordinary
  tracked files (`git ls-files lib` returns real paths, `git submodule status` is empty);
* the third declared path, `aiosu-rcm`, **did not exist at all** - the real directory was
  `app-rcm`.

This is the worst of both worlds. `git submodule update --init` fails on a nonexistent path,
local patches cannot be distinguished from upstream code, upstream fixes cannot be pulled, and
`git status` is permanently confusing.

## Decision Drivers

* A build must be reproducible: the same commit must produce the same dependency versions.
* Local modifications must be visible as modifications.
* Upstream security fixes must be pullable.
* The clone must not be painful for a contributor.

## Considered Options

* Git submodules pinned by explicit SHA, recorded in a `PINS.md`
* Vendoring full copies of dependency source into the repository
* CMake `FetchContent` / `ExternalProject`
* A package manager (Conan, vcpkg)

## Decision Outcome

Chosen option: **git submodules under `third_party/`, pinned by explicit commit SHA**, with two
deliberate exceptions vendored as single headers.

| Dependency | Mechanism | Why |
|---|---|---|
| Borealis | submodule | Large, forked, occasionally patched upstream |
| zipper | submodule | Small but genuinely upstream |
| nlohmann/json | vendored single header | A whole repository for one header is not worth the clone cost |
| doctest | vendored single header | Same; test-only, never shipped on device |
| Mozilla CA bundle | fetched and hash-pinned | Not code; see ADR-0006 |

`third_party/PINS.md` records every pin with its upstream URL, SHA, licence and the reason a fork
is used. `tools/deps/verify_pins.sh` runs in CI and fails when a declared submodule path does not
exist, when a submodule path is *also* tracked as ordinary files, or when a checked-out SHA has
no entry in `PINS.md`. The first two checks exist specifically to make the predecessor's failure
mode impossible.

Wrapper `CMakeLists.txt` files live in `third_party/`, **outside** the submodules, so
`git submodule update` never conflicts with our build glue.

### Consequences

* Good, because `git log` on a submodule pointer shows exactly when a dependency moved and in
  which commit.
* Good, because local changes to a dependency are impossible to make accidentally - they show up
  as a dirty submodule.
* Good, because `deps.yml` can propose bumps weekly as a pull request, never merged
  automatically: a dependency bump in software that writes executables to an SD card gets a human
  and a device test.
* Bad, because contributors must remember `--recurse-submodules`. The README and
  `docs/contributing/local-setup.md` both lead with it, and `verify_pins.sh` prints the exact
  command to run when they forget.
* Bad, because submodules are widely disliked and unfamiliar to some contributors.
* Neutral, because the wrapper CMake files must track upstream layout changes. Rare, and the
  build fails loudly when it happens.

## Pros and Cons of the Options

### Pinned submodules

* Good, because the pin is a first-class git object - unambiguous and diffable.
* Good, because it keeps upstream history reachable for bisecting.
* Bad, because of the clone ergonomics.

### Vendoring full copies

* Good, because the clone is one step and always complete.
* Bad, because upstream and local changes become indistinguishable, which is precisely the state
  the predecessor is in.
* Bad, because the repository carries tens of thousands of lines it does not own - Borealis alone
  is around 40,000 - polluting diffs, search results and language statistics.

### FetchContent / ExternalProject

* Good, because it is pure CMake with no extra tooling and pins by tag or SHA.
* Bad, because it downloads at configure time, so an offline or firewalled build fails.
* Bad, because the dependency source is not in the tree, making it harder to read while debugging
  a cross-compiled crash - which, on this platform, is a routine activity.

### A package manager

* Good, because it is the industry norm with real version resolution.
* Bad, because neither Conan nor vcpkg has usable devkitA64 support; every dependency would need
  a custom recipe.
* Bad, because it adds a toolchain contributors must install before they can build at all.

## More Information

* [`third_party/PINS.md`](../../third_party/PINS.md)
* [`tools/deps/verify_pins.sh`](../../tools/deps/verify_pins.sh)
* [ADR-0010](0010-use-the-borealis-fork-as-the-ui-framework.md)

Revisit if devkitPro ships a package manager with real Switch support, or if Borealis publishes
a CMake-native release we could consume directly.
