---
status: "accepted"
date: 2026-09-24
deciders: ["@mateussantoos"]
supersedes: []
superseded-by: []
tags: ["content", "safety", "architecture"]
---

# 0018. Install content by displacing rather than overwriting

## Context and Problem Statement

A CFW pack is thousands of files written over a working installation, and the write takes minutes.
That makes it the longest window in this application where the SD card is neither the old thing
nor the new one — and a Switch can be switched off, run out of battery, or eject its card at any
point inside it.

The predecessor did not have this problem so much as ignore it. It called `chdir("/")` and
extracted the downloaded archive straight over the card root (`utils.cpp:173`). There was no
staging, no backup, and no record that an install had begun, so an interruption left a card that
was part one pack and part another with nothing able to tell which files were which.

A FAT volume offers no transaction. Whatever we do here cannot be atomic, so the question is not
*how to make it atomic* but **what is recoverable from, and what records make that possible**.

## Decision Drivers

* An interrupted install must leave the card either fully updated or exactly as it was.
* The user's own files — boot entries, emuMMC pointer — must survive a pack that ships its own.
* Recovery must work at next boot, from state on the card, with no memory of the failed run.
* A pack is gigabytes. Anything that doubles the write, or writes per file, is too expensive.
* A partial restore must be reported as one. Silence here is worse than the original failure.

## Considered Options

* **Extract to staging, then displace-and-restore with a marker**
* Extract directly over the destination, as the predecessor did
* Copy the whole destination aside first, then extract over it
* A per-file journal, written as each file moves
* Write everything to a second directory and swap at the end

## Decision Outcome

Chosen option: **extract to a staging tree, then merge by displacing, guarded by a marker file.**

The merge moves each staged file into place. When something is already there, that file is moved
into a backup directory **first** — never overwritten — and the backup is kept until the whole
merge commits. A marker file naming the three directories is written before the first move and
removed after the last one, so its presence at startup means exactly one thing: a merge started
and did not finish.

Rejected, and why:

* **Extracting over the destination** is the predecessor's behaviour and has no recovery at all.
* **Copying the destination aside** doubles a multi-gigabyte write for files the pack will not
  touch.
* **A per-file journal** means thousands of small writes to a FAT card during the merge, and
  records information the directories already hold.
* **Swapping directories at the end** does not work for `/`: a pack merges into a live card
  alongside things it does not own.

### What recovery needs, and where it comes from

Three facts, and each has a home:

| Question | Answered by |
|---|---|
| Was a merge in flight? | the marker exists |
| What did it replace? | the backup directory mirrors the destination |
| What did it create? | the planned file list, minus what the backup holds |

The planned list is written once, before the merge, and is **not** the staging tree's contents.
Two exclusions matter and both were bugs before they were rules:

* **A preserved file is never listed.** It is in the package and in the staging tree, but the
  merge deliberately leaves the user's copy alone. A rollback removes every planned file it did
  not displace, so listing it would delete the exact file preserving it was meant to protect.
* **A file the merge never reached is left alone.** At the destination, an untouched original is
  indistinguishable from a file the merge created: both present, neither backed up. The staging
  tree settles it — the merge *moves* files out, so a staged copy still present means that file
  was never installed.

### Consequences

* Good: every failure path — I/O error, cancellation, power loss — restores, and the states are
  reachable in a host test rather than only on hardware.
* Good: the peak extra space is the backup, which holds only the files actually replaced.
* Good: nothing outside `staging/` changes until the merge, so a hostile archive or a failed
  download is found while the installation is untouched.
* Bad: the window between "displaced" and "installed" is one file wide and cannot be closed.
  Losing power inside it leaves that one file only in the backup, which is where recovery finds
  it.
* Bad: one install at a time. Two overlapping backups could not be unwound independently, so a
  second install refuses while a marker exists.
* Neutral: a rollback that cannot finish reports `MergeFailed`, not `RolledBack`. Claiming a
  clean undo when a file is genuinely missing would stop the user looking.

### The destination allow-list

A related constraint, implemented by `CfwInstallConfig::resolve`. The catalogue names a
destination **kind** — `sd-root`, `bootloader`, `atmosphere-contents` — and never a path; the
client maps it. That function is therefore the complete list of places an install can write, and
a compromised or mistaken catalogue can choose among them but cannot invent one.

### Confirmation

`tests/unit/cfw_install_test.cpp` drives the whole sequence against fakes: a failed rename part
way, a cancellation mid-merge, a power cut reconstructed from the exact on-card state one leaves,
an unreadable marker, and a restore that itself fails. Four mutations — one per rule above plus
the marker ordering — each turn the suite red.

`docker compose run --rm nsx archive` runs the extraction half against real archives built with
hostile entry names.

## More Information

* [`docs/architecture/cfw-install.md`](../architecture/cfw-install.md) — the flow in full
* [ADR-0017](0017-drive-the-update-flow-through-ports-so-it-is-host-testable.md) — the ports that
  make this testable
* [ADR-0011](0011-license-gplv3-and-isolate-the-gplv2-only-rcm-payload.md) — why content is data
  and never linked
