# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Released sections are generated from [Conventional Commits](docs/contributing/commit-convention.md)
by `git-cliff` during the release workflow - do not hand-edit them. Add entries under
`[Unreleased]` only when a change needs prose that a commit subject cannot carry.

<!-- next-header -->

## [Unreleased]

## [0.3.2] - 2026-09-26

### Added

- **Dedicated Support & Donation Tab (`DonateTab`)**:
  - High-resolution QR code view rendered against an OLED true black (#000000) background.
  - Integrated quiet-zone contrast plate ensuring instant optical scanning on portable and docked displays.
  - Bilingual typography support (en-US and pt-BR) with project support descriptions and contribution badge.
  - Added custom vector Lucide Heart icon to Borealis sidebar navigation.
- **RomFS Asset Packaging & Documentation**:
  - Embedded `assets/qr.png` into RomFS (`romfs:/images/qr.png` and `romfs:/qr.png`) via `cmake/NsxRomfs.cmake`.
  - Added dedicated Donations section to `README.md`.

### Fixed

- **Release Workflow Permissions & Asset Staging**:
  - Explicitly configured `permissions: contents: write` on GitHub Actions release jobs.
  - Automated staging of `nsx_rcm.bin` payload into release distributions and SHA-256 checksums.

## [0.3.1] - 2026-09-26

### Fixed

- **Static Analysis & Clang-Tidy (`cert-err33-c`)**:
  - Explicitly handled or discarded `std::snprintf` return values in `FileLogger` and `StringUtils` byte formatting.
  - Streamlined `.clang-tidy` rules by disabling cosmetic stylistic checks (`readability-uppercase-literal-suffix`, `readability-named-parameter`, `modernize-avoid-c-arrays`, `readability-avoid-nested-conditional-operator`).
  - Restricted Clang-Tidy translation unit processing strictly to first-party production code (`src/nsx/`), isolating test harness noise.

### Changed

- **C++20 Idiomatic Code Cleanups**:
  - Enforced `const` correctness on range-based loop variables across `path_utils`, `string_utils`, and `sysmodule_service`.
  - Replaced `count(...) != 0` container lookups with `contains(...)` in CFW install service.
  - Flattened nested ternary conditional operators into explicit branches in URL parsing and `std::clamp` in backoff jitter calculations.
  - Refactored validation loops in manifest parsing to `std::ranges::all_of`.

## [0.3.0] - 2026-09-26

### Added

- **Structured Disk and Console Logging (`core/log`)**:
  - Implemented thread-safe, non-blocking `FileLogger` with automatic size capping (1 MB) and log rotation.
  - Dual output: simultaneous console stdout and persistent append-only storage in `sdmc:/switch/nsx-manager/nsx.log`.
  - Comprehensive unit test coverage verifying formatting, buffer flush, and rotation boundaries.
- **Native Horizon Network Telemetry (`platform/network`)**:
  - Implemented `NetworkInterfaceService` wrapping Horizon OS `nifm` services.
  - Real-time queries for connection medium (Wi-Fi, Ethernet, Offline), assigned local IPv4 address, subnet mask, gateway, SSID, and RSSI signal level.
  - Mock implementation and full host test coverage.
- **Dynamic Community Bulletins and MOTD Protocol (`domain/motd`)**:
  - Implemented `MotdService` parsing optional community bulletin payloads from remote manifests (`update.json`).
  - Gated by application version (`min_app_version`) with persistent dismissal tracking and local caching.
  - Unit test suite verifying severity parsing, dismissal state, and version boundaries.
- **Payload Scanner and RCM Launcher (`platform/launch`)**:
  - Implemented `PayloadLauncher` scanning `.bin` payloads across `/bootloader/payloads/` and `/switch/nsx-manager/payloads/`.
  - Added safe reboot to payload execution and target chainloading via `envSetNextLoad`.
- **Modernized Dashboard Grid and Live Telemetry UI**:
  - Hardware card with runtime SoC stepping detection (`T210B01 Mariko` vs. `T210 Erista`).
  - System version card displaying real-time Horizon OS, Atmosphere (AMS) versions, and NAND mode badge (`SysNAND` / `EmuNAND`).
  - Live network card with connection status, IP address, SSID, multi-bar Wi-Fi signal gauge, and 90DNS telemetry protection shield indicator.
  - Storage gauge card displaying free vs. total storage and FAT32/exFAT cluster health indicator.
  - Prominent MOTD alert card rendered dynamically at the top of the dashboard when active bulletins are present.
- **Architecture Decision Records (ADRs)**:
  - Documented ADR-0019 (vendored dependency consolidation), ADR-0020 (file logging), ADR-0021 (network telemetry), ADR-0022 (MOTD protocol), and ADR-0023 (Tegra RCM LVGL optimization).

### Changed

- **Vendor Dependency Consolidation**:
  - Permanently consolidated all third-party dependencies (`borealis`, `zipper`, `nlohmann/json`, `doctest`) into the primary repository index.
  - Purged submodule metadata (`.gitmodules`) and nested `.git` references for offline, atomic repository clones.
  - Deduplicated `json.hpp` across Borealis and NSX Manager onto `third_party/nlohmann/json.hpp`.
  - Purged legacy desktop tests, documentation, and external CI files from vendored libraries.
- **Tegra RCM Payload Optimization**:
  - Disabled and excluded non-essential LVGL widgets (`lv_calendar`, `lv_chart`, `lv_gauge`, `lv_spinbox`, `lv_table`, `lv_tileview`, `lv_win`, `lv_roller`, `lv_preload`).
  - Reduced payload footprint to 75,809 bytes (over 50 KB safety margin below the 126,296-byte Tegra IRAM hardware limit).

## [0.2.5] - 2026-09-25

### Fixed

- Automated `dist/RELEASE_NOTES.md` extraction from `CHANGELOG.md` in release packaging to prevent missing artifact errors in GitHub Releases publication.
- Added resilience handling to GitHub Pages deployment pipeline (`enablement: true` and `continue-on-error: true`), guarding against unprovisioned Pages environments.

## [0.2.4] - 2026-09-25

### Fixed

- Synchronized upstream Mozilla CA certificate bundle hash in `third_party/cacert/cacert.pem.sha256`.
- Resolved Doxygen build directory creation failure in documentation pipeline (`mkdir -p build/docs`).
- Isolated `clang-tidy` checks strictly to first-party sources (`src/nsx/`, `apps/`, `tests/`), completely excluding `third_party/`.

### Changed

- Purged non-essential vendor test and doc directories and legacy CI configurations from `third_party/borealis/library/lib/extern/fmt/`.
- Deduplicated JSON library headers to use `third_party/nlohmann/json.hpp` exclusively, removing redundant copy from Borealis.
- Standardized RomFS branding assets on `assets/splash.png` and pruned duplicate `assets/images/splash.png`.
- Pruned obsolete infrastructure stubs (`mega`, `github`).
- Implemented minimal thread-safe file logger in `src/nsx/core/log/` writing to `sdmc:/switch/nsx-manager/nsx.log`.
- Retained clean placeholder exports in `src/nsx/platform/network/` and `src/nsx/domain/motd/`.
- Optimized RCM payload LVGL configuration by excluding heavy unreferenced widgets (`lv_calendar`, `lv_chart`, `lv_gauge`, `lv_table`, `lv_tileview`).

## [0.2.3] - 2026-09-25

### Fixed

- Restored POSIX executable bit (`100755`) on all shell scripts and hooks in Git tree:
  * Resolved `Permission denied` (exit code 126) on CI runners (`check_version.sh`, `verify_pins.sh`, `fetch.sh`, and `run_all.sh`).
- Aligned CI dependency workflows with vendored repository structure:
  * Handled absence of `.gitmodules` cleanly in `deps.yml` and `verify_pins.sh`.

## [0.2.2] - 2026-09-25

### Changed

- Updated visual identity and branding assets:
  * Replaced master boot logo (`assets/bootlogo.png`) and splash banner (`assets/splash.png`).
  * Re-generated baseline 256x256 RGB NRO icon (`assets/icon.jpg`) complying with Nintendo Switch homebrew menu specifications.
  * Re-derived in-app raster assets (`assets/images/logo.png`) for RomFS staging.
- Synchronized asset derivative pipeline:
  * Updated lock metadata in `assets/derivatives.json` linking all outputs to master asset SHA-256 digests.
  * Verified pipeline validation and asset linter parity across container and host environments.

## [0.2.1] - 2026-09-25

### Added

- Animated Preload & Splash Screen (`SplashScreen`):
  * Modern startup preloader with pure black background (#000000).
  * Centered typography logo with smooth fade-in entrance animation (0.0 to 1.0 opacity).
  * Shimmer / skeleton sweep effect using diagonal NanoVG linear gradients across the logo.
  * Slim horizontal progress bar (#E60012 crimson fill on #1A1A1A track) indicating subsystem initialization stages.
  * Non-blocking asynchronous initialization of system info, storage, network protection, and manifest cache during splash.
- Startup Update Alert Modal Dialog:
  * Automatic non-intrusive update detection following splash screen preloading.
  * Informational modal dialog displaying current versus remote version comparison.
  * One-touch redirect button taking user straight to `UpdateTab`.
  * Fully localized dialog strings in both `en-US` and `pt-BR`.

## [0.2.0] - 2026-09-25

### Added

- Modular Dashboard Grid (`HomeTab`):
  * Hardware model detection (Switch V1/V2, OLED, Lite).
  * Real-time Horizon OS Firmware and Atmosphere (AMS) version badges.
  * SD Card storage bar gauge and filesystem detection (FAT32 vs exFAT warning).
  * System status and protection cards with telemetry blocking check.
  * Interactive quick action tiles with single-stroke crimson focus highlight.
- Sidebar Overhaul:
  * Compact width (~230px) allocating more horizontal space for dashboard content.
  * Sleek minimalist selection with vertical crimson indicator bar (`|`) and highlighted text.
  * Embedded vector glyph icons for all tabs (Início, Atualização, Atmosphere, Firmware, Ferramentas, Configurações).
- Custom Dark Minimalist Theme with Nintendo Crimson Red (#E60012) accents and OLED true blacks.
- Catalog & Package Installation Services:
  * `CatalogService` integration for remote/cached manifest management.
  * `CfwInstallService` and `FirmwareInstallService` wired with modal progress dialog and cooperative B-button cancellation.
- Maintenance & Diagnostics Utilities (`ToolsTab`):
  * Reboot to Atmosphere payload via `splSetConfig` / `bpcDoReboot`.
  * Recursive FAT archive-bit directory attribute fixer.
  * Atmosphere sysmodule toggle manager (`/atmosphere/contents/` and `boot2.flag`).
  * Non-blocking DNS shield and telemetry connection diagnostic checker.
  * Safe cleanup service for staging and temporary caches.
- Streamlined Linear Settings (`SettingsTab`):
  * Clean linear menu items replacing stock Nintendo settings lists.
  * Application build information, changelog viewer, legal licenses, and update channel options.
- Terminology alignment: Updated all references from legacy "CFW" to "AMS" (Atmosphere).

## [0.1.0] - 2026-09-21

### Added

- Repository foundation: layered source tree, CMake build skeleton, CI and release workflows,
  commit and release conventions, and the full documentation set including 15 ADRs.
- Containerised build and verification environment (`Dockerfile`, `docker-compose.yml`,
  `docker/entrypoint.sh`). Developers and CI run the same tasks in the same image, with
  devkitA64, devkitARM, the Switch portlibs and a pinned clang 18 inside it.
- `nsx::core::SemVer` - strict and tolerant version parsing with SemVer section 11 precedence,
  covering the comparison defects that made the predecessor's updater offer downgrades.
- Pinned Mozilla CA bundle embedded as a compile-time blob, verified at configure time.
- RCM payload ported from the predecessor with its GPL-2.0-only headers intact, built by its own
  devkitARM Makefile and staged into romfs as opaque data.

[Unreleased]: https://github.com/mateussantoos/nsx-manager/compare/v0.3.2...HEAD
[0.3.2]: https://github.com/mateussantoos/nsx-manager/compare/v0.3.1...v0.3.2
[0.3.1]: https://github.com/mateussantoos/nsx-manager/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/mateussantoos/nsx-manager/compare/v0.2.5...v0.3.0
[0.2.5]: https://github.com/mateussantoos/nsx-manager/compare/v0.2.4...v0.2.5
[0.2.4]: https://github.com/mateussantoos/nsx-manager/compare/v0.2.3...v0.2.4
[0.2.3]: https://github.com/mateussantoos/nsx-manager/compare/v0.2.2...v0.2.3
[0.2.2]: https://github.com/mateussantoos/nsx-manager/compare/v0.2.1...v0.2.2
[0.2.1]: https://github.com/mateussantoos/nsx-manager/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/mateussantoos/nsx-manager/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/mateussantoos/nsx-manager/releases/tag/v0.1.0
