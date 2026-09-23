# Error codes

> **Status: skeleton.** The catalogue is filled in as `nsx::core::ErrorCode` grows. The scheme
> and the rules below are fixed.

Every user-visible failure has a stable code. A bug report quoting `NSX-UPD-0203` is
actionable; one saying "the update did not work" is not.

## Format

```
NSX-<AREA>-<NNNN>
```

| Area | Meaning |
|---|---|
| `NET` | Network transport: DNS, connection, TLS, timeouts |
| `UPD` | The update pipeline: manifest, download, verification, staging |
| `FWD` | The forwarder: swap, rollback, recovery |
| `FS` | Filesystem: space, permissions, corruption |
| `PKG` | Archive handling: extraction, traversal, integrity |
| `SYS` | Platform and libnx services |
| `CFG` | Settings and configuration |

Numbers are allocated sequentially within an area and are **never reused**, so an old code in an
old bug report keeps meaning what it meant.

## Rules

1. **Every error message shows its code.** The UI shows the code plus a plain-language
   explanation; `log.txt` gets the code plus the technical detail.
2. **An error says what to do next**, or admits there is nothing to do. "Download failed" is not
   an error message; "Download failed - check your connection and try again (NSX-NET-0104)" is.
3. **A code is stable once released.** Change the wording freely; never the meaning.
4. **Nothing security-relevant is silently swallowed.** A hash mismatch or a TLS failure is
   always surfaced. The predecessor swallowed a 404 into an empty string
   (`download.cpp:493-500`), which is exactly how a self-updater can be broken for years without
   anyone noticing.

## Reserved examples

Illustrative, pending implementation:

| Code | Condition | What the user is told |
|---|---|---|
| `NSX-NET-0101` | No network connection | Connect to Wi-Fi and try again |
| `NSX-NET-0110` | TLS verification failed, certificate not yet valid | **Set your console's clock** - the most common cause on a Switch |
| `NSX-UPD-0201` | Manifest unreadable | Update check unavailable; download manually, with the link |
| `NSX-UPD-0202` | `schema_version` newer than this client | This version is too old to update itself; reinstall from the zip |
| `NSX-UPD-0203` | SHA-256 mismatch after download | The download was corrupted or tampered with; retried once, then stopped |
| `NSX-UPD-0210` | Installed version below `min_supported` | In-app update not possible; reinstall from the zip |
| `NSX-FWD-0301` | Swap failed, rolled back | The update failed and your previous version was restored |
| `NSX-FS-0401` | Not enough free space | Free N MB and try again |
| `NSX-PKG-0501` | Archive entry escapes its directory | Refused - the download is not trustworthy |
