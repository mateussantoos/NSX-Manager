# Continuous integration

What each workflow does, and why.

## Workflows

| Workflow | Trigger | Purpose |
|---|---|---|
| [`ci.yml`](../../.github/workflows/ci.yml) | PR, push to `main` | The main gate |
| [`commitlint.yml`](../../.github/workflows/commitlint.yml) | PR | Commit messages and PR title |
| [`release.yml`](../../.github/workflows/release.yml) | tag `v*.*.*` | Build and publish a release |
| [`docs-pages.yml`](../../.github/workflows/docs-pages.yml) | push to `main` | Doxygen to GitHub Pages |
| [`deps.yml`](../../.github/workflows/deps.yml) | weekly | Propose dependency bumps |

## `ci.yml` jobs

### `lint`

Runs every script in `tools/lint/`. Each one exists because the predecessor got that thing wrong:

| Check | Prevents |
|---|---|
| `check_encoding.sh` | Mojibake - the predecessor's README was UTF-8 read as Latin-1 |
| `check_format.sh` | Formatting drift |
| `check_layering.sh` | Upward dependencies that would make `core` untestable |
| `forbid_hardcoded_version.sh` | The five drifting copies of the version string |
| `forbid_insecure_curl.sh` | Disabled TLS verification |
| `check_license_isolation.sh` | GPL-2.0 and GPL-3.0 code being combined |
| `check_adr_index.sh` | The ADR index drifting from the files |
| `check_i18n.py` | Fake locales and mismatched placeholders |

Run them all locally first: `tools/lint/run_all.sh`.

### `manifest`

Validates that the golden fixtures still behave as
[`tests/fixtures/manifests/README.md`](../../tests/fixtures/manifests/README.md) documents, then
generates a manifest and validates it against its own schema. If the parser contract moves, this
job says so.

### `host-tests`

`cmake --preset host-debug` then `ctest`. No Docker, no cross-toolchain, seconds.

### `sanitizers`

`host-asan` under ASan and UBSan on Linux. This job exists on Linux specifically because clang's
ASan cannot run against the MSVC debug CRT, so the preset is unusable on the maintainer's own
Windows machine - the check would otherwise silently never run.

### `build-switch` and `build-rcm`

`devkitpro/devkita64` and `devkitpro/devkitarm` respectively. **Two separate jobs with two
separate toolchains**, which is the licensing boundary expressed as infrastructure - see
[`../architecture/licensing.md`](../architecture/licensing.md).

### `docs`

Doxygen with `EXTRACT_ALL = NO` and `WARN_AS_ERROR = FAIL_ON_WARNINGS`: an undocumented public
entity fails the build. Then a link check over all markdown.

This configuration is why the predecessor's situation cannot recur - it has a commit claiming
"comprehensive Doxygen documentation across all headers" that produced no Doxyfile, no build and
no output.

## Required checks

`main` will not accept a merge without: `lint`, `manifest`, `host-tests`, `sanitizers`, `build-switch`, `docs`
and `commits`.

`build-rcm` is not required until the payload is ported.

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
| `host-tests` | Reproduce with `ctest --preset host-debug --output-on-failure` |
| `build-switch` | Usually submodules: `git submodule update --init --recursive` |
| `docs` | An undocumented public entity, or a broken relative link |
| `manifest` | A fixture stopped behaving as documented - the parser contract moved |
