# Architecture Decision Records

Every foundational decision in NSX Manager is recorded here, in
[MADR 4.0](https://adr.github.io/madr/) (trimmed). An ADR captures not just what was decided but
**what was rejected and why** - that is the part that stops a settled question being reopened in
six months.

New decision? Copy [`template.md`](template.md), take the next number, set `status: proposed`,
and add your row to the table below in the same pull request.

## Index

| ADR | Title | Status | Date |
|---|---|---|---|
| [0001](0001-record-architecture-decisions-using-madr.md) | Record architecture decisions using MADR | accepted | 2026-09-23 |
| [0002](0002-build-with-cmake-and-the-devkitpro-switch-toolchain.md) | Build with CMake and the devkitPro Switch toolchain file | accepted | 2026-09-23 |
| [0003](0003-use-a-layered-source-tree-with-co-located-headers.md) | Use a layered source tree with co-located headers | accepted | 2026-09-23 |
| [0004](0004-adopt-semantic-versioning-from-0-1-0-with-v-prefixed-tags.md) | Adopt Semantic Versioning from 0.1.0 with v-prefixed tags | accepted | 2026-09-23 |
| [0005](0005-distribute-updates-via-github-releases-with-a-published-manifest.md) | Distribute updates via GitHub Releases with a published manifest | accepted | 2026-09-23 |
| [0006](0006-verify-tls-with-an-embedded-ca-bundle-and-mandate-sha-256.md) | Verify TLS with an embedded CA bundle and mandate SHA-256 | accepted | 2026-09-23 |
| [0007](0007-self-update-by-staging-an-nro-and-chainloading-a-forwarder.md) | Self-update by staging an NRO and chainloading a forwarder | accepted | 2026-09-23 |
| [0008](0008-ship-a-bare-nro-for-in-app-updates-and-a-zip-for-first-install.md) | Ship a bare NRO for in-app updates and a zip for first install | accepted | 2026-09-23 |
| [0009](0009-vendor-dependencies-as-pinned-git-submodules.md) | Vendor dependencies as pinned git submodules | accepted | 2026-09-23 |
| [0010](0010-use-the-borealis-fork-as-the-ui-framework.md) | Use the Borealis fork as the UI framework | accepted | 2026-09-23 |
| [0011](0011-license-gplv3-and-isolate-the-gplv2-only-rcm-payload.md) | License GPLv3 and isolate the GPL-2.0-only RCM payload | accepted | 2026-09-23 |
| [0012](0012-use-borealis-i18n-with-en-us-as-the-source-of-truth.md) | Use Borealis i18n with en-US as the source of truth | accepted | 2026-09-23 |
| [0013](0013-adopt-conventional-commits-and-trunk-based-development.md) | Adopt Conventional Commits and trunk-based development | accepted | 2026-09-23 |
| [0014](0014-split-pure-logic-into-a-host-testable-core-library.md) | Split pure logic into a host-testable core library | accepted | 2026-09-23 |
| [0015](0015-build-and-verify-inside-a-container.md) | Build and verify inside a container | accepted | 2026-09-23 |

## Reading order

If you are new to the codebase, these four explain the most:

1. **[0003](0003-use-a-layered-source-tree-with-co-located-headers.md)** - why the source tree
   looks the way it does, and what the layering rule buys.
2. **[0005](0005-distribute-updates-via-github-releases-with-a-published-manifest.md)** - how
   updates reach users, and why the client never calls `api.github.com` on startup.
3. **[0006](0006-verify-tls-with-an-embedded-ca-bundle-and-mandate-sha-256.md)** - the trust
   model. This application writes executables to an SD card.
4. **[0011](0011-license-gplv3-and-isolate-the-gplv2-only-rcm-payload.md)** - why
   `apps/rcm-payload/` is walled off and must stay that way.

## Status lifecycle

```
proposed ──> accepted ──> deprecated
   │             │
   │             └──────> superseded   (by a newer ADR)
   │
   └──────> rejected
```

- **proposed** - written, not yet agreed. May be merged in this state to make the discussion
  reviewable.
- **accepted** - in force. The code is expected to match it.
- **rejected** - considered and declined. Kept, because "we thought about X and said no" is
  valuable.
- **deprecated** - no longer relevant; nothing replaces it.
- **superseded** - replaced by a newer ADR. Set `superseded-by: ["ADR-00NN"]`, and set
  `supersedes: ["ADR-00MM"]` on the new one. The link must be reciprocal.

## Rules

1. **Numbers are never reused and never renumbered.** Links rot otherwise.
2. **A superseded ADR is never deleted or edited** beyond its front-matter. It is the historical
   record of what was true.
3. **The index and the files must agree.** Checked by
   [`tools/lint/check_adr_index.sh`](../../tools/lint/check_adr_index.sh), which fails CI when an
   ADR is missing from the table, when the table lists a file that does not exist, when a status
   is invalid or disagrees with the front-matter, or when a `superseded-by` link is one-sided.
4. **An architectural change without an ADR is incomplete.** If your pull request changes a
   recorded decision, it changes the ADR too.

## What deserves an ADR

**Yes:** build system, source layout, dependency strategy, the update or trust model, licensing,
versioning, anything that would be expensive to reverse, and anything where you can imagine
someone asking "why on earth is it done this way?".

**No:** renaming a variable, adding a test, fixing a bug, or any choice a single commit message
fully explains.
