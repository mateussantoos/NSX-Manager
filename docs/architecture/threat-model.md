# Threat model

NSX Manager downloads executable code and writes it to a user's SD card. That single sentence is
the reason this page exists.

Policy for reporting: [`SECURITY.md`](../../SECURITY.md). Decision:
[ADR-0006](../adr/0006-verify-tls-with-an-embedded-ca-bundle-and-mandate-sha-256.md).

## What we are protecting

| Asset | Why it matters |
|---|---|
| The application binary | Replacing it gives an attacker persistent code execution under CFW |
| Downloaded content (packs, firmware, cheats) | Written to system paths; a malicious pack can brick or backdoor a console |
| The SD card outside our directories | We write to `/atmosphere/`, `/firmware/`, `/payload.bin` |
| The user's saves | Backed up and restored by this application |

## Assumptions

Stated plainly, because a threat model that pretends otherwise is theatre.

1. **The console is already modified.** The user runs custom firmware. We are not a security
   boundary against the console owner, and we do not try to be.
2. **Physical access is total.** Anyone holding the console can edit the SD card directly. We
   cannot defend against that and do not claim to.
3. **The network is hostile.** Public Wi-Fi, a compromised router, a hostile ISP. **This is the
   threat we actually defend against.**
4. **GitHub is trusted as a distribution channel.** If GitHub serves malicious release assets, we
   lose. Accepting this is what makes the model tractable.
5. **The build pipeline is trusted.** CI builds and signs off on what is published.

## Trust boundaries

```
  [ maintainer ] --commit--> [ GitHub + CI ]  <-- trusted: builds, hashes, publishes
                                   |
                            TLS 1.2+, pinned CA bundle     <== BOUNDARY 1: the network
                                   v
                          [ NSX Manager on device ]
                                   |
                        SHA-256 verified before use        <== BOUNDARY 2: the payload
                                   v
                            [ the SD card ]
                                   |
                         traversal-guarded paths           <== BOUNDARY 3: the filesystem
                                   v
                     /switch/, /config/, /atmosphere/, ...
```

### Boundary 1 - the network

**Threat:** an attacker substitutes content in transit.

**Defence:** `CURLOPT_SSL_VERIFYPEER = 1`, `CURLOPT_SSL_VERIFYHOST = 2`, TLS 1.2 minimum,
`https` only including after redirects.

**Where the trust anchor comes from.** devkitPro ships curl 7.69.1 built against **libnx's `ssl`
service** - the firmware's own TLS stack - so verification runs against the trust store the
console itself uses, maintained by Nintendo through system updates.

This corrects an earlier assumption. [ADR-0006](../adr/0006-verify-tls-with-an-embedded-ca-bundle-and-mandate-sha-256.md)
recorded that the backend was mbedTLS and that `CURLOPT_CAINFO_BLOB` was available; neither is
true on this toolchain, and the option did not compile. The reasoning and the correction are in
[ADR-0016](../adr/0016-verify-tls-against-the-firmware-trust-store.md).

A pinned Mozilla CA bundle is still compiled into the binary and is handed to curl automatically
if devkitPro ever ships 7.77.0 or newer. The `#if` guarding it is a **version gate, not a
fallback** - verification is enabled identically either way, and there is no branch in which
missing certificates turn it off.

**The predecessor had no defence here at all**: `CURLOPT_SSL_VERIFYPEER` and
`CURLOPT_SSL_VERIFYHOST` were set to `0` on every request
(`download.cpp:140-141, 198-199, 353-354, 472-473`).

**Clock skew:** the Switch RTC is often wrong, producing `CURLE_PEER_FAILED_VERIFICATION`. We
detect the date-related case and tell the user to set their clock. We never downgrade.

**Enforced by:** [`tools/lint/forbid_insecure_curl.sh`](../../tools/lint/forbid_insecure_curl.sh).

### Boundary 2 - the payload

**Threat:** bytes that arrived intact but are not what we intended - a compromised mirror, a
corrupted CDN cache, a bad SD card.

**Defence:** SHA-256 from the manifest, verified before the file is used for anything.

* Downloads land as `<asset>.part` and are hashed during the write.
* `size` **and** digest must both match; the digest is compared in constant time.
* Only then is the file renamed.
* A `.part` file is never executed, extracted or chainloaded. Stale ones are deleted at startup.
* The forwarder **re-verifies** before the swap.

**Gap - releases are not signed.** The manifest's hashes are themselves served over TLS from
GitHub, so the trust anchor is TLS plus GitHub. An attacker who can publish a release can publish
matching hashes. Minisign signatures with the public key in romfs are the intended fix; this is
documented in `SECURITY.md` so a report saying only "you do not sign releases" is a known
duplicate rather than a surprise.

### Boundary 3 - the filesystem

**Threat:** a path derived from remote data escaping its intended directory - zip-slip, or a
hostile asset name.

**Defence:**

* Asset names must be bare filenames. No `/`, `\`, `..`, or leading dot. Enforced by the schema
  **and** by the client.
* Archive entries are traversal-checked before extraction (`core/paths/traversal_guard`),
  host-tested with hostile inputs.
* **The application's own update contains no archive code.** It downloads one bare `.nro` and
  renames it, so zip-slip is not mitigated on that path - it is absent from it
  ([ADR-0008](../adr/0008-ship-a-bare-nro-for-in-app-updates-and-a-zip-for-first-install.md)).
* Nothing extracts to `/`. The predecessor did `chdir("/")` then extracted an arbitrary
  downloaded archive (`utils.cpp:173`).

## Non-threats

| Not defended against | Why |
|---|---|
| The console owner | They own the device and already run CFW |
| A malicious homebrew app already installed | It has the same filesystem access we do |
| GitHub serving malicious releases | Assumption 4; signing would reduce but not remove this |
| Nintendo detecting CFW | Not a security property of this application |
| An attacker with physical SD access | Assumption 2 |

## Open items

| Item | Status |
|---|---|
| Minisign release signatures | Proposed - open question 1 in the [project scope](../PROJECT_SCOPE.md#20-open-questions) |
| FTP server authentication | Open question 4. The predecessor's accepted **any** password and served the entire SD card, while advertising a PIN that did not exist in the code. It does not return without a decision. |
| Rotating the embedded CA bundle | `deps.yml` proposes a bump weekly; an expired bundle degrades over time |

## For reviewers

If you are auditing this project, these are where the value is:

1. `infra/http/tls_config.cpp` - is verification genuinely on, on every path?
2. `domain/selfupdate/` - can anything unverified reach a rename, an exec or an extract?
3. `core/paths/traversal_guard.cpp` - does any hostile input get through?
4. `apps/forwarder/` - can any sequence of crashes leave an unbootable installation?
5. `core/update/manifest.cpp` - what does a hostile `update.json` achieve?

Reporting: [`SECURITY.md`](../../SECURITY.md). Do not open a public issue.
