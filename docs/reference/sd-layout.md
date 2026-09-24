# SD card layout

Every path NSX Manager reads or writes. If a path is not listed here, the application does not
touch it - and a change that adds one updates this page in the same pull request.

## Paths NSX Manager owns

### `/switch/nsx-manager/`

| Path | Written by | Purpose |
|---|---|---|
| `nsx-manager.nro` | the installer zip, then the forwarder | The application. Launched from hbmenu. |
| `nsx-forwarder.nro` | first run, copied from romfs | The repair entry point, shown in hbmenu as **NSX Manager (Repair)**. Always present so there is a launchable binary even if the application one is lost. |

### `/config/nsx-manager/`

Created at first run. **Never shipped inside the installer zip**, so reinstalling can never
overwrite user settings.

| Path | Purpose |
|---|---|
| `settings.json` | Theme, language, update channel, first-run flag |
| `log.txt` | Append-only log, rotated at 1 MB. Attach this to a bug report. |
| `cache/update.json` | Last fetched manifest, byte for byte as the server sent it |
| `cache/update.meta.json` | ETag, fetch timestamp and the **digest of the document beside it** |
| `cache/backoff.json` | Persisted backoff state - survives a relaunch, so a failing endpoint is not re-hammered on every start |
| `staging/` | Downloads in progress and the update handoff |
| `staging/*.part` | An unverified download. **Never executed, never extracted, never chainloaded.** Deleted at startup if stale. |
| `staging/handoff.json` | The self-update handoff - see [`handoff-format.md`](handoff-format.md) |
| `staging/nsx-manager.nro` | The verified, staged new binary |
| `staging/nsx-manager.nro.bak` | The previous binary, kept until the swap is confirmed |
| `forwarder/nsx-forwarder.nro` | The forwarder copy that is actually chainloaded |
| `preserve.txt` | User-supplied list of files to preserve across a CFW pack update |

The cache is two files, and the metadata is written **after** the document. That ordering is what
makes a power cut between them detectable: the metadata then describes a document that is no
longer there, the digests disagree, and the cache is treated as absent rather than half-trusted.
A single combined file could not express that, and three loose files could disagree in more ways
than one.


## Paths NSX Manager reads but does not own

| Path | Why |
|---|---|
| `/bootloader/hekate_ipl.ini` | Detect the installed CFW pack version |
| `/atmosphere/` | Detect Atmosphere, locate `contents/` |
| `/emuMMC/emummc.ini` | Detect an emuMMC setup |
| `/switch/daybreak.nro` | Hand off firmware installation |

## Paths NSX Manager writes outside its own directories

These are the ones that matter for trust. Each is a deliberate, user-initiated action.

| Path | When | Guard |
|---|---|---|
| `/atmosphere/contents/` | installing cheats, translations or mods | Traversal-guarded extraction; hash verified first |
| `/firmware/` | downloading official firmware | Same |
| `/payload.bin` | preparing reboot-to-payload | Written from `romfs:/nsx_rcm.bin`, which ships inside the NRO |
| `/atmosphere/reboot_payload.bin` | same | Same |
| CFW pack destinations | installing a pack | Staged, then merged - never extracted blindly to `/` |

**What we do not do:** the predecessor called `chdir("/")` and extracted arbitrary downloaded
archives directly over the SD card root (`utils.cpp:173`). Nothing in NSX Manager extracts to `/`.

## Reserved names

| Suffix | Meaning |
|---|---|
| `.part` | Unverified. Never trusted for anything. |
| `.bak` | Rollback copy. Deleted only after the replacement is verified in place. |
| `.tmp` | Mid-write. Renamed into place after `fsync`. |

## Uninstalling

Delete `/switch/nsx-manager/` and `/config/nsx-manager/`. Nothing else belongs to NSX Manager.

Content it installed - cheats, firmware, translations - belongs to those systems and is left
alone; removing it is a separate, deliberate action.
