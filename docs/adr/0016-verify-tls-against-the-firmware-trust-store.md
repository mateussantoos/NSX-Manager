---
status: "accepted"
date: 2026-09-23
deciders: ["@mateussantoos"]
supersedes: ["ADR-0006"]
superseded-by: []
tags: ["security", "network"]
---

# 0016. Verify TLS against the firmware trust store

## Context and Problem Statement

[ADR-0006](0006-verify-tls-with-an-embedded-ca-bundle-and-mandate-sha-256.md) decided to embed a
Mozilla CA bundle and hand it to libcurl via `CURLOPT_CAINFO_BLOB`. Implementing it showed **two
of its stated facts are wrong**:

| ADR-0006 claimed | Actually true |
|---|---|
| "devkitPro's `switch-curl` is built against **mbedTLS**" | It is built against **libnx's `ssl` service** - the firmware's own TLS stack. `libcurl.a` references `sslConnectionDoHandshake`, `sslConnectionSetHostName`, `sslConnectionGetVerifyCertError`; it references no `mbedtls_*` symbol at all. |
| "`CURLOPT_CAINFO_BLOB` is supported since curl 7.81.0; devkitPro ships far newer" | devkitPro ships **curl 7.69.1** (`LIBCURL_VERSION_NUM 0x074501`). `CURLOPT_CAINFO_BLOB` arrived in **7.77.0**, so the option does not exist. The code did not compile. |

The mbedTLS claim came from the predecessor's link line (`Makefile:58` lists `-lmbedtls
-lmbedx509 -lmbedcrypto`). Those portlibs are installed, and the predecessor linked them - but
curl is not built against them. The inference was reasonable and it was wrong.

ADR-0006 also **rejected** the libnx `ssl` route, citing a 71-object `ServerPki` quota. That
limit applies to importing certificates *into* a context. It does not apply here, because with
this backend we import nothing - the firmware supplies the trust store.

## Decision Drivers

* Verification must be on, unconditionally. That part of ADR-0006 stands and is not reopened.
* The trust anchor must be real, not aspirational - an option that does not compile protects
  nobody.
* No code path may weaken verification, including any fallback.
* A future devkitPro curl should improve things automatically, not require anyone to remember.

## Considered Options

* Verify against the firmware trust store, keeping the embedded bundle version-gated
* Write the embedded bundle to a file and point `CURLOPT_CAINFO` at it
* Vendor and link mbedTLS ourselves, building curl against it
* Pin an older behaviour and revisit when devkitPro updates curl

## Decision Outcome

Chosen option: **verify against the firmware trust store**, with the embedded bundle retained
behind a version gate.

```cpp
curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

#if defined(CURLOPT_CAINFO_BLOB) && LIBCURL_VERSION_NUM >= 0x074D00
    curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &caBlob);
#endif
```

The `#if` is a **version gate, not a fallback**. Verification is enabled identically on both
sides of it; the only difference is which store answers. There is no branch in which a failure
to supply certificates turns verification off - which was ADR-0006's core reasoning and remains
correct.

Everything else in ADR-0006 stands unchanged: mandatory SHA-256 before use, `.part` files that
are never executed or extracted, constant-time digest comparison, the clock-skew message, and
`tools/lint/forbid_insecure_curl.sh`.

### Consequences

* Good, because the trust store is the one the console itself uses and Nintendo maintains it
  through system updates - no staleness for us to manage, and `deps.yml`'s weekly CA bump stops
  being load-bearing.
* Good, because it is one option fewer to get wrong, and it works on the toolchain we actually
  have.
* Good, because a newer devkitPro curl switches to the pinned bundle with no code change.
* Bad, because the trust decision now depends on firmware we do not control. Accepted: this
  application already runs under custom firmware on that same device, and the alternative was an
  option that does not exist.
* Bad, because a ~200 kB bundle is compiled in and currently unused by curl. It is still used by
  the startup self-check, and deleting it would have to be undone the moment curl updates.
* Neutral, because clock skew still surfaces as a verification failure. The detection in
  `classify()` reads `CURLINFO_SSL_VERIFYRESULT`, which this backend populates, so the
  "set your console clock" message still works.

## Pros and Cons of the Options

### Firmware trust store, bundle version-gated

* Good, because it compiles and runs today.
* Good, because the store is maintained without us.
* Bad, because we do not control the anchor.

### Write the bundle to a file, use `CURLOPT_CAINFO`

* Good, because it keeps our own pinned anchor with an option 7.69.1 does have.
* Bad, because with this backend `CURLOPT_CAINFO` is not honoured - the firmware service does
  the verification and takes no PEM file. It would be a silent no-op that *looks* like a control.
* Bad, because writing certificates to the SD card puts the trust anchor somewhere any homebrew
  can rewrite, which ADR-0006 rejected for good reason and which still holds.

### Vendor mbedTLS and build curl ourselves

* Good, because it would restore full control over the anchor and enable `CAINFO_BLOB`.
* Bad, because it means maintaining a curl build for a cross-toolchain, indefinitely, to replace
  a working trust store with an equivalent one.
* Bad, because it is a large, permanent cost paid for a property we have not shown we need.

### Pin and revisit

* Good, because it avoids a decision.
* Bad, because "it does not compile" is not a state to sit in, and the version gate already gives
  us the revisit for free.

## More Information

* Determined by inspecting `libcurl.a` in the build container:
  `aarch64-none-elf-nm --undefined-only libcurl.a | grep -i ssl`
* [curl 7.77.0 release notes](https://curl.se/changes.html) - `CURLOPT_CAINFO_BLOB`
* [`src/nsx/infra/http/curl_client.cpp`](../../src/nsx/infra/http/curl_client.cpp)
* [`docs/architecture/threat-model.md`](../architecture/threat-model.md)

Revisit when devkitPro ships curl 7.77.0 or newer: the gate opens by itself, and this ADR should
then be re-examined to decide whether the pinned bundle is genuinely preferable to the firmware
store, rather than defaulting to it because it is there.
