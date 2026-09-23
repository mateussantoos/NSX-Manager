# Security Policy

## Supported versions

NSX Manager is pre-1.0. Only the **latest release** receives security fixes. Once 1.0.0 ships,
this table will list the supported minor lines.

| Version | Supported |
|---|---|
| latest release | yes |
| anything older | no |
| NSX Updater 4.x (predecessor) | no - unmaintained, and its update path never functioned |

## Reporting a vulnerability

**Do not open a public issue for a security problem.**

Use GitHub's private reporting:
[Report a vulnerability](https://github.com/mateussantoos/nsx-manager/security/advisories/new).

Please include the version, your console model (Erista/Mariko) and firmware, what you observed,
and a reproduction if you have one.

**Response targets:** acknowledgement within 72 hours; an assessment with a fix plan or a
rejection rationale within 14 days; a fix released within 90 days of acknowledgement, or a public
advisory explaining why not. You will be credited in the advisory unless you ask otherwise.

## What is in scope

This application downloads executable code and writes it to your SD card. The update pipeline is
the highest-value target in the project, and reports about it are the ones we most want:

- Bypassing TLS certificate verification, or making the client accept a downgraded connection.
- Bypassing SHA-256 verification, or getting unverified bytes executed or extracted.
- Path traversal in archive extraction, or in any path built from remote data.
- Making the forwarder swap in a binary whose hash was never checked.
- Leaving the installation unbootable and unrecoverable after a failed update.
- Manifest parsing flaws reachable from a hostile `update.json`.
- Any code path that writes outside `/switch/nsx-manager/`, `/config/nsx-manager/` or the paths
  documented in [`docs/reference/sd-layout.md`](docs/reference/sd-layout.md) without consent.

## What is out of scope

- The security of custom firmware itself, of Atmosphere, or of hekate. Report those upstream.
- Anything requiring physical access to an already-modified console - that is the threat model
  this software operates inside, not a vulnerability in it.
- Content hosted on third-party servers that NSX Manager links to but does not control.
- The RCM payload's own internals, which are hekate/BDK code - report those to
  [CTCaer/hekate](https://github.com/CTCaer/hekate).
- Missing code signing. This is a **known, documented gap**, tracked in
  [ADR-0006](docs/adr/0006-verify-tls-with-an-embedded-ca-bundle-and-mandate-sha-256.md) and in
  [`docs/architecture/threat-model.md`](docs/architecture/threat-model.md). The current trust
  anchor is TLS plus the SHA-256 digests published in a release asset served over TLS. A report
  that says only "releases are not signed" is a duplicate of that record.

## Our commitments

Documented in [`docs/architecture/threat-model.md`](docs/architecture/threat-model.md) and
enforced in CI:

- TLS peer **and** host verification are always on. `tools/lint/forbid_insecure_curl.sh` fails the
  build on any attempt to disable them, or on any plaintext `http://` URL under `src/`.
- Every downloaded artefact is SHA-256 verified against the manifest **before** it is executed,
  extracted, or renamed into place.
- A failed or interrupted update rolls back to the previous binary.
