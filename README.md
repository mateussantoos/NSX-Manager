<p align="center">
  <img src="assets/banner.png" alt="NSX Manager Banner" width="100%">
</p>

<div align="center">

# NSX Manager

**A modern, safe, independent Atmosphere (AMS) and system maintenance utility for Nintendo Switch.**

[![CI](https://github.com/mateussantoos/nsx-manager/actions/workflows/ci.yml/badge.svg)](https://github.com/mateussantoos/nsx-manager/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/mateussantoos/nsx-manager?include_prereleases&sort=semver)](https://github.com/mateussantoos/nsx-manager/releases)
[![License](https://img.shields.io/badge/license-GPL--3.0--only-blue)](LICENSE)
[![Docs](https://img.shields.io/badge/docs-github--pages-informational)](https://mateussantoos.github.io/nsx-manager/)

</div>

---

## Overview

**NSX Manager** is an on-console custom firmware and maintenance management application for Nintendo Switch, written from scratch in strictly layered modern **C++20**. Designed as the reliable, safety-first successor to legacy updaters, NSX Manager provides an all-in-one on-device solution to maintain Atmosphere (AMS), manage system firmware, inspect hardware diagnostics, toggle sysmodules, and keep the application itself seamlessly updated.

All core domain logic—version negotiation, download staging, verification, and filesystem operations—is isolated into a pure, host-testable core library with 100% automated test coverage.

---

## Key Features

### 📊 Modular Dashboard Grid
- **Hardware Model Detection:** Identifies console hardware revisions at runtime (Switch V1 / Erista, V2 / Mariko, OLED, or Lite).
- **Real-Time Environment Badges:** Instant visual status of installed Horizon OS firmware version, Atmosphere (AMS) build, and active NAND mode (SysNAND vs. EmuNAND).
- **SD Card Storage Gauge:** Real-time capacity bar gauge displaying free vs. total storage alongside filesystem format detection (FAT32 vs. exFAT corruption warning).
- **Security & Telemetry Badge:** Automated connectivity probing against telemetry endpoints (`conntest.nintendowifi.net` and `ctest.cdn.nintendo.net`) confirming whether telemetry blocking / 90DNS shield protection is active.

### 🎨 Clean Lucide Aesthetics & Dark Theme
- **True Black OLED Theme:** Deep black background (`#000000`) paired with distinctive Nintendo crimson red accents (`#E60012`).
- **Minimalist Sidebar:** Compact 280px navigation rail offering expanded canvas width for dashboard cards and lists.
- **Modern Vector Line Icons:** Bespoke geometric line-art glyphs inspired by the Lucide icon system (Home, Update, Atmosphere, Firmware, Tools wrench, Settings).
- **Single-Stroke Focus Highlights:** Refined gamepad navigation with crisp single-border outlines avoiding nested focus artifacts.

### 🛠️ Maintenance & System Utilities
- **Archive-Bit Recursive Repair:** Scans and clears FAT32 archive-bit directory attributes across the SD card to resolve boot crashes, corrupted homebrew titles, and theme errors.
- **Reboot to Payload (RCM):** Reboots directly into Atmosphere or custom payloads (`/bootloader/update.bin`, `payload.bin`) via hardware `splSetConfig` and `bpcDoReboot` without powering off the console.
- **Sysmodules Toggle Manager:** Enumerates installed Atmosphere background modules under `/atmosphere/contents/` and toggles their startup presence via `flags/boot2.flag`.
- **Temporary Cache Cleanup:** Safely purges leftover staging directories, temporary download archives, and update swap residue.

### 🔄 Verified Remote Manifest Updates
- **Self-Update Architecture:** Two-stage transactional update process backed by an independent repair forwarder (`nsx-forwarder.nro`), rollback safeguards, and SHA-256 integrity verification.
- **Conditional HTTP (ETag) Caching:** Manifest requests query release CDNs using `If-None-Match` HTTP headers, preserving network bandwidth and avoiding unauthenticated GitHub API rate limits.
- **Atmosphere & Firmware Management:** Streamlined downloading and staging of AMS packages and official firmware update sets for Daybreak installation.

### ⚡ Startup Preloader & Update Alerts
- **Animated Splash Screen:** High-performance startup preloader rendering typography logo entrance animations and smooth diagonal NanoVG gradient shimmer sweeps.
- **Subsystem Preloading:** Probes system hardware, storage capacity, and network shields asynchronously in the background.
- **Automated Update Modal:** Non-intrusive startup dialog notifying the user immediately when a new NSX Manager version is available, with one-touch direct navigation to the Update tab.
- **Full Bilingual Localization:** Complete interface translations for **en-US** and **pt-BR**, enforced by automated key-parity linters.

---

## What It Is Not

NSX Manager is strictly an Atmosphere environment and homebrew utility. **It is not a piracy tool.** NSX Manager does not host, download, install, or distribute commercial game software, NSP/XCI packages, or tickets, and it never will. Please review [Scope - Non-goals](docs/PROJECT_SCOPE.md#4-non-goals).

---

## Installation

1. Download `nsx-manager-<version>.zip` from the [Latest Release](https://github.com/mateussantoos/nsx-manager/releases/latest).
2. Extract the archive to the **root** of your SD card. The archive contains:
   - `switch/nsx-manager/nsx-manager.nro`
3. Launch **NSX Manager** through the Homebrew Menu (hbmenu).

> **Tip:** You can verify downloaded releases against the published `SHA256SUMS`. Detailed user guides and recovery instructions are available in [`docs/user/installation.md`](docs/user/installation.md) and [`docs/user/updating.md`](docs/user/updating.md).

---

## Building from Source

All external dependencies (`third_party/borealis`, `third_party/zipper`, `nlohmann/json`, `doctest`) are **fully vendored directly** inside the repository. No submodule initialization (`--recurse-submodules`) is required.

The build environment is fully containerized with pinned devkitA64, devkitARM, Switch portlibs, and clang. You only need **Docker and Git**.

```sh
# Clone the repository (clean, single repository clone)
git clone https://github.com/mateussantoos/nsx-manager.git
cd nsx-manager

# Build the container image
docker compose build

# Compile Switch release binaries (app + forwarder + RCM payload)
docker compose run --rm nsx export
```

### Development & Quality Gate Commands

```sh
docker compose run --rm nsx test       # Run fast host unit tests (doctest, ~0.3s)
docker compose run --rm nsx lint       # Run full formatting, layering, encoding & i18n checks
docker compose run --rm nsx verify     # Complete release verification pipeline
docker compose run --rm nsx doctor     # Inspect container toolchains and portlib versions
```

For native local building without Docker, consult [`docs/contributing/local-setup.md`](docs/contributing/local-setup.md).

---

## Documentation

| Document | Description |
|---|---|
| [Project Scope](docs/PROJECT_SCOPE.md) | Standards, operational non-goals, and 1.0.0 feature parity roadmap |
| [Architecture Overview](docs/architecture/overview.md) | Six-tier layered model, transaction pipeline, and threat model |
| [Architecture Decisions (ADRs)](docs/adr/README.md) | Architectural decision records documenting engineering choices |
| [Contributing Guide](CONTRIBUTING.md) | Coding style, Conventional Commits format, and pull request workflow |
| [Troubleshooting Guide](docs/user/troubleshooting.md) | Diagnostic checklists, archive-bit fixes, and common error resolutions |
| [API Reference](https://mateussantoos.github.io/nsx-manager/) | Generated Doxygen technical reference |

---

## Legal & Licensing

- **License:** Released under the [GNU General Public License v3.0](LICENSE) (`GPL-3.0-only`).
- **RCM Payload Isolation:** The recovery payload in [`apps/rcm-payload/`](apps/rcm-payload/) is derived from CTCaer's hekate/BDK and is licensed **GPL-2.0-only**. As documented in [ADR-0011](docs/adr/0011-license-gplv3-and-isolate-the-gplv2-only-rcm-payload.md), it is maintained as an isolated sub-project compiled with a separate devkitARM toolchain and embedded as opaque data, never linked with GPL-3.0 application code.
- **Third-Party Notices:** Third-party libraries, fonts (Inter, Material Icons), and licenses are cataloged in [`NOTICE`](NOTICE).
- **Disclaimer:** NSX Manager is an independent open-source project and is not affiliated, associated, authorized, endorsed by, or in any way officially connected with Nintendo Co., Ltd. or Nintendo of America Inc. "Nintendo Switch" is a registered trademark of Nintendo Co., Ltd.

---

## Credits

NSX Manager builds upon the foundational homebrew ecosystem created by:
- [HamletDuFromage's aio-switch-updater](https://github.com/HamletDuFromage/aio-switch-updater)
- [Borealis UI Framework](https://github.com/natinusala/borealis)
- [CTCaer's hekate / BDK](https://github.com/CTCaer/hekate)
- The **CNX Pack** by Costela, which originated this lineage.
