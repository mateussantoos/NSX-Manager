---
status: "accepted"
date: 2026-09-26
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["platform", "network"]
---

# 0021. Native Horizon network telemetry integration

## Context and Problem Statement

NSX Manager downloads updates, catalogues, and firmware archives over the network. Users require immediate visibility
into active connection parameters (such as connection medium, assigned local IPv4 address, Wi-Fi SSID, and signal quality)
directly from the dashboard before initiating network-heavy operations. Furthermore, users require real-time verification
that Nintendo telemetry endpoints remain shielded (e.g. via 90DNS or Atmosphere DNS-MITM).

## Decision Drivers

* Clean isolation: `ui/` must not directly depend on `platform/` or `libnx` per ADR-0003 layering rules.
* Hardware telemetry: accurate queries of connection state, SSID, RSSI signal levels, and IP configurations via `nifm`.
* Host testability: telemetry queries must be mockable for automated CI execution without requiring a physical Nintendo Switch.

## Considered Options

* Direct `libnx` `nifm` invocations inside UI view components.
* Layered `NetworkInterfaceService` interface with `DefaultNetworkService` (`libnx`) and `MockNetworkService` (host).

## Decision Outcome

Chosen option: **layered `NetworkInterfaceService` in `src/nsx/platform/network/`**.

Platform-specific network telemetry wraps `nifm` services:
* `nifmGetInternetConnectionStatus` and `nifmGetCurrentIpAddress` for connection state and IPv4 metrics.
* `wlaninf` and `nifm` service wrappers for Wi-Fi SSID and RSSI signal bar/percentage calculations.
* A clean `ConnectionInfo` value type is exposed to the composition root (`app/main.cpp`) and projected into `SystemOverview`
  consumed by `ui/tabs/home_tab.cpp`.

### Consequences

* Positive: Layering rules (ADR-0003) remain completely intact (`ui` never references `platform`).
* Positive: Host unit test suites test telemetry mappings and mocks deterministically.
* Positive: High-resolution live network indicators rendered on the dashboard with signal bars and DNS shield status.
