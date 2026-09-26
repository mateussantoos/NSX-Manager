---
status: "accepted"
date: 2026-09-26
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["domain", "motd", "manifest"]
---

# 0022. Dynamic MOTD community bulletin protocol

## Context and Problem Statement

When breaking firmware updates occur in the Nintendo Switch ecosystem (e.g., major Horizon OS releases causing bootloops
under incompatible Atmosphere versions), users frequently encounter bricking risks before a stable CFW release is deployed.
NSX Manager requires a non-intrusive, real-time warning mechanism to communicate emergency notices, compatibility advisories,
and maintenance bulletins without mandating an immediate binary release.

## Decision Drivers

* Out-of-band communication using the existing unauthenticated update manifest (`update.json`).
* Version gating: ability to target bulletins to specific application version ranges (`min_app_version`).
* User experience: non-blocking, non-modal banner at the top of the dashboard that does not impede normal usage.
* Local caching: once fetched, bulletins remain visible across offline sessions until dismissed or updated.

## Considered Options

* Dedicated MOTD API endpoint requiring additional HTTP calls.
* Embedding optional `motd` payload within the existing `update.json` manifest.

## Decision Outcome

Chosen option: **embedding optional `motd` payload within `update.json` and managing it via `MotdService` in `domain/motd`**.

The manifest format is extended with an optional bulletin schema:
```json
"motd": {
  "id": "ams-24-warning",
  "severity": "warning",
  "title": "Firmware Warning",
  "message": "Horizon OS 24.0.0 is not yet compatible with current Atmosphere. Do not update via Daybreak.",
  "min_app_version": "0.2.0"
}
```
`MotdService` validates the payload, checks SemVer compatibility against the running application version,
and maintains dismissal tracking. `DashboardSummaryView` renders an alert banner above the dashboard summary cards.

### Consequences

* Positive: Zero additional network overhead; delivered in the standard manifest check.
* Positive: Protects users from breaking ecosystem updates with prominent warnings.
* Positive: Persistent dismissal tracking prevents banner fatigue.
