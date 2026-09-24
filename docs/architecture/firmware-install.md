# Installing official firmware

NSX Manager **never installs firmware**. It downloads a set, verifies it, puts it where Daybreak
looks, and hands over. Writing to NAND is Daybreak's job and it does it properly; a second
implementation of that would be a second way to brick a console.

## The sequence

```
 user picks a firmware item
     |
 kind == firmware?              --no--> refuse
     |
 scan /firmware/                        BEFORE downloading anything
     |
     +-- anything unrecognised? --yes--> refuse, and NAME the files
     |
 /switch/daybreak.nro present?  --no--> refuse: nothing could install it
     |
 pre-flight: free space
     |
     v
 download -> staging/firmware/<id>.zip
     |  SHA-256 verified by the gateway
     |
     |  NOTHING HAS BEEN DELETED UP TO HERE
     v
 clear /firmware/                       one file at a time; a failure stops it
     |
 extract -> /firmware/                  every entry through ExtractionPolicy
     |
 does it look like firmware?    --no--> refuse
     |
     v
 envSetNextLoad("/switch/daybreak.nro", "\"/switch/daybreak.nro\" \"/firmware\"")
```

## Why this is not a CFW pack install

[The CFW install](cfw-install.md) merges: it displaces what it replaces into a backup and can put
everything back. Firmware does neither, and both differences are deliberate.

**Replaced whole, not merged.** NCAs from two firmware versions in one directory is how Daybreak
installs a system that will not boot. There is no sensible way to merge two sets, so the previous
one is removed entirely before the new one lands.

**No backup.** A firmware set is several hundred megabytes; keeping a copy to roll back to would
demand that much again, on a card that may not have it. So the ordering carries the safety
instead: **the clear happens only after the replacement is downloaded and verified.** A failed
download, a bad digest, a cancellation — none of them costs the user the firmware they already
had, because at that point nothing has been deleted.

## Clearing `/firmware/`

This is the step that can destroy something, so it has the most guards.

`/firmware/` is a **well-known path this application does not own**. Every Atmosphère guide names
it, other tools write to it, and a user may have put something there deliberately. A recursive
delete to save them one manual step is not a trade worth making.

So the directory is classified first, by
[`core/firmware/firmware_set`](../../src/nsx/core/firmware/firmware_set.hpp):

| Found | Treated as | Why |
|---|---|---|
| `0100000000000809.nca` | firmware content, ours to remove | how a set is named |
| `SOMETHING.NCA` | firmware content | FAT is case-insensitive; a set unpacked on a PC may carry it |
| `readme.txt` | **not ours** | refuse and name it |
| `.nca` | **not ours** | no stem, so not something we produced - and a dotfile is usually deliberate |
| `notes.nca.bak` | **not ours** | not an NCA |

If anything is unrecognised the install stops before the download, and the outcome carries the
offending names so the message can say *which* file rather than "something is in the way".

A removal that fails stops the clear rather than continuing. Continuing would leave exactly the
mixed-version directory this whole design exists to avoid.

## The Daybreak handoff

```
envSetNextLoad("/switch/daybreak.nro", "\"/switch/daybreak.nro\" \"/firmware\"")
```

Daybreak takes the directory to install from as `argv[1]`. This is **not guessed at** — it is the
contract the predecessor shipped working (`dialogue_page.cpp:133`).

Daybreak's presence is checked before the download, not after. Staging a set with nothing able to
install it would waste the transfer and leave the user with no firmware where they had one.

## Failure matrix

| Failure | When | Result | `/firmware/` afterwards |
|---|---|---|---|
| Item is not firmware | before anything | `NotFirmwareItem` | untouched |
| Unrecognised file present | before the download | `DirectoryNotOurs` | untouched |
| Daybreak missing | before the download | `DaybreakMissing` | untouched |
| Not enough space | before the download | `InsufficientSpace` | untouched |
| Download failed | during | `DownloadFailed` | **untouched** |
| Digest or size mismatch | during | `VerifyFailed` | **untouched** |
| Cancelled before the clear | during | `Cancelled` | **untouched** |
| A file would not delete | during the clear | `ClearFailed` | partially cleared; reported |
| An entry would escape | during extraction | `UnsafeArchive` | cleared, empty |
| No NCA in the archive | after extraction | `NotFirmwareContent` | cleared, holds whatever it was |

The four rows marked **untouched** are the property that matters, and each has a test.

## Where this lives

| Concern | Module | Tested by |
|---|---|---|
| What counts as firmware content | `core/firmware/firmware_set` | `tests/unit/firmware_install_test.cpp` |
| Sequencing, clearing, the handoff | `domain/firmware/firmware_install_service` | same |
| Entry-level safety during extraction | `core/paths/extraction_policy` | `tests/unit/preserve_rules_test.cpp` |

## Not done

* **No resume.** A failed download restarts from zero, as everywhere else.
* **`min_atmosphere` is not enforced.** The catalogue carries it and the UI should warn, but
  refusing a firmware install on a version comparison is not something to do without being certain
  the comparison is right.
* **Nothing verifies the NCAs themselves.** The archive's SHA-256 is checked; the individual NCAs
  are Daybreak's to validate, and it does.
