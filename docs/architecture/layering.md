# Layering

The dependency rule, what it buys, and how it is enforced.

Decision: [ADR-0003](../adr/0003-use-a-layered-source-tree-with-co-located-headers.md).

## The rule

```
core  <-  platform  <-  infra  <-  domain  <-  ui  <-  app
```

| Layer | May include from | Must not touch |
|---|---|---|
| `core` | the C++ standard library, `nlohmann/json` | libnx, curl, Borealis, the filesystem, the network - **anything platform-specific** |
| `platform` | `core` | curl, Borealis, business rules |
| `infra` | `core`, `platform` | Borealis, business rules |
| `domain` | `core`, `infra` | Borealis, libnx directly |
| `ui` | `core`, `domain` | curl, libnx directly, the filesystem |
| `app` | everything | nothing - it is the composition root |

Additionally: **nothing under `src/` may include anything from `apps/rcm-payload/`**, in either
direction. That is a licensing boundary, not a style rule - see [`licensing.md`](licensing.md).

## Why each layer exists

**`core`** is the reason the whole scheme exists. It holds the logic most likely to be wrong -
version comparison, manifest parsing, path building, hashing, state machines - and it has no
platform dependency, so it compiles with a host compiler and runs under `ctest` in seconds. The
predecessor's version-comparison bug survived because the function lived in a file that included
`switch.h` and therefore could not be tested at all.

**`platform`** wraps libnx thinly. `spsm`, `nifm`, `setsys`, `fsdev`, `romfs`, `envSetNextLoad`.
No decisions, just adaptation. Keeping it thin means the libnx surface we depend on is small and
visible.

**`infra`** adapts the outside world: HTTP, the GitHub release CDN, archives, settings storage.
It implements interfaces that `core` declares.

**`domain`** holds the use-cases - "check for an update", "install a CFW pack". It orchestrates
and depends on interfaces, never on concrete implementations.

**`ui`** is Borealis views and nothing else. No network calls, no filesystem access, no business
rules. This is what makes the UI framework replaceable, and it is why Borealis types appear
nowhere else.

**`app`** wires it all together. The only place that constructs concrete types.

## Enforcement

[`tools/lint/check_layering.sh`](../../tools/lint/check_layering.sh) runs in CI. It:

1. determines each file's layer from its path;
2. extracts every `#include "nsx/<layer>/..."`;
3. fails on any include that points upward or sideways;
4. additionally fails if anything in `core/` includes `<switch.h>`, `<curl/...>` or Borealis;
5. fails on any include crossing into `apps/rcm-payload/`.

A compiler will happily let a UI file include a curl header. Without this script, "we have
layers" is aspiration. With it, a violation cannot merge.

```sh
tools/lint/check_layering.sh
```

## Working within the rule

### Inverting a dependency

`domain` needs to download something, but `domain` must not depend on curl. So `core` declares
the interface, `infra` implements it, and `app` injects it:

```cpp
// core/net/downloader.hpp          - the interface, pure
namespace nsx::core {
class Downloader {
public:
    virtual ~Downloader() = default;
    virtual Result<DownloadedFile> fetch(const Url& url, ProgressSink& sink) = 0;
};
}

// infra/http/curl_downloader.hpp   - the implementation
namespace nsx::infra {
class CurlDownloader final : public core::Downloader { /* ... */ };
}

// app/container.cpp                - the wiring
auto downloader = std::make_unique<infra::CurlDownloader>(tlsConfig);
auto selfUpdate = domain::SelfUpdateService{*downloader, *storage, *clock};
```

`domain` is now testable with a fake `Downloader`, and `core` stays pure.

### Injecting the clock

Anything time-dependent takes a `core::Clock`. A cache TTL test that sleeps for six hours is not
a test. The real implementation reads the system clock; tests advance a fake one.

### Where a new module goes

Ask what it needs:

* nothing but the standard library -> `core`
* a libnx service -> `platform`
* the network, the filesystem, or a third-party library -> `infra`
* it decides *what should happen* -> `domain`
* it draws -> `ui`

If a module seems to need two layers, it is two modules. That instinct - "this does not fit
anywhere, I will put it in utils" - is exactly how the predecessor grew an 822-line `utils.cpp`
that mixed networking, filesystem access, UI dialogs and power management.

## Common mistakes

| Mistake | Instead |
|---|---|
| `ui` calls `infra` directly to download | Go through a `domain` service |
| `core` includes `<switch.h>` for a type alias | Define the type in `core`; convert in `platform` |
| `domain` constructs a `CurlDownloader` | Take the interface; let `app` construct it |
| A helper "does not fit" so it goes in `core/util` | There is no `util`. Name the concept. |
| `infra` shows a dialog on failure | Return an error; `ui` decides how to present it |

The last one has a concrete precedent: the predecessor's network layer called
`brls::Application::crash()` from inside `download.cpp:381`, so a failed HTTP request took down
the whole application from three layers away.
