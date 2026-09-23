# Local setup

## What you need

| Tool | Version | For |
|---|---|---|
| devkitPro with `switch-dev` | current | Building the app |
| devkitPro with `devkitARM` | current | Building the RCM payload (optional) |
| CMake | 3.24+ | Everything |
| Ninja | any | The generator the presets use |
| git | 2.30+ | Submodules |
| Python | 3.10+ | Lint and release tooling |
| clang-format | 18 | Formatting (CI uses 18; other versions differ) |
| Node | 20+ | commitlint only - **not needed to commit** |

Only CMake, Ninja, git, Python and a host C++ compiler are needed for the host test build. You
can contribute to `core/` without devkitPro at all.

## Clone

```sh
git clone --recurse-submodules https://github.com/mateussantoos/nsx-manager.git
cd nsx-manager
tools/hooks/install.sh
```

Forgot `--recurse-submodules`? `git submodule update --init --recursive`.
`tools/deps/verify_pins.sh` tells you when this is the problem.

`install.sh` sets `core.hooksPath` and the commit template, so your commit messages are validated
locally instead of failing in CI ten minutes later.

## The fastest loop: host tests

No Switch toolchain, no hardware.

```sh
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug --output-on-failure
```

This is where most work on `core/` happens. Add `--preset host-asan` to run under ASan and UBSan.

## Building for the Switch

### Windows (MSYS2) - the maintainer's environment

1. Install [devkitPro for Windows](https://github.com/devkitPro/installer/releases). It installs
   MSYS2 and the package manager.
2. Open the **MSYS2 MinGW 64-bit** shell from the Start menu, then:

```sh
pacman -S switch-dev devkitARM cmake ninja
```

3. Build:

```sh
cmake --preset switch-release
cmake --build --preset switch-release
```

Use the devkitPro MSYS2 shell, not Git Bash or PowerShell - `$DEVKITPRO` and the toolchain
`PATH` are only set up there.

### Linux and macOS

```sh
# see https://devkitpro.org/wiki/Getting_Started for pacman setup
sudo dkp-pacman -S switch-dev devkitARM
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=$DEVKITPRO/devkitARM
cmake --preset switch-release && cmake --build --preset switch-release
```

### Docker - closest to CI

```sh
docker run --rm -v "$PWD:/src" -w /src devkitpro/devkita64:latest \
  sh -c 'cmake --preset switch-release && cmake --build --preset switch-release'
```

The RCM payload needs `devkitpro/devkitarm` instead; CI builds them in separate jobs for exactly
this reason.

## Deploying to a console

```sh
# with nxlink (ships with devkitPro), console in hbmenu with nxlink listening
nxlink -s build/switch-release/nsx-manager.nro
```

Or copy it to `/switch/nsx-manager/nsx-manager.nro` on the SD card.

For anything touching the update path, test on **real hardware**. Emulators do not reproduce
FatFs rename semantics or `envSetNextLoad`, which is where the interesting failures live.

## Before you push

```sh
tools/lint/run_all.sh
```

Runs everything CI runs: encoding, formatting, layering, version literals, insecure curl, licence
isolation, the ADR index, and i18n parity.

## Editor setup

`.editorconfig` and `.clang-format` are respected by every major editor.

For IntelliSense, point your extension at the generated compilation database:

```
build/host-debug/compile_commands.json     # or build/switch-release/
```

VS Code: install the C/C++ extension and set `"C_Cpp.default.compileCommands"` to that path.

## Troubleshooting

| Symptom | Fix |
|---|---|
| `DEVKITPRO is not set` | Use the devkitPro MSYS2 shell, or export it |
| `third_party/borealis is empty` | `git submodule update --init --recursive` |
| `VERSION must be SemVer 2.0.0` | `/VERSION` is malformed - the guard doing its job |
| `no sources yet - target skipped` | Expected until that module lands |
| clang-format disagrees with CI | Use version 18; formatting differs between major versions |
| Commit rejected by the hook | Read the message - it names the exact rule. See [`commit-convention.md`](commit-convention.md) |
