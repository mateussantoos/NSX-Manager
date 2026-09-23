<div align="center">

# NSX Manager

**An on-console manager for Nintendo Switch custom firmware, homebrew and content.**

[![CI](https://github.com/mateussantoos/nsx-manager/actions/workflows/ci.yml/badge.svg)](https://github.com/mateussantoos/nsx-manager/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/mateussantoos/nsx-manager?include_prereleases&sort=semver)](https://github.com/mateussantoos/nsx-manager/releases)
[![License](https://img.shields.io/badge/license-GPL--3.0--only-blue)](LICENSE)
[![Docs](https://img.shields.io/badge/docs-github--pages-informational)](https://mateussantoos.github.io/nsx-manager/)

</div>

---

> **Status: pre-release (0.x).** NSX Manager is being written from scratch. It is not yet
> feature-complete and no stable release has been cut. See [`docs/ROADMAP.md`](docs/ROADMAP.md)
> for the path to 1.0.0.

## What it is

NSX Manager is a homebrew application that runs **on the console** and manages a Switch custom
firmware installation: the CFW pack itself, official firmware, save data, cheats, ports,
translations and the maintenance tools around them - and it keeps **itself** up to date from
GitHub Releases.

It is the successor to NSX Updater 4.2.1, rebuilt on a layered architecture with a verified
update pipeline. See [`docs/PROJECT_SCOPE.md`](docs/PROJECT_SCOPE.md) for what is in scope and,
just as importantly, what is deliberately **not**.

## What it is not

It is not a piracy tool. NSX Manager does not download, install or distribute commercial game
content, and it never will. See [Scope - Non-goals](docs/PROJECT_SCOPE.md#4-non-goals).

## Install

1. Download `nsx-manager-<version>.zip` from the
   [latest release](https://github.com/mateussantoos/nsx-manager/releases/latest).
2. Extract it to the **root** of your SD card. The archive contains exactly one path:
   `switch/nsx-manager/nsx-manager.nro`.
3. Launch it from the homebrew menu.

Verify your download against `SHA256SUMS` from the same release. Full instructions, including
how updates work and how to recover a failed one, are in
[`docs/user/installation.md`](docs/user/installation.md) and
[`docs/user/updating.md`](docs/user/updating.md).

## Build

Requires [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the `switch-dev` and
`devkitARM` groups, plus CMake 3.24 or newer and Ninja.

```sh
git clone --recurse-submodules https://github.com/mateussantoos/nsx-manager.git
cd nsx-manager
cmake --preset switch-release
cmake --build --preset switch-release
```

Host-side unit tests need no Switch toolchain and no hardware:

```sh
cmake --preset host-debug && ctest --preset host-debug --output-on-failure
```

Full setup for Windows/MSYS2 and Docker: [`docs/contributing/local-setup.md`](docs/contributing/local-setup.md).

## Documentation

| | |
|---|---|
| [Project scope](docs/PROJECT_SCOPE.md) | What this project is, its standards, and the 1.0.0 parity checklist |
| [Architecture](docs/architecture/overview.md) | Layering, build system, update pipeline, threat model |
| [Architecture decisions](docs/adr/README.md) | 14 ADRs recording every foundational choice and its rejected alternatives |
| [Contributing](CONTRIBUTING.md) | Setup, coding style, commit convention, branching, releasing |
| [User guide](docs/user/installation.md) | Install, update, troubleshoot |
| [API reference](https://mateussantoos.github.io/nsx-manager/) | Doxygen, published from `main` |

## Licence

GPL-3.0-only - see [`LICENSE`](LICENSE).

The RCM payload under [`apps/rcm-payload/`](apps/rcm-payload/) is derived from hekate/BDK and is
**GPL-2.0-only**. It is a separate program, built by a separate toolchain, and is never linked
into the application. The reasoning is documented in
[`docs/architecture/licensing.md`](docs/architecture/licensing.md).

Third-party attributions: [`NOTICE`](NOTICE).

## Credits

NSX Manager stands on the work of [HamletDuFromage's aio-switch-updater](https://github.com/HamletDuFromage/aio-switch-updater),
the [Borealis](https://github.com/natinusala/borealis) UI framework, and
[CTCaer's hekate/BDK](https://github.com/CTCaer/hekate). The CNX Pack, by Costela, is the
project this lineage began with.
