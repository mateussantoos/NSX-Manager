# NSX Manager API reference {#mainpage}

Generated from the headers under `src/nsx/` and `apps/forwarder/src/`.

This is the **API reference only**. The prose documentation - architecture, ADRs, contributor
and user guides - lives in the repository and is meant to be read there, where its cross-links
resolve:

<https://github.com/mateussantoos/nsx-manager/tree/main/docs>

## Layers

The source tree is layered, and dependencies point downward only:

- `nsx::core` - pure C++20. No libnx, no curl, no Borealis. Compiles on any host, which is what
  makes it unit-testable without a console.
- `nsx::platform` - thin libnx wrappers.
- `nsx::infra` - adapters to the outside world: HTTP, GitHub, archives, storage.
- `nsx::domain` - use-cases.
- `nsx::ui` - Borealis views.
- `nsx::app` - the composition root.

Anything inside a `detail` namespace is private and is not documented here.

## Where to start

- `nsx::core::SemVer` and `nsx::core::parseSemVer` - version parsing and precedence.
- `nsx::core::Result` - the error-returning type used across layer boundaries.
- `nsx::infra::kCaBundlePem` - the embedded TLS trust anchor.
- `nsx::core::version` - build identity: version, git commit, user agent.
