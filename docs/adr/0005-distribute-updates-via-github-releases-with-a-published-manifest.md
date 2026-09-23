---
status: "accepted"
date: 2026-09-23
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["release", "update", "network"]
---

# 0005. Distribute updates via GitHub Releases with a published manifest

## Context and Problem Statement

The predecessor's update check called
`https://api.github.com/repos/{user}/{name}-updater/releases/latest` with `GITHUB_USER` set to
the literal placeholder `"NSX"` (`constants.hpp:11`). No such organisation hosts the project, so
the request 404s; the error was swallowed at `download.cpp:493-500`, `getLatestTag()` returned
`""`, and the comparison short-circuited to "no update". **The feature was inert for its entire
life and looked like it worked.**

Its content catalogue came from a third-party host,
`gamemod.com.br/nx-links/nx-links-v421.json`, with hardcoded fallbacks to `CostelaCNX/CNX` and
`CostelaCNX/NX-Firmwares`, plus a literal `"AMSNX"` string blacklist repeated in four places to
route around an organisation that had gone dead.

And it hit GitHub API rate limits: commit `7b3cdf6` is "fix github releases 403". Unauthenticated
REST is 60 requests/hour **per IP**, and the app made two calls on every launch
(`main_frame.cpp:50` and `:127`).

## Decision Drivers

* Updates must actually reach users, and the mechanism must be verifiable from outside the app.
* No dependency on infrastructure we do not control.
* Must work on shared and carrier-NAT'd networks, where a per-IP budget is consumed by strangers.
* Adding a source must not require shipping a new binary.

## Considered Options

* GitHub Releases plus a published `update.json` asset, fetched via `releases/latest/download/`
* GitHub REST API with ETag caching
* A self-hosted update server
* A manifest on a raw git branch

## Decision Outcome

Chosen option: **GitHub Releases, with an `update.json` manifest published as a release asset
under a stable filename**, fetched from

```
https://github.com/mateussantoos/nsx-manager/releases/latest/download/update.json
```

That URL is a `github.com` **web** redirect to the release CDN. It is not `api.github.com` and is
not subject to the REST budget. This is the entire reason the manifest exists as a stable-named
asset rather than being derived from an API response.

Layered on top:

1. Local cache at `/config/nsx-manager/cache/update.json`, TTL six hours; a manual check bypasses
   the TTL. `If-None-Match` is still sent - it saves bandwidth - but nothing *depends* on it.
2. Exponential backoff with jitter on `403`, `429` and `5xx`, base 60 s, cap 6 h, **persisted
   across launches**. The predecessor re-hammered on every start.
3. Mirror fallback at the `release-metadata` orphan branch, used only when `github.com` itself is
   unreachable. Also not the REST API.
4. `api.github.com` is used for exactly one optional feature - a release-history screen - behind
   an explicit user action, cached 24 h, degrading to "unavailable" rather than erroring. Never
   on the startup path.

The content catalogue moves to a manifest published under `mateussantoos`. Third-party sources
become entries *inside* it, so a dead upstream is a manifest edit, not a new release.

### Consequences

* Good, because the endpoint is verifiable by anyone with a browser - the class of failure where
  an update system silently 404s for years becomes immediately visible.
* Good, because GitHub hosts the assets: no server to run, pay for, or have go down.
* Good, because `mandatory`, `min_supported` and `channel` become per-release publisher decisions
  rather than compile-time behaviour. In particular, the predecessor hid all ten tabs whenever any
  newer tag existed (`main_frame.cpp:94-133`); that is now an explicit `mandatory: true`.
* Good, because the manifest is generated and **validated in CI by the same validator that
  defines the client contract**, so an unreadable manifest cannot be published.
* Bad, because the project depends on GitHub's availability. Accepted: the source already lives
  there, and the mirror branch covers partial outages.
* Bad, because `update.json` must be regenerated for every release. Automated in `release.yml`.
* Neutral, because `schema_version` must be honoured forever once published. That is the price of
  a versioned contract, and the client refusing anything newer than it understands is the escape
  hatch.

## Pros and Cons of the Options

### Releases plus a published manifest

* Good, because the happy path never touches a rate-limited API.
* Good, because one fetch returns everything needed: version, sizes, hashes, URLs, policy flags.
* Good, because sizes come with the manifest, so the free-space pre-flight needs no extra
  request - the predecessor issued a separate `HEAD` for that (`download.cpp:134-152`).
* Bad, because it is one more artefact to generate and keep correct.

### GitHub REST API with ETag caching

* Good, because it needs no extra published artefact.
* Bad, because it is rate-limited to 60/hour/IP and **an unauthenticated 304 still decrements the
  budget**, so ETags do not actually protect it. On shared IPs the budget is spent by other
  people. This is the predecessor's documented 403 failure, and the reason this option loses.
* Bad, because embedding a token to raise the limit would ship a credential in a public binary.

### A self-hosted update server

* Good, because complete control over policy, staged rollout and analytics.
* Bad, because it must be paid for, kept alive and secured; the predecessor's third-party host is
  a live example of that dependency becoming a liability.

### A manifest on a raw git branch

* Good, because it is simple and updatable without a release.
* Bad, because it decouples the manifest from the artefacts it describes - the two can disagree.
* Good enough as a *fallback*, which is exactly the role it is given.

## More Information

* [`docs/architecture/update-pipeline.md`](../architecture/update-pipeline.md)
* [`docs/reference/update-manifest.md`](../reference/update-manifest.md) and
  [`update.schema.json`](../reference/update.schema.json)
* [GitHub REST rate limits](https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api)
* [ADR-0008](0008-ship-a-bare-nro-for-in-app-updates-and-a-zip-for-first-install.md) - the asset contract

Revisit if GitHub changes how the latest-release download path behaves, or if release history
becomes important enough on the startup path to justify authentication.
