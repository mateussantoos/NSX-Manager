---
status: "accepted"
date: 2026-09-23
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["build", "ci", "process"]
---

# 0015. Build and verify inside a container

## Context and Problem Statement

Building this project natively needs devkitA64, devkitARM, seven Switch portlibs, CMake, Ninja,
a specific clang, Doxygen and a Python package. Installing that on a developer machine is a
multi-gigabyte, elevation-requiring change that also leaves permanent state behind.

Two concrete failures made the cost of *not* containerising visible:

1. **The maintainer's machine could not install devkitPro at all** - it is not in winget or
   choco, and its installer requires administrator rights the account does not have. The Switch
   build was simply unrunnable locally.
2. **Three different clang versions were in play at once** - 23 on the maintainer's Windows
   machine, 14 in the first container image, 18 pinned in CI. `clang-format` output differs
   between major versions, so `check_format.sh` would have given three different answers. A lint
   that disagrees with itself is one people learn to ignore.

A third problem was already documented: clang's AddressSanitizer is incompatible with the MSVC
debug CRT, so the `host-asan` preset aborts during CRT startup on Windows. Sanitizer coverage was
effectively unavailable to the only person working on the project.

## Decision Drivers

* A contributor should need Docker and git, and nothing else.
* "Passes locally" and "passes CI" must be the same statement, not two approximations.
* The toolchain must be pinned and reproducible, not "whatever the machine has".
* Sanitizers must be runnable by whoever is actually writing the code.
* Building the GPL-2.0 payload and the GPL-3.0 application must stay possible in one workflow
  without weakening their isolation.

## Considered Options

* One image, built on `devkitpro/devkita64`, driven by a task entrypoint
* Two images - `devkitpro/devkita64` for the app, `devkitpro/devkitarm` for the payload
* Devcontainer (`.devcontainer/`) only
* Keep native builds and only pin versions harder

## Decision Outcome

Chosen option: **a single image built on `devkitpro/devkita64`**, with devkitARM and the Switch
portlibs added through devkitPro's own package manager, plus a task entrypoint
(`docker/entrypoint.sh`) that both developers and CI invoke identically.

```sh
docker compose run --rm nsx switch     # app + forwarder + RCM payload
docker compose run --rm nsx verify     # lint + test + manifest + docs
docker compose run --rm nsx doctor     # what does this image actually provide?
```

Specifics that matter:

* **clang is pinned to 18** from LLVM's apt repository, not the base distribution's. This is the
  direct fix for the three-version problem.
* **Build trees live in named volumes**, not the bind mount. On Docker Desktop a bind mount
  crosses a filesystem boundary and compiling across it is several times slower.
* **CI runs the same tasks in the same image.** `ci.yml` is a matrix over entrypoint task names.
  There is no separate CI recipe that can drift.
* **`release.yml` also builds through the image**, so a published binary comes from a toolchain
  that has already been exercised rather than one assembled at release time.
* **A `doctor` task** reports the toolchain, because "is the environment what I think it is?"
  turned out to be the first question in every failure.

### Consequences

* Good, because the Switch build became runnable at all for the maintainer - it previously was
  not.
* Good, because ASan and UBSan now run, which on Windows they could not.
* Good, because a CI failure reproduces exactly: same image, same command.
* Good, because the payload's devkitARM toolchain lives alongside the application's devkitA64 one
  without weakening the licence isolation - that isolation is enforced by the build files and
  `check_license_isolation.sh`, not by keeping compilers in separate images.
* Good, because the whole toolchain is one reviewable file.
* Bad, because contributors need Docker, and a first `docker compose build` takes several minutes.
* Bad, because the build tree is not directly visible on the host, so getting an artefact out is
  an explicit copy. Documented in `local-setup.md`.
* Bad, because the image must be rebuilt when the Dockerfile changes, and forgetting that
  produces confusing staleness. `doctor` is the antidote.
* Neutral, because native builds still work and remain documented - this makes the container the
  supported path, not the only one.

## Pros and Cons of the Options

### One image, task entrypoint

* Good, because one `docker compose run` can build all three binaries.
* Good, because a task is defined once and used by both humans and CI, so they cannot diverge.
* Bad, because the image carries two cross-toolchains and is therefore large.

### Two images

* Good, because each image is smaller, and the toolchain split visibly mirrors the licence
  boundary.
* Bad, because building the full project needs two containers and something to orchestrate them,
  and the romfs staging step needs the payload output from the other image.
* Bad, because it implies the licence isolation depends on image separation. It does not - it
  depends on no shared source and no shared link step, which is enforced by a lint. Encoding it
  in infrastructure would make the guarantee look weaker than it is.

### Devcontainer only

* Good, because an editor-integrated environment is pleasant for day-to-day work.
* Bad, because it is VS Code-centric and does not give CI anything to run.
* Not mutually exclusive: a `.devcontainer` can be added later pointing at this same image.

### Keep native builds, pin harder

* Good, because no new tooling.
* Bad, because it does not solve the actual blocker - devkitPro could not be installed on the
  maintainer's machine at all.
* Bad, because "pin harder" on a native toolchain means documentation, and documentation is not
  enforcement. The three-clang-version problem arose *despite* the version being documented.

## More Information

* [`Dockerfile`](../../Dockerfile), [`docker-compose.yml`](../../docker-compose.yml),
  [`docker/entrypoint.sh`](../../docker/entrypoint.sh)
* [`docs/contributing/local-setup.md`](../contributing/local-setup.md)
* [`docs/contributing/ci.md`](../contributing/ci.md)
* [ADR-0002](0002-build-with-cmake-and-the-devkitpro-switch-toolchain.md) - the build system this
  containerises
* [ADR-0011](0011-license-gplv3-and-isolate-the-gplv2-only-rcm-payload.md) - why both toolchains
  coexisting is safe

Revisit if devkitPro publishes an image that already carries the portlibs this project needs, or
if image size becomes a real obstacle rather than an inconvenience.
