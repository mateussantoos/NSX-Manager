# Build system

CMake with devkitPro's Switch toolchain file. Decision and alternatives:
[ADR-0002](../adr/0002-build-with-cmake-and-the-devkitpro-switch-toolchain.md).

## Quick start

```sh
docker compose run --rm nsx switch     # the app, forwarder and RCM payload
docker compose run --rm nsx test       # host tests, no hardware
```

Those wrap the presets below. The container carries devkitA64, devkitARM and the Switch portlibs
so none of it has to be installed natively - see
[ADR-0015](../adr/0015-build-and-verify-inside-a-container.md). Running the presets directly
works too, if you have the toolchain.

## Presets

| Preset | Toolchain | Builds | Used for |
|---|---|---|---|
| `switch-debug` | devkitA64 | app, forwarder, RCM | On-device debugging |
| `switch-release` | devkitA64 | app, forwarder, RCM | **What releases are built from** |
| `host-debug` | host compiler | `nsx_core` + tests | CI unit tests; the everyday loop |
| `host-asan` | host compiler | same, with ASan/UBSan | Catching memory errors that a console cannot report |

`host-*` presets force `NSX_BUILD_APP=OFF` and `NSX_BUILD_RCM=OFF` - only `core` is portable, so
the restriction is structural rather than a flag anyone needs to remember.

## Targets

| Target | Type | Links |
|---|---|---|
| `nsx_core` | static | nothing but the standard library |
| `nsx_platform` | static | `nsx_core` |
| `nsx_infra` | static | `nsx_core`, `nsx_platform`, curl, mbedtls, z, `nsx::zipper` |
| `nsx_domain` | static | `nsx_core`, `nsx_infra` |
| `nsx_ui` | static | `nsx_core`, `nsx_domain`, `nsx::borealis` |
| `nsx-manager` | executable -> `.nro` | every layer |
| `nsx-forwarder` | executable -> `.nro` | `nsx_core` **only** |
| `nsx_rcm` | custom command | nothing - built by its own Makefile |
| `nsx_romfs` | custom target | Stages everything the app reads from `romfs:/` |

The link lines *are* the layering rule, restated by the build.

## Source lists are explicit

Each layer's `CMakeLists.txt` lists its files by name:

```cmake
set(NSX_CORE_SOURCES
    version/semver.cpp
    update/manifest.cpp
    ...
    PARENT_SCOPE)
```

**Never globbed.** A glob does not re-run when a file is added, so an unchanged build directory
silently omits new code. Explicit lists also make a pull request's scope visible in the diff.

`nsx_add_layer()` in [`cmake/NsxLayer.cmake`](../../cmake/NsxLayer.cmake) skips a target whose
source list is empty, which is what lets this scaffold configure cleanly before the first module
lands.

## Version single-sourcing

```
/VERSION  (0.1.0)
    |
cmake/NsxVersion.cmake   validates SemVer, reads git SHA and build date
    |
    +-> configure_file -> build/generated/nsx/core/version/version.hpp
    |                       nsx::core::version::kString, kUserAgent, kGitSha, ...
    +-> nx_generate_nacp(VERSION ...)   -> what hbmenu displays
```

Verify it standalone, without configuring the tree:

```sh
cmake -P cmake/NsxVersion.cmake
```

It **fails loudly** on a malformed version - a leading `v`, a missing component, or a leading
zero. Two lints back it up: `forbid_hardcoded_version.sh` rejects any version literal under
`src/`, and `check_version.sh` rejects a release whose tag disagrees with `/VERSION`.

## romfs

[`cmake/NsxRomfs.cmake`](../../cmake/NsxRomfs.cmake) stages an **explicit** list into
`build/<preset>/romfs/`: `assets/images`, `assets/sounds`, `assets/data`, `assets/i18n`, the
built forwarder, the RCM payload, and `CHANGELOG.md` rendered to `data/changelog.md` so the
in-app changelog is generated rather than hand-maintained. The predecessor's changelog was 47
versions of hardcoded `std::vector<std::string>` literals across 176 lines.

## The RCM payload

Built by `make -C apps/rcm-payload` via `add_custom_command`, with devkitARM, from its own
Makefile. CMake never compiles a line of it, and the resulting `.bin` is copied into romfs as
opaque data. This is a licensing boundary
([ADR-0011](../adr/0011-license-gplv3-and-isolate-the-gplv2-only-rcm-payload.md)) that happens
to also be good architecture: it is a freestanding ARM7 program for a different processor.

Disable it with `-DNSX_BUILD_RCM=OFF` if `DEVKITARM` is not installed.

## Switch portlibs

devkitPro's `Switch.cmake` sets up the compiler and the NRO helpers, but it does **not** put
`$DEVKITPRO/portlibs/switch/include` on the search path. Linking a bare `z` therefore compiles
until the first `#include <zlib.h>` and then fails pointing at the wrong problem.

[`cmake/NsxPortlibs.cmake`](../../cmake/NsxPortlibs.cmake) resolves each library explicitly and
fails at **configure** time naming the exact `dkp-pacman` package to install. Required:
`switch-zlib`, `switch-curl`, `switch-mbedtls`, `switch-glfw`, `switch-glad`, `switch-mesa`,
`switch-libdrm_nouveau`.

## Vendored C and GCC 15

devkitPro now ships GCC 15, which promoted `-Wincompatible-pointer-types` from a warning to an
error. minizip's `ioapi` predates that. `-w` does not help - it silences warnings, and this is an
error - so `third_party/CMakeLists.txt` scopes
`-Wno-error=incompatible-pointer-types` to the vendored C targets only. First-party code still
gets the diagnostic at full strength. The predecessor carried the same workaround globally, at
`Makefile:44`.

## third_party

Borealis ships only `library/borealis.mk`, so
[`third_party/CMakeLists.txt`](../../third_party/CMakeLists.txt) reproduces it as a CMake target.
This is the one real cost of choosing CMake, and it is written once. It lives **outside** the
submodule so `git submodule update` never conflicts with it.

Vendored code is exempt from our warning policy (`-w`) - we do not own its style.

## Options

| Option | Default | Effect |
|---|---|---|
| `NSX_BUILD_APP` | `ON` | Build the app and forwarder (forced `OFF` on host) |
| `NSX_BUILD_RCM` | `ON` | Build the RCM payload |
| `NSX_BUILD_TESTS` | `OFF` | Build host unit tests |
| `NSX_WERROR` | `ON` | Warnings are errors in first-party code only |

## Troubleshooting

| Symptom | Cause |
|---|---|
| `DEVKITPRO is not set` | devkitPro environment not sourced |
| `third_party/borealis is empty` | `git submodule update --init --recursive` |
| `VERSION must be SemVer 2.0.0` | `/VERSION` is malformed - the guard working as intended |
| `no sources yet - target skipped` | Expected in the scaffold; the module has not landed |
| `DEVKITARM is not set` | Install devkitARM, or configure with `-DNSX_BUILD_RCM=OFF` |
