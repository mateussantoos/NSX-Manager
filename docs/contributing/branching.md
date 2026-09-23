# Branching

Trunk-based. `main` is always releasable.

Decision: [ADR-0013](../adr/0013-adopt-conventional-commits-and-trunk-based-development.md).

## The flow

```
main  --*------*------*------*--------*---- (protected, linear, always releasable)
         \    /        \    /          \
          *--*          *--*            tag v0.2.0 -> release.yml
        feat/semver   fix/tls-skew
```

1. Branch from `main`.
2. Work. Commit as often as you like - it will be squashed.
3. Open a pull request. **The title becomes the commit on `main`**, so it must be a valid
   Conventional Commit.
4. CI runs. All required checks must pass.
5. Squash-merge. Delete the branch.
6. Releases are cut from tags on `main` by `tools/release/tag.sh`.

## Branch names

```
<type>/<kebab-summary>[-#issue]
```

Same type vocabulary as commits.

```
feat/semver-parser
fix/tls-verify-clock-skew-#42
refactor/split-about-tab
docs/adr-content-manifest
chore/ci-ccache
```

## `main` is protected

| Setting | Value |
|---|---|
| Direct pushes | blocked |
| Linear history | required |
| Required checks | `lint`, `manifest`, `host-tests`, `build-switch`, `docs`, `commits` |
| Merge method | **squash only** |
| Stale review dismissal | on |

Squash-only is why `git log --oneline main` *is* the changelog, and why `git-cliff` can generate
release notes without anyone editing them.

## Keep branches short

Target: **under three days.**

A long-lived branch diverges, accumulates conflicts, and hides work from CI. If a feature is too
big for three days, put it behind a CMake option rather than a branch:

```cmake
option(NSX_FEATURE_CATALOG "Enable the content catalogue (incomplete)" OFF)
```

Merge the incomplete work disabled by default. It stays compiled, CI keeps checking it, and
enabling it later is a one-line change.

## Release branches

`release/X.Y.x` exists **only** when an older minor needs a patch after a newer minor has
shipped. They are not pre-created. Until that situation arises, `main` is the only long-lived
branch.

## `release-metadata`

An **orphan** branch with no shared history, written only by `release.yml`. It holds a mirror of
`update.json` for clients that cannot reach the GitHub release download path.

Never merge it into anything, and never commit to it by hand.

## Rebasing

Rebase your own branch onto `main` freely. Because we squash-merge, a messy branch history costs
nothing - so prefer rebasing over merge commits to keep the diff readable during review.

```sh
git fetch origin
git rebase origin/main
```

Do not rebase `main`. It is protected and linear already.
