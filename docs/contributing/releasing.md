# Releasing

The runbook. Releases are cut from tags on `main` and built entirely by CI.

Standard: [`PROJECT_SCOPE.md` section 11](../PROJECT_SCOPE.md#11-release-standard).

## Before you tag

- [ ] `main` is green
- [ ] The device smoke checklist in [`testing.md`](testing.md) passes - **including the update
      path from the previous release on real hardware**
- [ ] `CHANGELOG.md` `[Unreleased]` has anything a commit subject could not carry
- [ ] No ADR is still `proposed` for something this release ships
- [ ] Docs match behaviour

## Cut it

```sh
git switch main && git pull
tools/release/tag.sh 0.2.0
```

That script bumps `/VERSION`, commits `chore(release): bump version to 0.2.0`, and creates the
annotated tag `v0.2.0`. It refuses if you are not on `main`, if the tree is dirty, if the tag
exists, or if the version is not SemVer.

Review, then publish:

```sh
git push origin main
git push origin v0.2.0      # this is the point of no return
```

Undo before pushing:

```sh
git tag -d v0.2.0 && git reset --hard HEAD~1
```

## What CI does

[`release.yml`](../../.github/workflows/release.yml):

1. **guard** - `check_version.sh` confirms the tag matches `/VERSION`, and that the tag is on
   `main`. Nothing is built until both pass.
2. **build** - all three binaries in `devkitpro/devkita64`.
3. **package** - `nsx-manager-<v>.nro`, `nsx-forwarder-<v>.nro`, and `nsx-manager-<v>.zip`
   containing exactly `switch/nsx-manager/nsx-manager.nro`.
4. **notes** - `git-cliff` generates `RELEASE_NOTES.md` from the Conventional Commits since the
   previous tag.
5. **manifest** - `gen_manifest.py` computes sizes and hashes **from the built files**, so they
   cannot disagree with what is published.
6. **validate** - `validate_manifest.py` runs over the generated manifest. **You cannot ship a
   manifest the client refuses to read.**
7. **checksums** - `SHA256SUMS`.
8. **publish** - the GitHub Release, with every asset.
9. **mirror** - `update.json` pushed to the `release-metadata` orphan branch.

A tag containing a hyphen (`v1.0.0-rc.1`) is published as a prerelease on the `beta` channel
automatically.

## After publishing

- [ ] The manifest resolves and looks right:

```sh
curl -sL https://github.com/mateussantoos/nsx-manager/releases/latest/download/update.json | jq .
```

- [ ] Download the `.nro` and check it against `SHA256SUMS`
- [ ] **Update to it from the previous version on a real console.** This is the only check that
      proves the release actually works.
- [ ] Announce

## Publishing a mandatory update

For a security fix, the manifest's `mandatory` flag blocks the main menu until the user updates.
It is a per-release decision, not a build-time one.

Set it by running `gen_manifest.py --mandatory` in the workflow for that release. Use it
sparingly - it takes the console away from the user until they act.

## Raising `min_supported`

When a release changes the SD layout or config format such that an older client cannot safely
update in-app, set `--min-supported` to the oldest version that can. Clients below it are told to
reinstall from the zip, with the link, instead of being walked into a broken update.

## If something goes wrong

**Bad release published.** Do not delete the tag - someone may already have it. Fix forward: cut
a patch release immediately. If the bad release is actively harmful, mark it as a prerelease in
the GitHub UI so `releases/latest` moves back to the previous good one.

**Tag pushed with the wrong version.** The guard job will have failed, so nothing was published.
Delete the tag locally and remotely, fix `/VERSION`, tag again.

**The manifest is wrong but the binaries are fine.** Regenerate and re-upload `update.json` as a
release asset. The filename is stable, so clients pick it up on their next check.

## Version numbers

[SemVer 2.0.0](https://semver.org/spec/v2.0.0.html) - see
[ADR-0004](../adr/0004-adopt-semantic-versioning-from-0-1-0-with-v-prefixed-tags.md).

| Bump | When |
|---|---|
| `MAJOR` | Breaking change to SD layout, config format, or manifest schema |
| `MINOR` | New functionality, backwards compatible |
| `PATCH` | Bug fixes only |

While on `0.x`, a minor may break things. `1.0.0` is cut against the parity checklist in
[`PROJECT_SCOPE.md` section 19](../PROJECT_SCOPE.md#19-road-to-100---parity-checklist).
