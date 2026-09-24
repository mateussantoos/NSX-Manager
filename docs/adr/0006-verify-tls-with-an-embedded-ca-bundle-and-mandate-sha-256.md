---
status: "superseded"
date: 2026-09-23
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: ["ADR-0016"]
tags: ["security", "network"]
---

# 0006. Verify TLS with an embedded CA bundle and mandate SHA-256

## Context and Problem Statement

The predecessor disabled certificate verification on **every** HTTP request:

```cpp
curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);   // download.cpp:140, 198, 353, 472
curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);   // download.cpp:141, 199, 354, 473
```

It then extracted the downloaded archives to the SD card root - `utils.cpp:173` does
`chdir("/")` before `extract::extract(..., ROOT_PATH)` - with no signature, no hash and no
checksum. The only validation was a four-byte `PK\x03\x04` magic test (`utils.cpp:27-39`).

Any attacker on the network path could replace the CFW pack, the official firmware, or the
application itself, and the payload would be written to the SD card and executed.

The root cause is worth naming, because it explains why this was not simply carelessness:
devkitPro's `switch-curl` is built against **mbedTLS** (evidenced by the link line at
`Makefile:58`), and the mbedTLS backend has **no system trust store** to fall back on. Verification
therefore failed for every request, and it was switched off rather than supplied with a trust
anchor.

## Decision Drivers

* This software writes executable code to a user's SD card. That is the whole threat model.
* Any solution must have no "certificates missing, what now?" branch, because such a branch
  invites exactly the insecure fallback we are removing.
* The Switch RTC is frequently wrong, which breaks certificate date validation.
* It must be impossible to reintroduce the old behaviour without CI noticing.

## Considered Options

* Embed a pinned Mozilla CA bundle as a compile-time blob (`CURLOPT_CAINFO_BLOB`)
* Ship `cacert.pem` in romfs and point `CURLOPT_CAINFO` at it
* Ship `cacert.pem` on the SD card
* Use the libnx `ssl` service trust store (`sslContextImportServerPki`)

## Decision Outcome

Chosen option: **embed a pinned Mozilla CA bundle as a compile-time blob**, plus **mandatory
SHA-256 verification of every downloaded artefact before it is used**.

```cpp
curl_blob blob{ /* kCaBundlePem */ , len, CURL_BLOB_NOCOPY };
curl_easy_setopt(c, CURLOPT_CAINFO_BLOB,         &blob);
curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER,      1L);
curl_easy_setopt(c, CURLOPT_SSL_VERIFYHOST,      2L);
curl_easy_setopt(c, CURLOPT_SSLVERSION,          CURL_SSLVERSION_TLSv1_2);
curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR,       "https");
curl_easy_setopt(c, CURLOPT_REDIR_PROTOCOLS_STR, "https");
```

`CURLOPT_CAINFO_BLOB` is supported by the mbedTLS backend from curl 7.81.0; devkitPro ships far
newer. The bundle lives at `third_party/cacert/cacert.pem`, is **gitignored**, and only its
SHA-256 is committed - so the trust anchor changes as a one-line reviewable diff instead of a
250 kB blob nobody reads. `tools/cacert/fetch.sh` refuses to install a bundle whose hash does not
match.

**Clock skew** is handled explicitly: a date-related `CURLE_PEER_FAILED_VERIFICATION` surfaces as
"set your console clock". It never downgrades verification.

**SHA-256 is not optional.** A download lands as `<asset>.part`, is hashed **during** the write in
the same pass, and is compared against both `size` and `sha256` from the manifest in constant time.
Only on both matching is it renamed. A `.part` file is never executed, extracted or chainloaded,
and stale ones are deleted at startup.

**Signing is deliberately deferred.** The trust anchor today is TLS plus digests published in a
release asset served over TLS. Minisign signatures with the public key baked into romfs are the
intended follow-up; recorded here so the gap is known rather than forgotten, and documented in
`SECURITY.md` so a report saying only "releases are not signed" is a known duplicate.

### Consequences

* Good, because a network attacker can no longer substitute content, which was trivially possible
  before.
* Good, because `tools/lint/forbid_insecure_curl.sh` fails CI on `SSL_VERIFYPEER, 0`,
  `SSL_VERIFYHOST, 0`, `SSL_VERIFYSTATUS, 0` or a literal `http://` under `src/`. The regression
  is mechanically impossible to merge.
* Good, because hashing during the write costs one pass, not a re-read.
* Good, because the manifest supplies sizes, so the free-space pre-flight needs no extra request.
* Bad, because the CA bundle must be kept fresh. `deps.yml` opens a weekly pull request; a stale
  bundle eventually breaks connections to sites with newly issued roots.
* Bad, because roughly 250 kB of certificates are compiled into the binary.
* Bad, because users with a badly wrong clock now see a failure where they previously saw a
  (dangerously) successful download. This is the correct trade and the message says what to fix.
* Neutral, because we are not yet signing. Stated, tracked, and not pretended otherwise.

## Pros and Cons of the Options

### Embedded CA blob

* Good, because there is no file to be missing, so there is **no fallback branch** - which is
  precisely where insecure defaults creep back in.
* Good, because the trust anchor is reviewed as a hash diff.
* Bad, because updating it requires a rebuild and a release.

### cacert.pem in romfs

* Good, because it is updatable without touching the code, and keeps the binary smaller.
* Bad, because it depends on `MBEDTLS_FS_IO` and on romfs devoptab mount ordering.
* Bad, because it creates a "the file is missing" path, and the honest handling of that path is
  to fail closed - at which point the romfs file bought nothing over a blob.

### cacert.pem on the SD card

* Good, because a user could add a private CA.
* Bad, because it is a **user-writable trust store** on a device whose filesystem any homebrew can
  modify. This defeats the purpose entirely.

### libnx ssl service

* Good, because it would use the firmware's own trust store and ship no certificates.
* Bad, because a context is limited to **71 `ServerPki` objects** and libcurl imports one per
  certificate, so a ~120-root bundle overruns the quota on the first connection.
* Bad, because it is irrelevant to our build anyway: our curl is mbedTLS-backed, not
  libnx-ssl-backed. Recorded so nobody "optimises" into it later.

## More Information

* [`docs/architecture/threat-model.md`](../architecture/threat-model.md)
* [curl CURLOPT_CAINFO_BLOB](https://curl.se/libcurl/c/CURLOPT_CAINFO_BLOB.html)
* [Mozilla CA bundle](https://curl.se/docs/caextract.html)
* [`tools/lint/forbid_insecure_curl.sh`](../../tools/lint/forbid_insecure_curl.sh)

Revisit when minisign signing is implemented - that becomes its own ADR superseding the "deferred"
part of this one.
