# The update handoff (`handoff.json`)

How the application tells the forwarder what to do. Written by the app, read by
`nsx-forwarder.nro`, parsed by **one shared implementation** - `nsx::core::Handoff`, linked into
both binaries.

Rationale: [ADR-0007](../adr/0007-self-update-by-staging-an-nro-and-chainloading-a-forwarder.md).

## Location

```
/config/nsx-manager/staging/handoff.json
```

**Always this absolute path.** The forwarder receives it as `--handoff=<path>` in `argv`, with
the compile-time constant `NSX_HANDOFF_PATH` as a fallback.

This is deliberate. The predecessor did:

```cpp
std::ifstream file("forwarder.conf");   // app-forwarder/source/main.cpp:28
```

a CWD-relative open that worked only because hbmenu happens to `chdir` into the NRO's directory.
When it failed there was no error path: every key became an empty string, which was then handed
to `rename("")` and `remove("")`. A relative path in a handoff is now a parse error, and
[`tests/fixtures/handoff/relative-path.json`](../../tests/fixtures/handoff/relative-path.json)
keeps it that way.

## Example

```json
{
  "schema_version": 1,
  "created_at_unix": 1790000000,
  "from_version": "0.1.0",
  "to_version": "0.2.0",
  "staged_nro": "/config/nsx-manager/staging/nsx-manager.nro",
  "staged_sha256": "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08",
  "target_nro": "/switch/nsx-manager/nsx-manager.nro",
  "backup_nro": "/config/nsx-manager/staging/nsx-manager.nro.bak",
  "forwarder_nro": "/config/nsx-manager/forwarder/nsx-forwarder.nro",
  "attempts": 0,
  "max_attempts": 3
}
```

## Fields

| Field | Type | Meaning |
|---|---|---|
| `schema_version` | integer | A forwarder that does not understand the version refuses to act and chainloads the existing target unchanged. |
| `created_at_unix` | integer | Staleness check. A handoff older than 7 days is treated as abandoned. |
| `from_version` / `to_version` | SemVer | Logged, and shown in the "update failed" message. |
| `staged_nro` | absolute path | The verified new binary. |
| `staged_sha256` | 64 hex | **Re-verified by the forwarder** before the swap - this catches corruption between download and swap, not just during it. |
| `target_nro` | absolute path | Where the binary must end up. |
| `backup_nro` | absolute path | Where the outgoing binary is kept until the swap is confirmed. |
| `forwarder_nro` | absolute path | The forwarder's own location, so it can delete itself when done. |
| `attempts` | integer | Incremented **before** each attempt. |
| `max_attempts` | integer | Normally 3. Reaching it triggers rollback instead of another try. |

**Every path must be absolute.** Any of `..`, a relative prefix, a backslash or an empty string
makes the whole handoff invalid.

## Lifecycle

```
app: download verified          -> write handoff (attempts = 0)
app: envSetNextLoad(forwarder --handoff=<abs>)  -> quit
forwarder: attempts >= max?     -> ROLLBACK
forwarder: attempts++, rewrite handoff atomically
forwarder: re-verify staged_sha256
             mismatch           -> abort, target untouched
forwarder: copy   staged -> target.new, fsync
forwarder: rename target  -> backup
forwarder: rename target.new -> target
forwarder: verify target, delete backup + staged + handoff
forwarder: envSetNextLoad(target)
```

The two renames exist because FatFs cannot overwrite an existing file. The window in which the
target is briefly absent is covered by the boot-time recovery below, and by the repair NRO always
being present.

## Recovery states

Determined by which files exist when the app or the forwarder starts:

| `handoff.json` | `target` | `backup` | Meaning | Action |
|---|---|---|---|---|
| absent | present | absent | Normal | Nothing |
| present, `attempts` 0 | present | absent | Staged, not yet attempted | Proceed |
| present, `0 < attempts < max` | present | present | Interrupted mid-swap | Retry |
| present, `attempts >= max` | either | present | Repeated failure | **Roll back**: restore `backup` to `target`, delete staging, report |
| present | **absent** | present | Power loss between the two renames | Restore `backup` to `target` |
| present | absent | absent | Worst case | Re-verify `staged`; install it if it matches, else tell the user to reinstall from the zip |
| absent | absent | - | No application | Launch **NSX Manager (Repair)** from hbmenu |

The final row is why `nsx-forwarder.nro` is both shipped in the installer zip and published as a
release asset: there must always be something launchable.

## Testing

The state machine is host-tested against
[`tests/fixtures/handoff/`](../../tests/fixtures/handoff/) - `valid.json`, `exhausted.json` and
`relative-path.json` - so every branch above is exercised without a console.
