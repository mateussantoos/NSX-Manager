# Continuous integration

What each workflow does, and why.

## Workflows

| Workflow | Trigger | Purpose |
|---|---|---|
| [`ci.yml`](../../.github/workflows/ci.yml) | PR, push to `main` | The main gate, run inside the container |
| [`commitlint.yml`](../../.github/workflows/commitlint.yml) | PR | Commit messages and PR title |
| [`release.yml`](../../.github/workflows/release.yml) | tag `v*.*.*` | Build and publish a release |
| [`docs-pages.yml`](../../.github/workflows/docs-pages.yml) | push to `main` | Doxygen to GitHub Pages |
| [`deps.yml`](../../.github/workflows/deps.yml) | weekly | Propose dependency bumps |

## `ci.yml` jobs

**Every check runs inside the container** - the same image you get from
`docker compose build`. A job is one `docker run ... nsx <task>`, where the task is the same one
you run locally. There is no separate CI recipe that can drift from what developers use.

### `image`

Builds the toolchain image once and shares it with the other jobs through the GitHub Actions
cache. Everything else waits on it.

### `checks` (matrix)

One job per task: `lint`, `test`, `asan`, `manifest`, `docs`, `switch`, `tidy`.

`lint` runs every script in `tools/lint`. Each exists because the predecessor got that thing
wrong:

| Check | Prevents |
|---|---|
| `check_encoding.sh` | Mojibake and CRLF - the predecessor's README was UTF-8 read as Latin-1 |
| `check_format.sh` | Formatting drift |
| `check_layering.sh` | Upward dependencies that would make `core` untestable |
| `forbid_hardcoded_version.sh` | The five drifting copies of the version string |
| `forbid_insecure_curl.sh` | Disabled TLS verification |
| `check_license_isolation.sh` | GPL-2.0 and GPL-3.0 code being combined |
| `check_adr_index.sh` | The ADR index drifting from the files |
| `check_i18n.py` | Fake locales and mismatched placeholders |

`switch` builds all three binaries - the application, the forwarder, and the RCM payload through
its own devkitARM Makefile.

`asan` is why the sanitizers are useful at all: clang's ASan cannot run against the MSVC debug
CRT, so the `host-asan` preset is unusable on the maintainer's Windows machine. In the container
it works, so the check actually runs somewhere.

`docs` runs Doxygen with `EXTRACT_ALL = NO` and `WARN_AS_ERROR = FAIL_ON_WARNINGS`: an
undocumented public entity fails the build. This is why the predecessor's situation cannot
recur - it has a commit claiming "comprehensive Doxygen documentation across all headers" that
produced no Doxyfile, no build and no output.

### `pins`

`tools/deps/verify_pins.sh` - every submodule must sit on the commit `third_party/PINS.md`
records.

### `links`

Checks that relative links in the documentation resolve. Runs outside the container: it needs no
toolchain, so keeping it off the image's critical path gives faster feedback on a docs-only
change.

## Reproducing a CI failure

Exactly, on your machine:

```sh
docker compose run --rm nsx lint
docker compose run --rm nsx test
docker compose run --rm nsx switch
```

Same image, same command, same result.

## Why the clang version is pinned

The image installs clang **18** from LLVM's apt repository rather than whatever the base
distribution ships. clang-format output differs between major versions, so an unpinned toolchain
means the formatting lint gives one answer locally, another in the container and a third in CI -
and a check that disagrees with itself is one people learn to ignore.

## Action pinning

Every action is pinned by **commit SHA**, not by tag:

```yaml
uses: actions/checkout@11bd71901bbe5b1630ceea73d27597364c9af683 # v4.2.2
```

A tag can be moved by whoever owns the action; a SHA cannot. `dependabot.yml` keeps the pins
fresh, so this costs nothing ongoing.

## Why there is no CodeQL

CodeQL's C++ support needs to observe a build, and our build is a cross-compile inside a
devkitPro container against libnx headers it does not model. The result is near-total false
negatives plus noise, which trains people to ignore the security tab.

We get more from the targeted lints above - which encode this project's *actual* failure modes -
plus `.clang-tidy` with `bugprone-*`, `cert-*` and `clang-analyzer-*` as errors, plus ASan and
UBSan on host tests.

Revisit if CodeQL gains usable devkitA64 support.

## Debugging a failure

| Failure | Look at |
|---|---|
| `lint` | Run `tools/lint/run_all.sh` locally - same scripts, same results |
| `commits` | The hook message names the exact rule. Check the **PR title** too. |
| `test` | Reproduce with `docker compose run --rm nsx test` |
| `switch` | Usually submodules: `git submodule update --init --recursive` |
| `docs` | An undocumented public entity, or a broken relative link |
| `manifest` | A fixture stopped behaving as documented - the parser contract moved |
