---
status: "accepted"
date: 2026-09-23
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["architecture"]
---

# 0003. Use a layered source tree with co-located headers

## Context and Problem Statement

The predecessor put 38 `.cpp` files in one flat `source/` directory with a mirrored `include/`.
Two files absorbed everything that did not obviously belong elsewhere:

* `utils.cpp`, 822 lines, mixing string formatting, JSON access, filesystem operations, UI dialog
  construction, power management, GitHub API calls, zip dispatch and CFW detection. The file
  admits it: a `/* MY METHODS */` divider at line 411 separates inherited code from fork
  additions.
* `download.cpp`, 565 lines, mixing libcurl transport, Mega.nz AES-CTR key derivation, GitHub
  releases parsing and HTML `<title>` scraping.

The mirrored trees also drifted: `include/list_extra_tab.hpp` against
`source/list_extras_tab.cpp` - singular versus plural, in a pair that is supposed to match.

Consequences: nothing could be unit-tested, because every interesting function transitively
pulled in libnx. Nothing could be reasoned about locally. A network-layer function called
`brls::Application::crash()` (`download.cpp:381`).

## Decision Drivers

* Pure logic must be separable from I/O, or it cannot be tested off-device.
* A reader must be able to hold one module in their head.
* The dependency direction must be enforceable, not merely agreed.
* Adding a module must be one directory, not two edits in two trees.

## Considered Options

* Layered tree (`core/platform/infra/domain/ui/app`) with co-located headers
* Layered tree with mirrored `include/` and `src/` trees
* Feature-based tree (`update/`, `cfw/`, `saves/`, each self-contained)
* Keep the flat tree and just split the large files

## Decision Outcome

Chosen option: **a layered tree under `src/nsx/` with headers co-located beside their sources**,
one include root at `src/`.

```
core -> platform -> infra -> domain -> ui -> app
```

| Layer | May depend on |
|---|---|
| `core` | the C++ standard library only |
| `platform` | `core` |
| `infra` | `core`, `platform` |
| `domain` | `core`, `infra` |
| `ui` | `core`, `domain` |
| `app` | everything |

`utils.cpp` fans out to nine destinations and `download.cpp` to five; the full mapping is in
[`docs/architecture/source-tree.md`](../architecture/source-tree.md). `fetchTitle`, the HTML
scraper, is deleted outright - the manifest replaces it.

### Consequences

* Good, because `core/` has no platform dependency and therefore compiles on any host, which is
  what makes [ADR-0014](0014-split-pure-logic-into-a-host-testable-core-library.md) possible.
* Good, because `tools/lint/check_layering.sh` turns the dependency rule into a CI failure.
  Compilers do not enforce layering; without this, "we have layers" is aspiration.
* Good, because every include is `#include "nsx/<layer>/<module>/<file>.hpp"` - greppable, and
  the layer of a dependency is visible at the point of use.
* Bad, because more directories means more navigation for a small change.
* Bad, because co-located headers give no physical public/private boundary. Mitigated by the
  convention that anything under a `detail/` directory is private, which the layering lint checks.
* Neutral, because this layout is uncommon in Switch homebrew. That is a consequence of most
  Switch homebrew having no tests.

## Pros and Cons of the Options

### Layered, co-located headers

* Good, because one module is one directory: `git mv` moves both files, and the two cannot drift
  apart the way `list_extra_tab.hpp` and `list_extras_tab.cpp` did.
* Good, because a single `target_include_directories(... PUBLIC src)` removes `-I` sprawl. The
  predecessor needed four `INCLUDES` entries, one of which had a leading slash and was simply
  broken (`Makefile:22`).
* Bad, because there is no physical boundary between public and private headers.

### Layered, mirrored include/ and src/

* Good, because public API is physically separated from implementation.
* Bad, because every rename is two operations in two trees, and the predecessor demonstrates
  exactly how that drifts.
* Bad, because adding a module means creating two directories.

### Feature-based

* Good, because everything about one feature sits together, which suits feature work.
* Bad, because there is no natural place for the shared primitives (semver, hashing, paths), so
  they collect in a `common/` directory - which is how `utils.cpp` was born.
* Bad, because the pure/impure boundary that makes host testing possible cuts *across* features,
  so this layout does not produce it.

### Flat tree, just split the big files

* Good, because it is the smallest possible change.
* Bad, because it fixes the symptom. Nothing prevents the next `utils.cpp`, and nothing makes any
  of it testable - the dependency on libnx is what blocks testing, not the file size.

## More Information

* [`docs/architecture/layering.md`](../architecture/layering.md) - the rules in detail
* [`docs/architecture/source-tree.md`](../architecture/source-tree.md) - the predecessor-to-new mapping
* [`tools/lint/check_layering.sh`](../../tools/lint/check_layering.sh)

Revisit if the layer count proves to be more ceremony than value - most plausibly by merging
`platform` into `infra` if the distinction stops earning itself.
