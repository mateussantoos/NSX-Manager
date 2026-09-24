# Local setup

**The supported way to build NSX Manager is the container.** It carries devkitA64, devkitARM, the
Switch portlibs, a pinned clang, CMake, Ninja, Doxygen and the Python tooling - and it is the
same image CI runs, so "works on my machine" and "passes CI" stop being different questions.

A native toolchain still works and is documented at the bottom, but nothing in this project
requires you to install devkitPro on your own machine.

## Requirements

Docker with Compose v2, and git. That is all.

```sh
git clone --recurse-submodules https://github.com/mateussantoos/nsx-manager.git
cd nsx-manager
docker compose build          # once, roughly 5 minutes
docker compose run --rm nsx doctor
```

`doctor` prints every toolchain the container provides and flags anything missing. Run it first
whenever something behaves oddly.

## The tasks

```sh
docker compose run --rm nsx <task>
```

| Task | What it does |
|---|---|
| `switch` | configure + build the Switch release: app, forwarder, RCM payload |
| `switch-debug` | the same, debug configuration |
| `rcm` | only the RCM payload, via devkitARM |
| `host` | configure + build `nsx_core` and the tests |
| `test` | host build, then `ctest` |
| `asan` | host build under ASan and UBSan, then `ctest` |
| `lint` | every check in `tools/lint`, exactly as CI runs them |
| `format` | rewrite sources in place with the pinned clang-format |
| `tidy` | clang-tidy over the host compilation database |
| `docs` | Doxygen, with warnings as errors |
| `manifest` | validate the `update.json` fixtures and a generated manifest |
| `release <tag>` | package a release into `dist/` the way `release.yml` does |
| `archive` | extraction against real hostile zip archives |
| `export` | build the Switch targets and copy the `.nro` files to `dist/` |
| `verify` | lint + test + archive + manifest + docs - **run this before pushing** |
| `all` | `verify`, then the Switch build |
| `doctor` | report what the container provides |
| `shell` | interactive shell with everything on `PATH` |

Named shortcuts exist for the common ones:

```sh
docker compose run --rm nsx switch
docker compose run --rm nsx test
docker compose run --rm nsx verify
```

Anything unrecognised runs verbatim, so this works too:

```sh
docker compose run --rm nsx cmake --version
docker compose run --rm nsx ctest --preset host-debug -R semver --output-on-failure
```

## How the mounts work

The repository is bind-mounted at `/workspace`, so edits on your machine are visible immediately
and generated files land back in your working tree.

**Build trees live in a named volume**, not in the bind mount. On Docker Desktop for Windows and
macOS a bind mount crosses a filesystem boundary, and compiling tens of thousands of objects
across it is several times slower. The consequence worth knowing: `build/` inside the container
is not the same directory as `build/` on your host.

To get built artefacts onto your machine, use the `export` task:

```sh
docker compose run --rm nsx export
```

It builds the Switch targets and copies every `.nro` into `dist/`, which **is** in the bind mount,
then prints sizes and SHA-256 digests so a copy to the SD card can be checked rather than assumed.
A truncated NRO fails on the console with no explanation.

Copying by hand needs the real paths, which are nested per target - there is no `.nro` at the root
of the build tree:

```sh
build/switch-release/src/nsx/app/nsx-manager.nro
build/switch-release/apps/forwarder/nsx-forwarder.nro
build/switch-release/apps/ui-probe/nsx-ui-probe.nro
```

And the destination must be inside `/workspace`. Copying to anywhere else - including
`/workspace/build`, which the named volume covers - writes into the container and disappears with
it.

`ccache` and the devkitPro package cache are also named volumes, so they survive `--rm`.

## The fastest loop

Most work on `core/` needs neither devkitPro nor a console:

```sh
docker compose run --rm nsx test
```

Seconds. `nsx_core` is pure C++20 compiled by the host compiler from the same source list the
Switch build uses.

## Before you push

```sh
docker compose run --rm nsx verify
```

Lint, host tests, manifest contract and Doxygen - the same checks that gate a pull request.

## Editor integration

Generate a compilation database in the container, then point your editor at it. Because the build
tree is a volume, copy it into the working tree first:

```sh
docker compose run --rm nsx bash -c \
  'cmake --preset host-debug >/dev/null && cp build/host-debug/compile_commands.json /workspace/'
```

VS Code: set `"C_Cpp.default.compileCommands": "${workspaceFolder}/compile_commands.json"`.
It is gitignored.

`.editorconfig` and `.clang-format` are respected by every major editor. If you format locally,
use **clang-format 18** - the version the container pins. Other major versions produce different
output and will fight the lint.

---

## Native toolchain (optional)

Only if you would rather not use Docker.

| Tool | Version |
|---|---|
| devkitPro with `switch-dev` and `devkitARM` | current |
| Switch portlibs | `switch-zlib switch-curl switch-mbedtls switch-glfw switch-glad switch-mesa switch-libdrm_nouveau` |
| CMake | 3.24+ |
| Ninja | any |
| clang / clang-format / clang-tidy | **18** |
| Python | 3.10+, with `jsonschema` |

```sh
cmake --preset switch-release && cmake --build --preset switch-release
cmake --preset host-debug && ctest --preset host-debug --output-on-failure
```

On Windows use the devkitPro MSYS2 shell, not Git Bash or PowerShell - `$DEVKITPRO` and the
toolchain `PATH` are only set up there.

**Sanitizers do not work on Windows.** clang's ASan is incompatible with the MSVC debug CRT, so
`host-asan` aborts inside `ucrtbased.dll` during CRT startup, before any test runs. Use the
container, where they work.

## Troubleshooting

| Symptom | Fix |
|---|---|
| `third_party/borealis is empty` | `git submodule update --init --recursive` |
| `The CA bundle is missing` | `tools/cacert/fetch.sh` - it is gitignored, only its hash is committed. The container does this for you. |
| `Missing Switch portlib 'z'` | Native build without the portlibs. Install them, or use the container. |
| `VERSION must be SemVer 2.0.0` | `/VERSION` is malformed - the guard working as intended |
| `no sources yet - target skipped` | Expected until that module lands |
| clang-format disagrees with CI | Use version 18, or just run `docker compose run --rm nsx format` |
| Docker build is slow the first time | It downloads devkitPro packages. Subsequent builds are cached. |
| Commit rejected by the hook | Read the message - it names the exact rule. See [`commit-convention.md`](commit-convention.md) |
