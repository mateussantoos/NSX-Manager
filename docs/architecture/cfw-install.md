# Installing a CFW pack

How a package goes from a catalogue entry to files on the card, and what happens at every point
it can stop. Decided in
[ADR-0018](../adr/0018-install-content-by-displacing-rather-than-overwriting.md).

This is the second most security-sensitive path in the application, after
[the update pipeline](update-pipeline.md). It writes thousands of files over a working
installation.

## The sequence

```
 user picks an item from the catalogue
     |
 marker present?  --yes--> refuse: an interrupted install must be resolved first
     |no
 clear this item's leftovers (staging tree, archive, .part, backup)
     |
 pre-flight: free space >= size * 3.5        (size from the catalogue - no extra request)
     |                                         refused BEFORE the first byte
     v
 download -> staging/<id>.zip
     |  SHA-256 verified by the gateway; a .part is never promoted
     v
 extract -> staging/<id>/
     |  every entry through ExtractionPolicy; one bad name condemns the archive
     |  NOTHING OUTSIDE staging/ HAS CHANGED UP TO HERE
     v
 plan: which staged files will actually be written
     |  preserved files are excluded here, not skipped later
     v
 write staging/merge.files   (the plan)
 write staging/merge.json    (the marker)
     |
     v
 merge, one file at a time:
     |
     +-- destination occupied?  --yes--> move it to backup/<relative>   [displace]
     |                                        |failure -> roll back
     +-- move staging/<relative> -> destination
     |                                        |failure -> roll back
     +-- cancelled? -> roll back
     v
 commit: delete backup, delete staging, delete archive, delete plan, delete marker
     v
 installed
```

## What each directory is for

| Path | Holds | Lifetime |
|---|---|---|
| `staging/cfw/<id>.zip` | The downloaded, verified archive | Until commit or rollback |
| `staging/cfw/<id>/` | The extracted tree, a faithful copy of the package | Until commit or rollback |
| `staging/cfw/<id>.backup/` | Exactly the files the merge displaced, mirroring the destination | Until commit or rollback |
| `staging/cfw/merge.files` | Every path this merge intends to write, one per line | Until commit or rollback |
| `staging/cfw/merge.json` | The marker: item, and the three directories above | Written first, deleted last |

The marker is written **before** the first move and deleted **after** the last cleanup. A marker
with no merge behind it costs a cleanup; a merge with no marker cannot be undone at all.

## Recovery

`recoverInterruptedMerge()` runs at startup, before anything else touches the card.

| Marker | Backup | Staging | Action |
|---|---|---|---|
| absent | — | — | nothing was in flight |
| present, unreadable | — | — | **Unrecoverable.** Something was in flight and we cannot tell what; guessing at directories to delete from is the one thing not to do |
| present | has files | — | **Roll back**: restore, then remove what was created |
| present | empty | has files | Clean up: the merge stopped before it displaced anything, so the destination is untouched |
| present | empty | empty | Clean up: nothing of it remains |

### How a rollback decides what to remove

Restoring the backup is the easy half. The hard half is the files the merge **created**, which
have nothing to restore over them.

The backup listing is captured **before** any restore. Restoring moves each file back out of the
backup, so asking afterwards whether a file "had a backup" answers no for everything just
restored — and the removal pass would then delete it a second time.

Then, for each planned file with no backup:

- **still in the staging tree** → the merge never reached it. The merge *moves* files out, so a
  staged copy still present means this one was not installed and whatever is at the destination is
  the user's original. Leave it.
- **not in the staging tree** → the merge moved it out and created it. Remove it.

A rollback that cannot put everything back reports `MergeFailed`, never `RolledBack`. Claiming a
clean undo when a file is genuinely missing would stop the user looking.

## preserve.txt

A pack ships its own `hekate_ipl.ini`, `bootloader/ini/` and `emummc.ini`. Those are the user's.
[`PreserveRules`](../../src/nsx/core/paths/preserve_rules.hpp) decides what survives; the defaults
cover exactly those three.

Preserved files are removed from the plan **before** the merge starts rather than skipped during
it. That ordering is what keeps the rollback correct — see above, and ADR-0018.

An item only honours preserve rules when the catalogue says it should. A CFW pack does, because it
overwrites the whole card. A self-contained tool does not: it needs to replace its own files, and
a rule that skipped one would leave half a package installed.

## Failure matrix

| Failure | Detected by | Result | Card afterwards |
|---|---|---|---|
| A merge already unresolved | the marker exists | `Busy` | untouched |
| Not enough space | pre-flight, before the first byte | `InsufficientSpace` | untouched |
| Download failed | transport or status | `DownloadFailed` | untouched |
| Digest or size mismatch | the gateway, before promoting `.part` | `VerifyFailed` | untouched |
| An entry would escape | `ExtractionPolicy`, during extraction to staging | `UnsafeArchive` | untouched |
| Decompression bomb | declared size vs the ceiling, before expanding | `ExtractFailed` | untouched |
| Archive expands to nothing | the staging tree is empty | `ExtractFailed` | untouched |
| A rename failed mid-merge | the return value | `RolledBack` | restored |
| Cancelled mid-merge | the progress callback | `Cancelled` | restored |
| A rename failed AND the restore failed | `rollBack` returns false | `MergeFailed` | **partially restored, and says so** |
| Power loss mid-merge | the marker at next start | `RolledBack` | restored |
| Power loss, marker unreadable | the parser | `MergeFailed` | untouched by recovery; reported |

## What this is not

**Not atomic, and it cannot be.** A FAT volume has no transaction and a pack is thousands of
files. The window between displacing a file and installing its replacement is one file wide;
losing power inside it leaves that file only in the backup, which is exactly where recovery looks.

**Not concurrent.** One install at a time. Two overlapping backups could not be unwound
independently, so a second install refuses while a marker exists.

**No resume.** A failed download restarts from zero. Same as the update pipeline, and for the same
reason: `Range:` support is not implemented yet.

## Where this lives

| Concern | Module | Tested by |
|---|---|---|
| Whether an entry may be written at all | `core/paths/traversal_guard` | `tests/unit/traversal_guard_test.cpp` |
| What the user keeps | `core/paths/preserve_rules` | `tests/unit/preserve_rules_test.cpp` |
| Per-entry decisions | `core/paths/extraction_policy` | `tests/unit/preserve_rules_test.cpp` |
| What is in the catalogue | `core/catalog/catalog` | `tests/unit/catalog_test.cpp` |
| Recovering an interrupted merge | `core/cfw/merge_marker` | `tests/unit/cfw_install_test.cpp` |
| Sequencing all of it | `domain/cfw/cfw_install_service` | `tests/unit/cfw_install_test.cpp` |
| Expanding a real zip | `infra/archive/zip_extractor` | `tests/integration/zip_extraction/` |
