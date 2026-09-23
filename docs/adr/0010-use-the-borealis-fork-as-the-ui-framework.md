---
status: "accepted"
date: 2026-09-23
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["ui", "deps"]
---

# 0010. Use the Borealis fork as the UI framework

## Context and Problem Statement

NSX Manager needs a UI that looks native on the Switch, is driven by a gamepad, supports touch,
and renders efficiently within a homebrew memory budget. Writing one from scratch is not
realistic for this project.

The predecessor used [HamletDuFromage's Borealis fork](https://github.com/HamletDuFromage/borealis)
via `library/borealis.mk`. That choice was inherited from `aio-switch-updater` rather than made,
so it deserves an explicit decision - particularly because the fork, not upstream, is what is
actually used.

## Decision Drivers

* Native Switch look and feel, including the horizon-style sidebar and list idioms.
* Gamepad and touch input handled by the framework, not by us.
* Proven on this exact platform under devkitA64.
* We must be able to build it with CMake (ADR-0002).

## Considered Options

* Borealis, HamletDuFromage fork (Borealis 0.x)
* Borealis upstream, `natinusala/borealis`
* Borealis 2.x
* Dear ImGui on top of libnx and EGL
* A bespoke NanoVG UI layer

## Decision Outcome

Chosen option: **the HamletDuFromage Borealis 0.x fork**, pinned by SHA under `third_party/`
(ADR-0009) and isolated behind the `nsx::ui` layer.

Two constraints follow and are enforced:

1. **Borealis types appear only under `src/nsx/ui/`.** No other layer may include a Borealis
   header - checked by `tools/lint/check_layering.sh`. If the framework is ever replaced, the
   change is confined to one layer.
2. **We do not inject into the `brls::` namespace.** The predecessor opened
   `namespace brls { ... }` in its own `sound_manager.cpp:6` and declared `brls::SearchListItem`
   in its own header - first-party classes living inside a third-party namespace, so nobody could
   tell which code was whose. Our widgets are `nsx::ui::*` and compose Borealis types rather than
   extending its namespace.

Borealis is GPL-3.0, which is what fixes the application's licence (ADR-0011).

### Consequences

* Good, because the Switch-native look is free, and gamepad plus touch input are solved.
* Good, because it is proven: the predecessor shipped a ten-tab application on it.
* Good, because the fork carries Switch-specific fixes not merged upstream, which is why the
  ecosystem uses it.
* Bad, because we depend on a **fork** whose maintenance is not guaranteed. Mitigated by pinning
  a SHA - upstream going quiet does not break our build - and by confining it to one layer.
* Bad, because we must maintain the CMake wrapper reproducing `borealis.mk`. This is the
  acknowledged cost of ADR-0002.
* Bad, because it forces the application to GPL-3.0, which is what creates the RCM payload
  licensing problem. That is resolved by isolation, not by changing this choice.
* Neutral, because Borealis 0.x is not actively developed upstream. For a stable, pinned UI
  toolkit that is acceptable.

## Pros and Cons of the Options

### Borealis 0.x, HamletDuFromage fork

* Good, because it has the Switch fixes the ecosystem actually relies on.
* Good, because it is a known quantity for this specific application.
* Bad, because forks bit-rot, and we inherit that risk.

### Borealis upstream

* Good, because it is the canonical source with clearer provenance.
* Bad, because it lacks the fork's Switch-specific fixes, which would have to be re-applied and
  then maintained locally - a fork by another name, with more work.

### Borealis 2.x

* Good, because it is actively maintained and cross-platform, which would allow a desktop preview
  harness for UI work.
* Bad, because the API differs substantially, so nothing from the ecosystem transfers directly.
* Bad, because it is a large unknown to take on at the same time as every other foundational
  decision. Deferred deliberately - see open question 6 in the project scope.

### Dear ImGui

* Good, because it is excellent, well documented and easy to iterate with.
* Bad, because it is an immediate-mode debug UI aesthetic; it would not look like a Switch
  application, and gamepad navigation would need substantial work.

### A bespoke NanoVG layer

* Good, because total control and no dependency.
* Bad, because it means writing focus management, scrolling, touch handling, theming and i18n
  ourselves. The predecessor's hand-rolled NanoVG screens are the least maintainable part of that
  codebase - `about_tab.cpp` is 711 lines of draw calls with inline magic numbers, and its theme
  manager was defined but never consulted by the drawing code.

## More Information

* [Borealis](https://github.com/natinusala/borealis) and the
  [fork](https://github.com/HamletDuFromage/borealis)
* [`third_party/CMakeLists.txt`](../../third_party/CMakeLists.txt) - the wrapper
* [ADR-0012](0012-use-borealis-i18n-with-en-us-as-the-source-of-truth.md) - its i18n system

Revisit after 1.0.0, when a Borealis 2.x migration could be evaluated on its own merits rather
than alongside everything else.
