# Contributing to NSX Manager

Thanks for taking the time. This page is the map; the detail lives in
[`docs/contributing/`](docs/contributing/).

By participating you agree to the [Code of Conduct](CODE_OF_CONDUCT.md).

## Start here

| I want to... | Read |
|---|---|
| set up a build environment | [`local-setup.md`](docs/contributing/local-setup.md) |
| know how to name and format things | [`coding-style.md`](docs/contributing/coding-style.md) |
| write a commit message that passes CI | [`commit-convention.md`](docs/contributing/commit-convention.md) |
| know which branch to work on | [`branching.md`](docs/contributing/branching.md) |
| write or run tests | [`testing.md`](docs/contributing/testing.md) |
| understand what CI is checking | [`ci.md`](docs/contributing/ci.md) |
| cut a release | [`releasing.md`](docs/contributing/releasing.md) |
| understand why the code is shaped this way | [`docs/architecture/`](docs/architecture/overview.md) and [`docs/adr/`](docs/adr/README.md) |

## The five-minute version

```sh
git clone --recurse-submodules https://github.com/mateussantoos/nsx-manager.git
cd nsx-manager
tools/hooks/install.sh                 # commit-msg validation + commit template
docker compose build                   # once
docker compose run --rm nsx verify     # lint + tests + manifest + docs
```

You need Docker and git. The toolchain - devkitA64, devkitARM, clang, CMake, Ninja, Doxygen -
lives in the image, not on your machine.

Then branch, change, and open a pull request:

```sh
git switch -c feat/my-change
git commit                             # the template tells you the format
git push -u origin feat/my-change
```

## Rules that CI enforces

These are not style preferences; a pull request that breaks one will not merge.

1. **Conventional Commits**, with a scope from [`tools/hooks/scopes.txt`](tools/hooks/scopes.txt).
   The PR **title** is checked too, because we squash-merge and the title becomes the commit.
2. **Layering.** `core` depends on nothing but the standard library; each layer may only depend
   downward. See [`docs/architecture/layering.md`](docs/architecture/layering.md).
3. **No hardcoded version strings.** [`VERSION`](VERSION) is the only source of truth.
4. **No disabling TLS verification**, and no plaintext `http://` under `src/`.
5. **No hardcoded UI strings.** Every user-visible string is an i18n key, authored in `en-US`
   first. Locale files must agree on keys and on placeholder counts.
6. **Public API is documented.** Doxygen runs with `EXTRACT_ALL = NO` and warnings as errors, so
   an undocumented public entity fails the build.
7. **No source shared with `apps/rcm-payload/`.** It is GPL-2.0-only and must stay a separate
   program - see [`docs/architecture/licensing.md`](docs/architecture/licensing.md).

Every one of these exists because the predecessor project got it wrong. The reasoning is recorded
in the [ADRs](docs/adr/README.md).

## Proposing an architectural change

If your change alters a decision recorded in an ADR, or makes a new one of comparable weight,
add an ADR in the same pull request. Copy [`docs/adr/template.md`](docs/adr/template.md), take
the next number, set `status: proposed`, and add the row to
[`docs/adr/README.md`](docs/adr/README.md) - CI checks that the index and the files agree.

Never renumber or delete an existing ADR. Supersede it instead.

## Reporting bugs and vulnerabilities

Bugs: [open an issue](https://github.com/mateussantoos/nsx-manager/issues/new/choose).

Security problems: **do not** open an issue. Follow [`SECURITY.md`](SECURITY.md).
