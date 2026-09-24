# NSX Manager - Project Scope

**Status:** authoritative
**Applies to:** `mateussantoos/nsx-manager`, from 0.1.0 onward
**Last reviewed:** 2026-09-23

This document defines what NSX Manager is, how the repository is organised, and the standards
every change is held to. Where a rule here has a recorded rationale, it links to an
[ADR](adr/README.md). Where a rule is machine-checked, it names the script that checks it.

If this document and the code disagree, that is a bug in one of them - say so in an issue.

---

## Table of contents

1. [Product definition](#1-product-definition)
2. [Lineage and what changes](#2-lineage-and-what-changes)
3. [Goals](#3-goals)
4. [Non-goals](#4-non-goals)
5. [Architecture](#5-architecture)
6. [Directory layout](#6-directory-layout)
7. [Naming conventions](#7-naming-conventions)
8. [Versioning](#8-versioning)
9. [Commit convention](#9-commit-convention)
10. [Branching and pull requests](#10-branching-and-pull-requests)
11. [Release standard](#11-release-standard)
12. [Update and content sources](#12-update-and-content-sources)
13. [Security requirements](#13-security-requirements)
14. [Documentation standard](#14-documentation-standard)
15. [Testing standard](#15-testing-standard)
16. [Licensing](#16-licensing)
17. [Language policy](#17-language-policy)
18. [Definition of done](#18-definition-of-done)
19. [Road to 1.0.0 - parity checklist](#19-road-to-100---parity-checklist)
20. [Open questions](#20-open-questions)

---

## 1. Product definition

NSX Manager is a **Nintendo Switch homebrew application** that runs on the console and manages a
custom firmware installation. It is distributed as a single `.nro` launched from the homebrew
menu.

It manages:

| Domain | What it does |
|---|---|
| CFW pack | Lists and installs releases of the NSX Pack custom firmware bundle |
| Official firmware | Downloads Nintendo firmware sets and hands off to Daybreak for installation |
| Itself | Checks GitHub Releases, verifies, and self-updates with rollback |
| Saves | Backs up and restores save data; integrates with JKSV |
| Cheats | Installs cheat databases into the Atmosphere contents directory |
| Ports | Installs homebrew ports from a catalogue |
| Translations and mods | Installs community translation and modification packs |
| Maintenance tools | Reboot to payload, archive-bit repair, temp cleanup, sysmodule toggles, FTP server, DNS and network configuration, controller diagnostics, theme selection |

**Audience:** users running the NSX Pack CFW distribution, predominantly Brazilian, on both
Erista and Mariko consoles.

**Runtime:** ARMv8 (aarch64) under Atmosphere, via devkitA64 + libnx, with a
[Borealis](https://github.com/natinusala/borealis) user interface.

---

## 2. Lineage and what changes

NSX Manager is a **complete rewrite** of NSX Updater 4.2.1 (itself a fork of
[aio-switch-updater](https://github.com/HamletDuFromage/aio-switch-updater), rebranded from the
CNX Pack lineage). No code is carried over except the RCM payload, which is vendored unchanged
for licensing reasons ([section 16](#16-licensing)).

The rewrite exists because three defects in the predecessor were structural, not incidental:

| Defect in NSX Updater 4.2.1 | Evidence | Consequence |
|---|---|---|
| The self-update endpoint pointed at a repository that does not exist | `constants.hpp:11` set `GITHUB_USER = "NSX"`, producing `api.github.com/repos/NSX/nsx-updater`; the 404 was swallowed at `download.cpp:493-500` | **No user has ever received an update.** The feature looked present and was inert. |
| Version comparison stripped every non-digit and compared the result as one integer | `main_frame.cpp:53-73` turned `"v4.2.1"` into `421` | `4.2.1-hotfix2` became `4212` and was offered as an "update" over `4.2.1` - a downgrade. A date-style tag overflowed `std::stoi`, throwing an uncaught `std::out_of_range`. |
| TLS verification was disabled on every request, and unverified archives were extracted to the SD card root | `download.cpp:140-141`, `:198-199`, `:353-354`, `:472-473` set `CURLOPT_SSL_VERIFYPEER` and `CURLOPT_SSL_VERIFYHOST` to `0`; `utils.cpp:173` did `chdir("/")` before extracting | Any network attacker could replace the CFW pack, the firmware, or the application itself. The only check was a four-byte `PK\x03\x04` magic test. |

Underneath those sat the shape that made them hard to see or fix: 38 `.cpp` files in one flat
directory, an 822-line `utils.cpp` mixing networking, filesystem, UI dialogs and power
management, a 565-line `download.cpp` mixing HTTP with Mega.nz crypto and GitHub API parsing, no
tests, no CI, no documentation, and a 5,209-line dead file still being compiled and linked into
the shipped binary.

**This project's foundational rule follows from that:** every standard below is enforced by a
script in [`tools/`](../tools/) that runs in CI. A convention nothing checks is a convention that
decays.

---

## 3. Goals

1. **Updates that work and are verifiable.** Every release is reachable from the app, every
   downloaded byte is hash-checked before use, and a failed update rolls back.
2. **A codebase that can be reasoned about in pieces.** Strict layering, small modules, pure
   logic separated from I/O.
3. **Logic that is testable without a console.** Everything interesting compiles and runs on a
   normal computer.
4. **Decisions that survive their author.** Every foundational choice is an ADR that records the
   rejected alternative and why.
5. **Standards that are mechanical.** Naming, formatting, layering, commits, licences and version
   single-sourcing are all checked by CI, not by memory.
6. **Honest internationalisation.** English source strings, real translations, no fake locales.

## 4. Non-goals

Stated explicitly so they can be pointed at when a request arrives:

- **Piracy.** NSX Manager does not download, install, index or link to commercial game content -
  no ROMs, no NSPs, no XCIs, no title keys. This is not negotiable and is not revisited.
- **Circumventing online bans or Nintendo's services.** No ban-evasion tooling, no telemetry
  spoofing beyond the DNS-blocking configuration that already ships with every CFW distribution.
- **Being a CFW.** NSX Manager installs and manages Atmosphere and hekate. It does not replace
  them.
- **Running off-console.** There is no desktop, web or mobile client. It runs on the Switch.
- **Supporting other CFWs.** Atmosphere only. ReiNX and SX OS detection existed in the
  predecessor and is not carried over.
- **A plugin or scripting system.** Remote code that is not part of a hash-verified release is
  outside the trust model.
- **Backwards compatibility with NSX Updater's SD layout.** New paths, new config directory. See
  [section 12](#12-update-and-content-sources).

---

## 5. Architecture

Six layers. Dependencies point **downward only**.

```
app        composition root - wires everything, owns main()
  |
ui         Borealis views. No network, no filesystem, no rules.
  |
domain     use-cases. Orchestrates; depends on interfaces, not implementations.
  |
infra      adapters to the outside world: HTTP, GitHub, archives, storage
  |
platform   thin libnx wrappers: fs, power, system, network, romfs, launch
  |
core       pure C++20. No libnx, no curl, no Borealis. Compiles on any host.
```

| Layer | May depend on |
|---|---|
| `core` | the C++ standard library, and nothing else |
| `platform` | `core` |
| `infra` | `core`, `platform` |
| `domain` | `core`, `infra` |
| `ui` | `core`, `domain` |
| `app` | everything |

**Enforced by** [`tools/lint/check_layering.sh`](../tools/lint/check_layering.sh), which reads
every `#include "nsx/..."` and fails on an upward or sideways edge. Compilers do not enforce
layering; nothing else would stop a UI file from including a curl header.

**Why `core` is special.** It is the layer with the bugs worth testing - version comparison,
manifest parsing, path construction, hashing. Keeping it free of platform dependencies is what
lets the same source files compile a second time with a host compiler and run under `ctest` in
seconds ([ADR-0014](adr/0014-split-pure-logic-into-a-host-testable-core-library.md)).

Details: [`architecture/overview.md`](architecture/overview.md),
[`architecture/layering.md`](architecture/layering.md),
[ADR-0003](adr/0003-use-a-layered-source-tree-with-co-located-headers.md).

---

## 6. Directory layout

```
nsx-manager/
├── .github/            workflows, issue and PR templates, CODEOWNERS
├── apps/
│   ├── forwarder/      nsx-forwarder.nro - completes the self-update swap
│   └── rcm-payload/    nsx_rcm.bin - hekate/BDK derived, GPL-2.0-only, ISOLATED
├── assets/             staged into romfs: images, sounds, data, i18n
├── cmake/              NsxVersion, NsxLayer, NsxPortlibs, NsxCaBundle, NsxRomfs, NsxRcmPayload
├── docker/             entrypoint and doctor for the build container
├── Dockerfile          devkitA64 + devkitARM + pinned clang + verification tooling
├── docs/               this file, ADRs, architecture, contributing, user, reference
├── src/nsx/            the application, one directory per layer
│   ├── core/           result, version, update, paths, text, json, hash, net, log
│   ├── platform/       fs, power, system, network, romfs, launch
│   ├── infra/          http, github, archive, storage, mega
│   ├── domain/         selfupdate, cfw, firmware, catalog, motd
│   ├── ui/             app_shell, tabs, pages, widgets, i18n
│   └── app/            main.cpp, bootstrap, container
├── tests/              unit/ and fixtures/ - host only
├── third_party/        pinned submodules and deliberate single-header vendors
└── tools/              hooks/ lint/ release/ deps/ cacert/
```

**Rules:**

- **One include root.** Every project include is `#include "nsx/<layer>/<module>/<file>.hpp"`.
  No relative includes, no `-I` sprawl.
- **Headers sit beside their sources.** `semver.hpp` lives next to `semver.cpp`. There is no
  parallel `include/` tree to drift out of sync. Anything inside a `detail/` directory is
  private.
- **Source lists are explicit.** Modules are named in a `CMakeLists.txt`, never globbed - a glob
  hides a new file from an unchanged build directory.
- **No file may be dead.** If nothing references it, delete it. The predecessor linked a
  5,209-line `icons_page.cpp` that nothing instantiated into every shipped binary.

Full map: [`architecture/source-tree.md`](architecture/source-tree.md).

---

## 7. Naming conventions

| Entity | Convention | Example |
|---|---|---|
| Directories | `snake_case`, singular | `src/nsx/core/update/` |
| Files | `snake_case`, stem is the primary type in snake_case | `SemVer` -> `semver.hpp` / `semver.cpp` |
| Namespaces | lowercase, `nsx::<layer>::<module>`; `detail` for private | `nsx::infra::http` |
| Types (class, struct, enum, concept, alias) | `PascalCase` | `UpdateManifest`, `ContentKind` |
| Enum constants | `PascalCase` | `UpdateAction::UpToDate` |
| **Functions and methods** | **`camelCase`** | `parseSemVer()`, `createDownloadItems()` |
| Locals and parameters | `camelCase` | `statusCode` |
| Private and protected members | `m_camelCase` | `m_statusText` |
| Constants and `constexpr` | `kPascalCase` | `kMaxFetchLinks` |
| Macros | `UPPER_SNAKE_CASE` (avoid entirely) | `NSX_HANDOFF_PATH` |
| Test files | `<module>_test.cpp` | `semver_test.cpp` |
| i18n keys | `dotted.lower.case` | `update.progress.verifying` |
| Branches | `<type>/<kebab-summary>[-#issue]` | `fix/tls-clock-skew-#42` |

**Headers use `#pragma once`,** not include guards.

**Include order** (clang-format regroups automatically): own header, C standard library, C++
standard library, third-party, then project `"nsx/..."` includes.

**Enforced by** `.clang-tidy`'s `readability-identifier-naming` (warnings are errors) and
`.clang-format`. The predecessor mixed `CreateDownloadItems()` with `createList()` in the same
file; `camelCase` is the single answer.

Details: [`contributing/coding-style.md`](contributing/coding-style.md).

---

## 8. Versioning

**[Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html), starting at `0.1.0`.**

- `MAJOR` - breaking change to the SD layout, the config format, the update manifest schema, or
  a removed feature.
- `MINOR` - new functionality, backwards compatible.
- `PATCH` - bug fixes only.
- Pre-release: `1.0.0-rc.1`. Ranks **below** `1.0.0`, per SemVer section 11.

**While on `0.x` the project is explicitly pre-release.** The public API and SD layout may change
in any minor. `1.0.0` is cut when [section 19](#19-road-to-100---parity-checklist) is complete -
not on a date.

We deliberately did **not** continue the predecessor's `4.2.1`, a number inherited from the
upstream fork and unrelated to this codebase. Nothing is lost by restarting: the old
self-updater never functioned, so there is no upgrade path to preserve.

### One source of truth

[`/VERSION`](../VERSION) holds the bare version, no `v` prefix. It flows outward and is never
retyped:

```
/VERSION -> cmake/NsxVersion.cmake -> version.hpp -> nsx::core::version::kString
                                                  -> the NACP (shown by hbmenu)
                                                  -> the curl User-Agent
                                                  -> the UI footer
```

**Enforced by** [`forbid_hardcoded_version.sh`](../tools/lint/forbid_hardcoded_version.sh), which
fails CI on any version literal under `src/`, and
[`check_version.sh`](../tools/release/check_version.sh), which fails the release unless the tag
matches `/VERSION`.

The predecessor had **five** copies of `4.2.1` (`Makefile:25`, the user-agent at
`download.cpp:22`, the splash footer at `splash_page.cpp:183`, a badge at `tools_tab.cpp:321`,
and a changelog entry at `changelog_page.cpp:154`). Bumping the version reliably missed at least
one.

[ADR-0004](adr/0004-adopt-semantic-versioning-from-0-1-0-with-v-prefixed-tags.md)

### Building

The build and verification environment is a container carrying devkitA64, devkitARM, the Switch
portlibs and a pinned clang. `docker compose run --rm nsx <task>` is what developers and CI both
run, so the two cannot drift.
[ADR-0015](adr/0015-build-and-verify-inside-a-container.md)

---

## 9. Commit convention

**[Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/).**

```
<type>(<scope>): <subject>

<body>

BREAKING CHANGE: <what breaks and how to migrate>
Refs: #123
```

**Types:** `feat` `fix` `perf` `refactor` `docs` `style` `test` `build` `ci` `chore` `revert`.

**Scopes** come from [`tools/hooks/scopes.txt`](../tools/hooks/scopes.txt) - the single list both
enforcers read. Compound scopes like `ui/tabs` are valid when every part is. A scope is
**required** for `feat`, `fix`, `perf` and `refactor`; optional for housekeeping types.

**Subject:** imperative mood, lowercase first letter, no trailing period, header at most 72
characters, **ASCII only** (the mechanical proxy for "written in English"). Body wrapped at 100.

**Breaking changes** need `!` after the scope **and** a `BREAKING CHANGE:` footer. Both, so
neither a human skimming `git log` nor `git-cliff` can miss one.

```
feat(update): verify sha-256 before staging a downloaded nro
fix(http): enable tls peer verification with the embedded ca bundle
refactor(ui/tabs): split the about tab into independent cards
feat(update)!: require manifest schema_version 2
docs(adr): record the decision to own the content manifest
```

**Two enforcers, one list.** [`tools/hooks/commit-msg`](../tools/hooks/commit-msg) is POSIX `sh`
and runs locally - a C++ repository must not require `npm install` before you can commit.
[`commitlint.config.js`](../commitlint.config.js) runs in CI and reads the same `scopes.txt`.
Install the hook with [`tools/hooks/install.sh`](../tools/hooks/install.sh).

[ADR-0013](adr/0013-adopt-conventional-commits-and-trunk-based-development.md) ·
[`contributing/commit-convention.md`](contributing/commit-convention.md)

---

## 10. Branching and pull requests

**Trunk-based.** `main` is always releasable.

- `main` is protected: no direct pushes, linear history, required checks (`lint`, `manifest`,
  `host-tests`, `build-switch`, `docs`, `commits`).
- Branches are short-lived - target under three days. Long work goes behind an `NSX_FEATURE_*`
  CMake option, not a long-lived branch.
- Branch names: `<type>/<kebab-summary>[-#issue]`, using the commit type vocabulary.
- **Squash-merge only.** `main` gets exactly one Conventional Commit per change, so
  `git log --oneline main` *is* the changelog. This is why the PR **title** is linted too.
- `release/X.Y.x` maintenance branches are created only when an older minor needs a patch. They
  are not pre-created.

[ADR-0013](adr/0013-adopt-conventional-commits-and-trunk-based-development.md) ·
[`contributing/branching.md`](contributing/branching.md)

---

## 11. Release standard

**Every release is a GitHub Release, created by CI from an annotated tag. There is no other
distribution channel.**

### Tags

`v<major>.<minor>.<patch>[-<prerelease>]` - `v0.1.0`, `v1.0.0-rc.1`. Created only by
[`tools/release/tag.sh`](../tools/release/tag.sh), only on `main`, and only after
`check_version.sh` passes. Pushing the tag triggers
[`release.yml`](../.github/workflows/release.yml).

### Assets - a contract, not a convention

| Asset | Purpose |
|---|---|
| `nsx-manager-<version>.nro` | **What the in-app updater downloads.** A bare file: no archive, no extraction, no path traversal, one hash-verified rename. |
| `nsx-manager-<version>.zip` | **First-time human installation only.** Contains exactly `switch/nsx-manager/nsx-manager.nro` and nothing else, so extracting at the SD root can never clobber user configuration. |
| `nsx-forwarder-<version>.nro` | Repair and recovery. Also embedded in romfs. |
| `update.json` | **Stable filename** - `releases/latest/download/update.json` always resolves to the newest one. |
| `SHA256SUMS` | Digests for every binary, for manual verification. |
| `RELEASE_NOTES.md` | Generated from Conventional Commits by `git-cliff`; rendered in the in-app changelog. |

Asset names are parsed by [`gen_manifest.py`](../tools/release/gen_manifest.py) to assign the
manifest `kind`. Renaming an asset is a breaking change to the update pipeline.

### Release procedure

1. `tools/release/tag.sh <version>` - bumps `/VERSION`, commits, creates the annotated tag.
2. `git push origin main && git push origin v<version>`.
3. CI: guard (tag matches `/VERSION`, tag is on `main`) -> build all three binaries -> package ->
   generate `update.json` and `SHA256SUMS` -> **validate the manifest with the same validator the
   client contract is defined by** -> publish -> mirror the manifest to `release-metadata`.

The validation step exists because you must never ship a manifest the application cannot read.

[`contributing/releasing.md`](contributing/releasing.md) ·
[ADR-0005](adr/0005-distribute-updates-via-github-releases-with-a-published-manifest.md) ·
[ADR-0008](adr/0008-ship-a-bare-nro-for-in-app-updates-and-a-zip-for-first-install.md)

---

## 12. Update and content sources

**Everything NSX Manager depends on is owned by `mateussantoos`.**

| What | Source |
|---|---|
| The application itself | `github.com/mateussantoos/nsx-manager` releases |
| The update manifest | `releases/latest/download/update.json` |
| Manifest fallback | the `release-metadata` orphan branch (raw.githubusercontent.com) |
| The content catalogue | a manifest published from a repository under `mateussantoos` |

The predecessor read its catalogue from `gamemod.com.br/nx-links/nx-links-v421.json` with
hardcoded fallbacks to `CostelaCNX/CNX` and `CostelaCNX/NX-Firmwares`, plus a literal `"AMSNX"`
string blacklist repeated in four places to route around an organisation that had gone dead. All
of that is dropped. Third-party sources are now **entries inside our own manifest**, so a dead
upstream is a manifest edit rather than a new binary.

### Why the client never calls `api.github.com` on startup

Unauthenticated GitHub REST is **60 requests per hour per IP**, and an `If-None-Match` request
that returns `304` still decrements the budget - so ETag caching alone does not solve it. On a
shared or carrier-NAT'd network the budget is consumed by strangers. This is why the predecessor
hit 403s (commit `7b3cdf6`, "fix github releases 403") while calling the API twice per launch.

The manifest is fetched from `github.com/.../releases/latest/download/update.json`, a web
redirect to the release CDN that is not subject to that budget. On top of that: a six-hour local
cache, exponential backoff with jitter persisted across launches (base 60 s, cap 6 h), and a
mirror branch. `api.github.com` is used for exactly one optional screen - release history -
behind a user action, cached 24 hours, degrading to "unavailable" rather than to an error.

### SD card paths

| Path | Contents |
|---|---|
| `/switch/nsx-manager/nsx-manager.nro` | the application |
| `/switch/nsx-manager/nsx-forwarder.nro` | the repair entry point, visible in hbmenu |
| `/config/nsx-manager/` | settings, cache, logs, staging - created at first run |

Full inventory: [`reference/sd-layout.md`](reference/sd-layout.md).

---

## 13. Security requirements

This application downloads executable code and writes it to a user's SD card. These are
requirements, not aspirations, and each one is checked.

1. **TLS verification is always on.** `CURLOPT_SSL_VERIFYPEER = 1`,
   `CURLOPT_SSL_VERIFYHOST = 2`, TLS 1.2 minimum, `https` only for both initial and redirected
   requests. devkitPro's curl is built against libnx's `ssl` service, so verification runs
   against the firmware trust store the console itself uses. A pinned Mozilla bundle is compiled
   in and used automatically if a future curl accepts it - see
   [ADR-0016](adr/0016-verify-tls-against-the-firmware-trust-store.md), which corrects the
   mbedTLS assumption in ADR-0006.
   *Enforced by* [`forbid_insecure_curl.sh`](../tools/lint/forbid_insecure_curl.sh).

2. **A console clock that is wrong produces a clear message, never a downgrade.** Switch RTC skew
   surfaces as `CURLE_PEER_FAILED_VERIFICATION`; the UI says "set your console clock".

3. **Nothing is used before its hash is checked.** Downloads land as `<asset>.part`, are SHA-256
   hashed during the write, compared against both `size` and `sha256` from the manifest in
   constant time, and only then renamed. A `.part` file is never executed, extracted or
   chainloaded; stale ones are deleted at startup.

4. **No path from remote data reaches a filesystem call unguarded.** Asset names must be bare
   filenames. Archive entries are traversal-checked before extraction.

5. **A failed update rolls back.** The previous binary is kept until the new one is verified in
   place; an attempt counter bounds retries; a second forwarder copy ships at
   `/switch/nsx-manager/nsx-forwarder.nro` as "NSX Manager (Repair)" so there is always a
   launchable entry point.

6. **Signing is a known gap.** The current trust anchor is TLS plus digests published in a
   release asset served over TLS. Minisign signatures are a proposed follow-up, recorded rather
   than forgotten.

[`architecture/threat-model.md`](architecture/threat-model.md) ·
[ADR-0006](adr/0006-verify-tls-with-an-embedded-ca-bundle-and-mandate-sha-256.md) ·
[`SECURITY.md`](../SECURITY.md)

---

## 14. Documentation standard

| Audience | Lives in |
|---|---|
| Anyone | [`README.md`](../README.md) |
| Users | [`docs/user/`](user/installation.md) |
| Contributors | [`CONTRIBUTING.md`](../CONTRIBUTING.md) -> [`docs/contributing/`](contributing/local-setup.md) |
| Maintainers and future readers | [`docs/architecture/`](architecture/overview.md), [`docs/adr/`](adr/README.md) |
| Machines and integrators | [`docs/reference/`](reference/update-manifest.md) |
| API | Doxygen, published to GitHub Pages |

**Rules:**

- **Every architectural decision gets an ADR**, in trimmed [MADR 4.0](https://adr.github.io/madr/)
  format, numbered `NNNN-kebab-title.md`. Numbers are never reused or renumbered. A decision that
  changes **supersedes** the old ADR; the old one is never edited beyond its front-matter.
  *Enforced by* [`check_adr_index.sh`](../tools/lint/check_adr_index.sh), which fails when the
  index and the files disagree, or when a `superseded-by` link is not reciprocal.

- **MADR over Nygard** because every decision here has a real runner-up, and the rejected option
  plus its reason is the part that stops a decision being relitigated in six months.

- **Every public entity in a header under `src/nsx/` is documented.** `@brief` mandatory,
  `@param` for each parameter, `@return` for each non-void return, `@throws` where applicable.
  Doxygen runs with `EXTRACT_ALL = NO` and `WARN_AS_ERROR = FAIL_ON_WARNINGS`, so an undocumented
  public entity fails the build. `detail::` namespaces, `.cpp`-local statics, `third_party/` and
  `apps/rcm-payload/` are exempt.

  This configuration is deliberate. The predecessor has a commit titled *"add comprehensive
  Doxygen international documentation across all headers and source files"* that produced doc
  comments, no Doxyfile, no build and no output - nothing was ever verified, and the comments
  have since rotted.

- **Documentation changes ship with the change.** A pull request that alters behaviour without
  updating the affected page is incomplete.

- **All documentation is in English**, UTF-8, LF, wrapped at 100 columns.

---

## 15. Testing standard

Switch homebrew cannot run on CI hardware. The answer is to make the logic worth testing not need
hardware.

**Host-tested** - all of `core/`, compiled with the host compiler from the same source list the
Switch build uses, run under `ctest` with [doctest](https://github.com/doctest/doctest):

`semver` (parsing and precedence) · `update_policy` · `manifest` (against the golden fixtures) ·
`handoff` (the state machine) · `sd_paths` and `traversal_guard` · `sha256` (NIST vectors and
chunk-boundary splits) · `text` · `json` · `mega_link`.

**Device-only** - `platform/`, `infra/http`, `ui/`, the forwarder swap, the RCM payload. Covered
by the numbered pre-release smoke checklist in
[`contributing/testing.md`](contributing/testing.md): fresh install; update from N-1; network cut
mid-download; corrupted staged NRO; forced forwarder failure and rollback; applet mode; Erista
and Mariko; exFAT and FAT32.

**Fixtures are specifications.** [`tests/fixtures/manifests/`](../tests/fixtures/manifests/)
records exactly what a hostile or broken `update.json` must produce.
`validate_manifest.py --check-fixtures` runs in CI, so the parser contract cannot drift silently.

**Coverage** is measured on `core/` only. Report-only until the module set settles around 0.3.0,
then 80% line and 70% branch enforced. It is never measured on `ui/` - that would only create
pressure to write worthless tests.

[ADR-0014](adr/0014-split-pure-logic-into-a-host-testable-core-library.md)

---

## 16. Licensing

**The application is GPL-3.0-only.** Borealis is GPL-3.0, so the application must be.

**The RCM payload is GPL-2.0-*only*.** hekate/BDK carries a GPLv2 notice with **no** "or any
later version" clause. GPL-2.0-only and GPL-3.0 cannot be combined into one program.

**Resolution: hard isolation** - which is already how the code behaves. The payload is a separate
freestanding ARM7 program, built by devkitARM from its own Makefile at its own load address. The
application never links it, never includes a header from it, and treats the built `nsx_rcm.bin`
as opaque data read from romfs. Two independent programs shipped in one archive is mere
aggregation, which GPLv2 section 2 permits.

Every first-party file carries `SPDX-License-Identifier: GPL-3.0-only`. Every payload file keeps
its original GPL-2.0 header untouched.

*Enforced by* [`check_license_isolation.sh`](../tools/lint/check_license_isolation.sh), which
fails on an include crossing the boundary in either direction, and on a missing SPDX line.

[`architecture/licensing.md`](architecture/licensing.md) ·
[ADR-0011](adr/0011-license-gplv3-and-isolate-the-gplv2-only-rcm-payload.md) ·
[`NOTICE`](../NOTICE)

---

## 17. Language policy

**English is the project language.** Code, identifiers, comments, commit messages, documentation,
issues and pull requests.

**The user interface is translated.** Every user-visible string is an i18n key - never a literal
in a `.cpp` file. `assets/i18n/en-US/` is the source of truth, authored and reviewed first.
`pt-BR` is a first-class shipped translation, because that is who uses this software. A locale
directory exists only if it is genuinely translated.

*Enforced by* [`check_i18n.py`](../tools/lint/check_i18n.py), which fails on a missing key, an
extra key, or a differing `{}` placeholder count between a locale and `en-US`.

This is a direct response to what the predecessor shipped: twelve locale directories in which
eight were byte-identical copies of each other, `en-US/menus.json` contained Portuguese, and
`ja/menus.json` read *"O {} ({}) usa a licenca GPL-3.0"*. Meanwhile the real UI strings were
hardcoded Portuguese literals in nine `.cpp` files that bypassed i18n entirely. The app claimed
twelve languages and spoke one.

[ADR-0012](adr/0012-use-borealis-i18n-with-en-us-as-the-source-of-truth.md)

---

## 18. Definition of done

A change is done when all of these hold:

- [ ] It compiles for the Switch and, if it touches `core/`, for the host.
- [ ] `tools/lint/run_all.sh` passes.
- [ ] New logic in `core/` has host tests; anything device-only names what was tested on hardware.
- [ ] Public API has Doxygen comments that build without warnings.
- [ ] New user-visible strings are keys, present in both `en-US` and `pt-BR`.
- [ ] Affected documentation is updated in the same pull request.
- [ ] An architectural decision is recorded as an ADR, with its index row.
- [ ] The commit and the PR title are valid Conventional Commits.
- [ ] No version literal, no disabled TLS verification, no layering violation, no source shared
      with `apps/rcm-payload/`.

---

## 19. Road to 1.0.0 - parity checklist

`1.0.0` is cut when NSX Manager does everything NSX Updater 4.2.1 did, correctly, plus the
foundations that release did not have. Ordered by milestone, not by priority within one.

### 0.1.x - foundation

- [x] Repository, standards, CI, documentation and ADRs
- [ ] `core/` modules: `semver`, `manifest`, `update_policy`, `handoff`, `sd_paths`, `sha256`
- [ ] Host test suite green in CI
- [x] Borealis and zipper pinned as submodules; the tree builds
- [x] Containerised build and verification environment (ADR-0015)
- [x] RCM payload ported and building
- [x] CA bundle pinned and embedded
- [ ] A minimal bootable shell that reports its own version

### 0.2.x - the update path

- [ ] `infra/http` with TLS verification and the embedded CA bundle
- [ ] `infra/github` manifest fetch with cache, backoff and mirror fallback
- [ ] `domain/selfupdate`: download, verify, stage
- [ ] The forwarder: swap, attempt counter, rollback, repair mode
- [ ] `v0.2.0` published and **updated to from `v0.1.0` on real hardware** - the first time this
      product line has ever successfully self-updated

### 0.3.x - content

- [ ] The content catalogue manifest, published under `mateussantoos`
- [ ] CFW pack listing and installation
- [ ] Official firmware listing and Daybreak handoff
- [ ] Coverage gate enabled on `core/`

### 0.4.x - the rest of the feature surface

- [ ] Saves (backup, restore, JKSV)
- [ ] Cheats
- [ ] Ports with the catalogue and cover art
- [ ] Translations and mods, with search and multi-select
- [ ] Tools: reboot to payload, archive bit, temp cleanup, sysmodules, FTP, DNS test, network
      configuration, controller test, themes
- [ ] MOTD and changelog screens

### 0.5.x - polish

- [ ] Complete `en-US` and `pt-BR` locales with parity enforced
- [ ] Theme system actually consulted by the drawing code (the predecessor's was not)
- [ ] Touch input across every screen
- [ ] Applet-mode handling and memory limits verified

### 1.0.0 - gates

- [ ] Every box above ticked
- [ ] The full device smoke checklist passes on both Erista and Mariko
- [ ] An update path from every published 0.x verified on hardware
- [ ] No `TODO` or `FIXME` in `src/`
- [ ] Every ADR is `accepted` or `superseded` - none still `proposed`
- [ ] `SECURITY.md` response process exercised at least once, even if only as a drill

---

## 20. Open questions

Tracked here so they are not silently decided. Each becomes an ADR when answered.

| # | Question | Blocking |
|---|---|---|
| 1 | Do we sign releases with minisign, and where does the public key live so it can be rotated? | 1.0.0 |
| 2 | Where is the content catalogue published - a release asset, an orphan branch, or its own repository? | 0.3.0 |
| 3 | Do we keep Mega.nz download support at all, or drop it with the sources that needed it? | 0.3.0 |
| 4 | Does the FTP server return, and if so with what authentication? The predecessor's accepted **any** password and served the whole SD card, while its changelog advertised a PIN that did not exist in the code. | 0.4.0 |
| 5 | Do we offer a beta channel at 1.0.0, or keep `update-beta.json` unused until there is demand? | 1.0.0 |
| 6 | Is Borealis 2.x worth migrating to after 1.0.0, or do we stay on the pinned fork? | post-1.0 |

---

## Changing this document

This is a living document, but not a casual one. Changing a **standard** here means changing the
ADR that records it and the script that enforces it, in the same pull request. Changing scope -
adding a goal, or moving something out of [non-goals](#4-non-goals) - needs its own ADR.

Use `docs(repo): ...` for editorial changes and `docs(adr): ...` when a decision moves.
