# The update pipeline

End to end, plus every failure and what happens. This is the most security-sensitive path in the
application: it downloads executable code and writes it to the user's SD card.

Decisions: [ADR-0005](../adr/0005-distribute-updates-via-github-releases-with-a-published-manifest.md),
[ADR-0006](../adr/0006-verify-tls-with-an-embedded-ca-bundle-and-mandate-sha-256.md),
[ADR-0008](../adr/0008-ship-a-bare-nro-for-in-app-updates-and-a-zip-for-first-install.md).

## The sequence

```
 startup (never blocks on the network)
     |
     +-- cache fresh (< 6 h)? --yes--> use it
     |          |no
     |          v
     |   GET github.com/.../releases/latest/download/update.json
     |     TLS verified against the embedded CA bundle
     |     If-None-Match sent; 304 is fine
     |          |
     |     failure? --> persisted backoff, serve cache, show "last checked N ago"
     |          |
     |     unreachable? --> mirror: raw.githubusercontent.com/.../release-metadata/update.json
     v
 parse manifest  (schema_version > 1 -> refuse, tell the user to update manually)
     |
 decideUpdate(installed, manifest)
     |
     +-- UpToDate        -> nothing
     +-- UnsupportedPath -> "reinstall from the zip", with the link
     +-- Optional        -> offer
     +-- Mandatory       -> block the menu until updated
     |
 user accepts
     |
 pre-flight: free space >= size * 1.2   (size comes from the manifest - no extra request)
     |
 download asset kind=app-nro  ->  /config/nsx-manager/staging/nsx-manager.nro.part
     |                              SHA-256 computed DURING the write, one pass
     |
 size matches AND sha256 matches (constant time)?
     |                    |no -> delete .part; retry once; then hard-fail with
     |                    |       expected vs actual. NEVER fall back to unverified.
     |yes
 rename .part -> staging/nsx-manager.nro
     |
 write staging/handoff.json (attempts = 0)
     |
 envSetNextLoad(forwarder, "--handoff=/config/nsx-manager/staging/handoff.json")
 quit
     |
 forwarder: re-verify hash -> backup -> swap -> verify -> chainload
     |                                                        (see self-update-forwarder.md)
     v
 new version running
```

## Why the manifest, and not the API

Unauthenticated GitHub REST allows **60 requests per hour per IP**, and an `If-None-Match`
request returning `304` **still decrements** that budget. ETag caching therefore does not solve
the problem; on a shared or carrier-NAT'd network, strangers consume the allowance. The
predecessor called the API twice on every launch and hit 403s - commit `7b3cdf6` is literally
"fix github releases 403".

`github.com/.../releases/latest/download/<asset>` is a **web** redirect to the release CDN. It is
not the REST API and is not subject to that budget. That is the whole reason `update.json` exists
as a stable-named release asset.

`api.github.com` is used for exactly one optional screen - release history - behind a user
action, cached 24 hours, degrading to "unavailable". Never on the startup path.

## Trust

| Step | Guarantee |
|---|---|
| Transport | TLS 1.2+, peer **and** host verified against a pinned Mozilla CA bundle embedded as a compile-time blob. `https` only, including after redirects. |
| Manifest | Parsed strictly. A future `schema_version` is refused, not guessed at. |
| Asset URL | Must be `https://github.com/...`. A plaintext or off-host URL is rejected. |
| Asset name | Must be a bare filename - no `/`, `\`, `..` or leading dot. |
| Payload | SHA-256 verified against the manifest before the file is renamed out of `.part`. |
| Staged binary | Hash **re-verified by the forwarder** before the swap. |

`tools/lint/forbid_insecure_curl.sh` fails CI on any attempt to disable verification or use a
plaintext URL, so the predecessor's behaviour cannot return.

## Failure matrix

| Failure | Detected by | Recovery |
|---|---|---|
| No network | `nifm` reports down | Skip silently; the cache serves the UI |
| DNS or connection failure | curl error | Persisted backoff; serve cache |
| TLS verification failed | `CURLE_PEER_FAILED_VERIFICATION` | **Never downgrade.** If the cause is certificate dates, show "set your console clock" |
| HTTP 403 / 429 / 5xx | status code | Exponential backoff with jitter, base 60 s, cap 6 h, persisted across launches |
| `github.com` unreachable | connection failure | Try the mirror branch |
| Manifest is not valid JSON | parser | Serve cache; "update check unavailable" |
| `schema_version` too new | parser | Refuse; "this version is too old to update itself" |
| No `app-nro` asset | manifest validation | Treat as unreadable; the manifest is malformed |
| Not enough space | pre-flight `size * 1.2` | Abort **before** the first byte, naming how much is needed |
| Truncated download | byte count vs `size`, or `CURLE_PARTIAL_FILE` | Delete `.part`; resume with HTTP `Range:`, up to 3 attempts |
| Hash mismatch | SHA-256 compare | Delete `.part`; retry **once**; then hard-fail showing expected vs actual. Never fall back. |
| Forwarder binary corrupt | hash against the romfs copy | Re-copy from romfs; if that fails, **abort before `envSetNextLoad`** - never chainload an unverified binary |
| Power loss mid-swap | `handoff.json` present at next boot | Restore the backup; see [`self-update-forwarder.md`](self-update-forwarder.md) |
| Repeated swap failure | `attempts >= max_attempts` | Roll back, delete staging, report with a pointer to the log |

Every row is a decision made in advance. The predecessor had none of them: a 404 became an empty
string, and an empty string became "you are up to date".

## Content updates

CFW packs, firmware, cheats, ports and translations follow the same trust rules - TLS, hash
before use - but they **are** archives, so extraction applies. Extraction is traversal-guarded
(`core/paths/traversal_guard`) and staged rather than written to `/`.

The application's own update deliberately shares none of that code: it downloads one bare `.nro`
and renames it. A bug in the archive path therefore cannot affect the binary that would have to
be run to fix it.
