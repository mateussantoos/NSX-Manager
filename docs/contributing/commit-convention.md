# Commit convention

[Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/), enforced.

Decision: [ADR-0013](../adr/0013-adopt-conventional-commits-and-trunk-based-development.md).

## Format

```
<type>(<scope>): <subject>

<body>

BREAKING CHANGE: <what breaks and how to migrate>
Refs: #123
```

## Types

| Type | Use for | Appears in the changelog |
|---|---|---|
| `feat` | New functionality | **Added** |
| `fix` | A bug fix | **Fixed** |
| `perf` | A performance improvement | **Changed** |
| `refactor` | Restructuring with no behaviour change | no |
| `docs` | Documentation only | no |
| `style` | Formatting, whitespace - no code change | no |
| `test` | Tests only | no |
| `build` | Build system, dependencies | no |
| `ci` | Workflows and CI configuration | no |
| `chore` | Housekeeping | no |
| `revert` | Reverting a previous commit | **Removed** |

## Scopes

From [`tools/hooks/scopes.txt`](../../tools/hooks/scopes.txt) - the single list both the local
hook and CI read, so they cannot disagree.

Scopes mirror the source tree: `core` `semver` `version` `manifest` `policy` `handoff` `paths`
`text` `json` `hash` `log` · `platform` `fs` `power` `system` `network` `romfs` `launch` ·
`infra` `http` `tls` `github` `archive` `storage` `mega` · `domain` `update` `cfw` `firmware`
`catalog` `motd` · `ui` `shell` `tabs` `pages` `widgets` `theme` `sound` `i18n` · `app`
`forwarder` `rcm` · `assets` `build` `cmake` `deps` `ci` `release` `tools` `docs` `adr` `tests`
`repo` `security`

Compound scopes work when every part is valid: `refactor(ui/tabs): ...`

**A scope is required** for `feat`, `fix`, `perf` and `refactor`; optional for the rest.

Need a scope that does not exist? Add it to `scopes.txt` **in the same commit**. That is
deliberate: a new area of the codebase should be a visible decision, not an invented word.

## Subject

* Imperative mood - "add", not "added" or "adds"
* Lowercase first letter
* No trailing period
* The whole header is at most **72 characters**
* **ASCII only** - the mechanical proxy for "written in English"

## Breaking changes

Both markers are required: `!` after the scope **and** a `BREAKING CHANGE:` footer. One is for the
human skimming `git log`; the other is for `git-cliff`. Requiring both means neither can be
missed.

```
feat(update)!: require manifest schema_version 2

Adds a required `signatures` array. Clients on schema_version 1 will refuse the
manifest and prompt for a manual update, which is the intended behaviour.

BREAKING CHANGE: update.json now requires schema_version 2. Clients older than
0.5.0 cannot update in-app and must reinstall from the zip.
```

## Examples

Good:

```
feat(semver): add strict and tolerant semver parsers
fix(http): enable tls peer verification with the embedded ca bundle
refactor(ui/tabs): split the about tab into independent cards
docs(adr): record the decision to own the content manifest
ci(release): validate the generated manifest before publishing
test(handoff): cover rollback when attempts are exhausted
build(deps): bump borealis to 3f2a1c9
```

Rejected, and why:

| Message | Problem |
|---|---|
| `Fixed the updater` | Not a valid type; past tense; no scope |
| `feat: add parser` | `feat` requires a scope |
| `feat(bogus): add parser` | `bogus` is not in `scopes.txt` |
| `feat(semver): Add parser` | Subject must start lowercase |
| `feat(semver): add parser.` | No trailing period |
| `feat(ui): adicionar tradução` | Non-ASCII - commits are in English |
| `feat(semver)!: change the api` | `!` without a `BREAKING CHANGE:` footer |

## How it is enforced

**Locally** - [`tools/hooks/commit-msg`](../../tools/hooks/commit-msg), POSIX `sh`, no
dependencies. A C++ repository must not require `npm install` before you can commit. Install with
`tools/hooks/install.sh`. It names the exact rule you broke.

**In CI** - `commitlint.yml` runs commitlint over every commit on the branch, over the PR title,
and then re-checks the title through the local hook so the two enforcers cannot silently diverge.

**The PR title matters.** We squash-merge, so the title becomes the commit on `main`. A title
that is not a valid Conventional Commit will not merge.

`git commit --no-verify` skips the local hook. CI does not care.

## Body

Optional, wrapped at 100 columns. Explain **why**, not what - the diff shows what.

```
fix(github): persist backoff state across launches

Backoff lived in memory, so quitting and relaunching reset it and the app
hammered a failing endpoint on every start. It now lives in
/config/nsx-manager/cache/backoff.json.

Refs: #42
```

## Footers

| Footer | Meaning |
|---|---|
| `BREAKING CHANGE: <text>` | Required alongside `!` |
| `Refs: #123` | Related issue |
| `Closes: #123` | Closes on merge |
| `Co-authored-by: Name <email>` | Credit |
