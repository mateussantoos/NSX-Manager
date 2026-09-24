# Roadmap

Milestones to 1.0.0. The authoritative checklist is
[`PROJECT_SCOPE.md` section 19](PROJECT_SCOPE.md#19-road-to-100---parity-checklist); this page
adds the ordering and the reasoning.

**1.0.0 is cut when the work is done, not on a date.**

## 0.1.x - foundation (current)

**Goal: a repository that is correct by construction, and a binary that boots.**

- [x] Repository, standards, CI, documentation, 15 ADRs
- [ ] `core/`: `semver`, `manifest`, `update_policy`, `handoff`, `sd_paths`, `sha256`
- [ ] Host test suite green in CI
- [x] Borealis and zipper pinned; the tree builds
- [x] Containerised build environment, with the Switch build green
- [ ] A minimal bootable shell that reports its own version

`core/` comes first deliberately: it is testable without hardware, and it contains the logic the
predecessor got wrong.

## 0.2.x - the update path

**Goal: the thing this project exists for.**

- [x] `infra/http` with TLS verification (against the firmware trust store - ADR-0016)
- [x] Manifest fetch, six-hour cache, persisted backoff, mirror fallback
- [x] `domain/selfupdate`: download, verify, stage, hand off
- [x] The forwarder: swap, attempt counter, rollback, repair mode
- [x] The update flow reachable end to end - check, confirm, download, hand off, chainload
- [ ] Borealis UI for it; today the whole flow is a console prompt behind the Y button
- [ ] **`v0.2.0` published and updated to from `v0.1.0` on real hardware**

The cache and backoff landed in `core/update/` rather than `infra/github/` as originally
sketched: they are pure decisions about whether to make a request, not adapters for talking to
GitHub, and putting them in `core` is what makes the whole matrix host-testable.

That final item is the milestone that matters. It will be the first time this product line has
ever successfully updated itself.

## 0.3.x - content

**Goal: own the catalogue, stop depending on infrastructure we do not control.**

- [ ] The content catalogue manifest, published under `mateussantoos`
- [ ] CFW pack listing and installation
- [ ] Official firmware listing and Daybreak handoff
- [x] The RCM payload ported and building (moved up from 0.3.x - it came with the container work)
- [ ] Coverage gate enabled on `core/`

## 0.4.x - feature parity

**Goal: everything NSX Updater 4.2.1 did.**

- [ ] Saves (backup, restore, JKSV)
- [ ] Cheats
- [ ] Ports with the catalogue and cover art
- [ ] Translations and mods, with search and multi-select
- [ ] Tools: reboot to payload, archive bit, temp cleanup, sysmodules, FTP, DNS test, network
      configuration, controller test, themes
- [ ] MOTD and changelog screens

The FTP server returns **only** if [open question 4](PROJECT_SCOPE.md#20-open-questions) is
answered. The predecessor's accepted any password and served the entire SD card while advertising
a PIN that did not exist in the code.

## 0.5.x - polish

- [ ] Complete `en-US` and `pt-BR` locales with parity enforced
- [ ] Theme system actually consulted by the drawing code
- [ ] Touch input across every screen
- [ ] Applet-mode handling and memory limits verified

The theme item is not cosmetic: the predecessor had a `ThemeManager` that the drawing code never
called, so the themes did nothing.

## 1.0.0 - gates

- [ ] Every box above ticked
- [ ] The full device smoke checklist on both Erista and Mariko
- [ ] An update path from every published 0.x verified on hardware
- [ ] No `TODO` or `FIXME` in `src/`
- [ ] Every ADR `accepted` or `superseded` - none still `proposed`
- [ ] The `SECURITY.md` response process exercised at least once, even as a drill

## After 1.0.0

Not committed to, and recorded so they are not silently assumed:

| Idea | Tracked as |
|---|---|
| Minisign release signatures | [open question 1](PROJECT_SCOPE.md#20-open-questions) |
| Borealis 2.x migration | [open question 6](PROJECT_SCOPE.md#20-open-questions) |
| A beta channel | [open question 5](PROJECT_SCOPE.md#20-open-questions) |
| More locales | Whenever someone genuinely translates one |

## What will not happen

See [non-goals](PROJECT_SCOPE.md#4-non-goals). Briefly: no game content, no ban evasion, no
desktop client, no CFW of our own, no plugin system.
