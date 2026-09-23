# NSX Manager documentation

## Start here

**[PROJECT_SCOPE.md](PROJECT_SCOPE.md)** - what NSX Manager is, how the repository is organised,
and every standard the project holds itself to. If you read one page, read that one.

## By audience

### Users

| Page | |
|---|---|
| [Installation](user/installation.md) | Install and verify |
| [Updating](user/updating.md) | How updates work, and recovery if one fails |
| [Troubleshooting](user/troubleshooting.md) | When something goes wrong |
| [FAQ](user/faq.md) | Common questions |

### Contributors

| Page | |
|---|---|
| [Local setup](contributing/local-setup.md) | Toolchain, building, the fast test loop |
| [Coding style](contributing/coding-style.md) | Naming, errors, documentation, what is forbidden |
| [Commit convention](contributing/commit-convention.md) | Conventional Commits, enforced |
| [Branching](contributing/branching.md) | Trunk-based, squash-merge |
| [Testing](contributing/testing.md) | Host tests and the device smoke checklist |
| [CI](contributing/ci.md) | What each workflow checks, and why |
| [Releasing](contributing/releasing.md) | The release runbook |

### Maintainers and reviewers

| Page | |
|---|---|
| [Architecture overview](architecture/overview.md) | The map |
| [Layering](architecture/layering.md) | The dependency rule and its enforcement |
| [Source tree](architecture/source-tree.md) | Every directory, and where the old god-files went |
| [Build system](architecture/build-system.md) | CMake targets, presets, version single-sourcing |
| [Update pipeline](architecture/update-pipeline.md) | End to end, with the failure matrix |
| [Self-update forwarder](architecture/self-update-forwarder.md) | The swap and its recovery states |
| [Threat model](architecture/threat-model.md) | Trust boundaries and known gaps |
| [Licensing](architecture/licensing.md) | The GPL-2.0 / GPL-3.0 problem and its resolution |
| [**Architecture decisions**](adr/README.md) | 15 ADRs - why each of the above is as it is |

### Integrators and machines

| Page | |
|---|---|
| [Update manifest](reference/update-manifest.md) | `update.json` explained |
| [`update.schema.json`](reference/update.schema.json) | The machine-readable schema |
| [Handoff format](reference/handoff-format.md) | `handoff.json` and the swap state machine |
| [SD layout](reference/sd-layout.md) | Every path NSX Manager reads or writes |
| [Error codes](reference/error-codes.md) | The error catalogue |
| [API reference](https://mateussantoos.github.io/nsx-manager/) | Doxygen, published from `main` |

## Also

| | |
|---|---|
| [Roadmap](ROADMAP.md) | Milestones to 1.0.0 |
| [Glossary](GLOSSARY.md) | CFW, RCM, NRO, romfs, Erista, Mariko, forwarder... |
| [Changelog](../CHANGELOG.md) | What changed |
| [Security policy](../SECURITY.md) | Reporting a vulnerability |

## How this documentation works

- **All in English**, UTF-8, LF, wrapped at 100 columns.
- **Documentation ships with the change.** A pull request that alters behaviour without updating
  the affected page is incomplete.
- **Decisions go in ADRs**, not in prose scattered across pages. If you find yourself explaining
  *why* something is the way it is in three places, it wants an ADR.
- **Claims are grounded.** Where a rule exists because the predecessor got something wrong, the
  page cites the file and line. Where a rule is machine-checked, it names the script.
- **Checked in CI.** Relative links must resolve, the ADR index must match the files, and Doxygen
  runs with warnings as errors.
