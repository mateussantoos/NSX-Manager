---
status: "accepted"
date: 2026-09-23
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["build"]
---

# 0002. Build with CMake and the devkitPro Switch toolchain file

## Context and Problem Statement

Switch homebrew conventionally uses the devkitPro Makefile (`switch_rules`), which the
predecessor used. That Makefile builds its source list with

```make
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))   # Makefile:84
```

and sets `VPATH` to the source directories (`Makefile:78`). Two consequences block the layered
tree this project needs ([ADR-0003](0003-use-a-layered-source-tree-with-co-located-headers.md)):

1. **Objects are flat.** `core/update/manifest.cpp` and `ui/tabs/update/manifest.cpp` both become
   `build/manifest.o`. One silently overwrites the other and the wrong object links. There is no
   diagnostic.
2. **`$(wildcard)` is not recursive.** Every directory must be listed in `SOURCES` by hand -
   about 30 entries, maintained forever, where a forgotten line means a silently missing module.

Separately, we want `core/` to compile a second time with the host compiler for unit tests
([ADR-0014](0014-split-pure-logic-into-a-host-testable-core-library.md)), and we want
`compile_commands.json` for clang-tidy.

## Decision Drivers

* The layered tree must work without a workaround guarding it.
* One source list, compiled twice (device and host) - never two hand-maintained lists.
* `compile_commands.json` is a hard requirement for `.clang-tidy` in CI.
* The developer's real environment is Windows/MSYS2, where tooling choices matter.
* Greenfield: there is no application code yet, so migration cost is near zero *today*.

## Considered Options

* CMake with `$DEVKITPRO/cmake/Switch.cmake`
* The devkitPro Makefile, plus a recursive-wildcard helper and a duplicate-basename lint
* The devkitPro Makefile now, migrating to CMake after 1.0.0

## Decision Outcome

Chosen option: **CMake >= 3.24 with the official devkitPro `Switch.cmake` toolchain file**, Ninja
generator, because it is the only option where the layered tree, the host test build and
`compile_commands.json` all fall out naturally instead of being worked around.

devkitPro ships `Switch.cmake` together with `nx_generate_nacp()` and `nx_create_nro()`, so
nothing is lost relative to `switch_rules`.

`apps/rcm-payload/` keeps its devkitARM Makefile **verbatim**. hekate's build is genuinely exotic
(custom `IPL_LOAD_ADDR`, custom linker script, `VPATH` across `bdk/*/*/`), and it must stay a
separate program for licensing reasons anyway
([ADR-0011](0011-license-gplv3-and-isolate-the-gplv2-only-rcm-payload.md)). CMake invokes it
opaquely via `add_custom_command` and copies the resulting blob into romfs.

### Consequences

* Good, because per-target object directories are path-qualified by construction -
  `CMakeFiles/nsx_core.dir/src/nsx/core/update/manifest.cpp.o` cannot collide.
* Good, because `nsx_core` is one `add_library` compiled by two presets. With Make it would be a
  second Makefile duplicating the file list, which would drift.
* Good, because `CMakePresets.json` reduces onboarding to `cmake --preset switch-release`.
* Good, because `CMAKE_EXPORT_COMPILE_COMMANDS` gives clang-tidy what it needs; `bear` does not
  work reliably for a cross-compile under MSYS2.
* **Bad, because upstream Borealis ships only `library/borealis.mk`.** We must write and maintain
  `third_party/CMakeLists.txt` - roughly 25 lines reproducing that mk file. This is the single
  real cost of this decision and it is not hypothetical.
* Bad, because most Switch homebrew uses Makefiles, so a contributor arriving from that world
  meets something unfamiliar.
* Neutral, because CMake and Ninja are extra tools to install. Both ship with devkitPro's
  environment and the CI Docker images.

## Pros and Cons of the Options

### CMake with Switch.cmake

* Good, because it solves the object-collision and recursion problems natively rather than by
  policing them.
* Good, because the host test build is the same tree with a different preset.
* Good, because the Borealis wrapper is written once and then frozen - it is not ongoing work.
* Bad, because that wrapper has to be written at all, and must be revisited if Borealis
  restructures its sources.

### devkitPro Makefile plus a lint

* Good, because it is the proven path - the predecessor builds with it today.
* Good, because no Borealis wrapper is needed.
* Bad, because the duplicate-basename problem becomes a **rule contributors must obey** rather
  than an impossibility. `core/update/manifest.cpp` and `ui/tabs/update/manifest.cpp` are both
  natural names; forbidding that is a tax on every future file.
* Bad, because the host test build needs a second Makefile duplicating the source list.
* Bad, because there is no `compile_commands.json`, so clang-tidy in CI becomes impractical.

### Makefile now, CMake after 1.0.0

* Good, because the first build works immediately with zero toolchain risk.
* Bad, because the migration cost only grows - the whole argument for choosing CMake now is that
  there is no application code to migrate. Deferring converts a one-file change into a
  hundred-file one.
* Bad, because the host test suite, which is the main defence against repeating the predecessor's
  version-comparison bug, would be blocked until after 1.0.0.

## More Information

* [devkitPro/docker](https://github.com/devkitPro/docker) - `devkitpro/devkita64`, `devkitpro/devkitarm`
* Build details: [`docs/architecture/build-system.md`](../architecture/build-system.md)
* Version single-sourcing: [`cmake/NsxVersion.cmake`](../../cmake/NsxVersion.cmake)

Revisit if devkitPro deprecates `Switch.cmake`, or if the Borealis wrapper becomes a recurring
maintenance burden rather than a one-time cost.
