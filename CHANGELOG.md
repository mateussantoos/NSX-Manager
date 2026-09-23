# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Released sections are generated from [Conventional Commits](docs/contributing/commit-convention.md)
by `git-cliff` during the release workflow - do not hand-edit them. Add entries under
`[Unreleased]` only when a change needs prose that a commit subject cannot carry.

<!-- next-header -->

## [Unreleased]

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

[Unreleased]: https://github.com/mateussantoos/nsx-manager/commits/main
