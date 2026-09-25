# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Released sections are generated from [Conventional Commits](docs/contributing/commit-convention.md)
by `git-cliff` during the release workflow - do not hand-edit them. Add entries under
`[Unreleased]` only when a change needs prose that a commit subject cannot carry.

<!-- next-header -->

## [Unreleased]

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

[Unreleased]: https://github.com/mateussantoos/nsx-manager/compare/v0.2.1...HEAD
[0.2.1]: https://github.com/mateussantoos/nsx-manager/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/mateussantoos/nsx-manager/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/mateussantoos/nsx-manager/releases/tag/v0.1.0
