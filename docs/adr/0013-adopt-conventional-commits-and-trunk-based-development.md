---
status: "accepted"
date: 2026-09-23
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["process"]
---

# 0013. Adopt Conventional Commits and trunk-based development

## Context and Problem Statement

The predecessor's 27 commits already used Conventional Commits fairly consistently - but nothing
enforced it, so subjects ran to 110 characters, some commits had no scope, scopes were invented
ad hoc (`(standards)`, `(toolchain)`, `(touch)`), and at least one commit message described work
that is not in the tree (`300af6a`, "add comprehensive Doxygen international documentation across
all headers and source files", which produced no Doxyfile and no output).

There were also no tags and no branches beyond `main`, so there was no relationship between
history and releases.

We want release notes generated from history rather than hand-written, which requires history to
be machine-readable.

## Decision Drivers

* `git-cliff` must be able to generate a changelog without human editing.
* A convention nobody checks is a convention that decays - the predecessor proves it.
* A single maintainer: process must not cost more than it returns.
* Committing must not require a Node toolchain in a C++ repository.

## Considered Options

* Conventional Commits with a fixed scope vocabulary, trunk-based, squash-merge
* Conventional Commits with free-form scopes
* git-flow with `develop` and `release/*` branches
* No enforced convention

## Decision Outcome

Chosen option: **Conventional Commits with a fixed scope vocabulary, on a trunk-based flow with
squash-merge only.**

**Commits.** Types `feat fix perf refactor docs style test build ci chore revert`. Scopes come
from `tools/hooks/scopes.txt` - one file, read by both enforcers - and mirror the source tree, so
the scope tells you where the change landed. Compound scopes like `ui/tabs` are valid when every
part is. Scope is **required** for `feat`, `fix`, `perf` and `refactor`; optional for
housekeeping. Subject: imperative, lowercase, no trailing period, header at most 72 characters,
**ASCII only** - the mechanical proxy for "written in English". Breaking changes need `!` **and**
a `BREAKING CHANGE:` footer, so neither a human nor `git-cliff` can miss one.

**Two enforcers, one list.** `tools/hooks/commit-msg` is POSIX `sh` with no dependencies, because
a C++ repository must not require `npm install` before you can commit. `commitlint.config.js`
runs in CI and reads the same `scopes.txt`. If they ever disagree, that is a bug in one of them -
and `commitlint.yml` cross-checks the PR title through both.

**Branching.** Trunk-based. `main` is protected, linear, always releasable. Branches are
`<type>/<kebab-summary>[-#issue]` and short-lived - under three days. Long work goes behind an
`NSX_FEATURE_*` CMake option, not a long-lived branch. `release/X.Y.x` is created only when an
older minor needs a patch.

**Squash-merge only**, which is why the PR **title** is linted: it becomes the commit on `main`.

### Consequences

* Good, because `git log --oneline main` *is* the changelog, and `git-cliff` generates release
  notes with no hand editing.
* Good, because the scope vocabulary makes `git log --grep='(update)'` a useful query.
* Good, because a fixed vocabulary forces a deliberate decision when a new area appears - adding
  a scope is a visible commit rather than an invented word.
* Good, because the local hook gives feedback in under a second, before CI.
* Bad, because contributors must learn the format. Mitigated by `.gitmessage`, by the hook's
  error messages naming the exact problem, and by `docs/contributing/commit-convention.md`.
* Bad, because squash-merge discards intermediate commits, so a carefully staged series becomes
  one commit. Accepted: a clean `main` is worth more here than branch archaeology.
* Neutral, because the rules are duplicated in `sh` and JavaScript. They share the scope list,
  which is the part that actually changes.

## Pros and Cons of the Options

### Conventional Commits, fixed scopes, trunk-based

* Good, because history is machine-readable, which is the prerequisite for generated notes.
* Good, because trunk-based suits a single maintainer with no parallel release lines.
* Bad, because the fixed vocabulary needs occasional maintenance.

### Conventional Commits, free-form scopes

* Good, because there is no list to maintain and no friction when adding an area.
* Bad, because scopes fragment - `(ui)`, `(UI)`, `(interface)` and `(frontend)` all appear, and
  the changelog grouping becomes noise. The predecessor's ad hoc scopes show the beginning of
  this.

### git-flow

* Good, because it separates in-progress work from released work explicitly, which helps when
  several versions are supported simultaneously.
* Bad, because `develop` adds a permanent merge tax and a second "what is actually released?"
  question, for zero benefit when releases are cut from tags on `main`.
* Bad, because it is designed for teams with scheduled releases, which this is not.

### No enforced convention

* Good, because zero friction.
* Bad, because release notes must be written by hand, which means eventually they are not written
  at all.

## More Information

* [Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/)
* [`tools/hooks/scopes.txt`](../../tools/hooks/scopes.txt)
* [`docs/contributing/commit-convention.md`](../contributing/commit-convention.md)
* [`docs/contributing/branching.md`](../contributing/branching.md)

Revisit if a second regular contributor joins and the squash-merge policy starts losing
information that matters.
