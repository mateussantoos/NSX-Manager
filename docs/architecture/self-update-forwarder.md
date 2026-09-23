# The self-update forwarder

A running `.nro` cannot overwrite its own file. The forwarder is how NSX Manager replaces itself
anyway, without ever leaving the user unable to launch anything.

Decision: [ADR-0007](../adr/0007-self-update-by-staging-an-nro-and-chainloading-a-forwarder.md).
Format: [`handoff-format.md`](../reference/handoff-format.md).

## The two binaries

| Binary | Location | Role |
|---|---|---|
| `nsx-manager.nro` | `/switch/nsx-manager/` | The application |
| `nsx-forwarder.nro` | `/switch/nsx-manager/` **and** `/config/nsx-manager/forwarder/` | Swaps the binary; also the manual repair entry point |

The copy in `/switch/nsx-manager/` appears in hbmenu as **"NSX Manager (Repair)"**. It is the
reason a failed update is never fatal: there is always something launchable. The copy under
`/config/` is the one actually chainloaded, so the swap never touches a file hbmenu is showing.

A third copy lives inside romfs, so a corrupted forwarder can be redeployed from the application.

## Handoff

The application writes `/config/nsx-manager/staging/handoff.json` and chainloads:

```
envSetNextLoad("/config/nsx-manager/forwarder/nsx-forwarder.nro",
               "\"/config/nsx-manager/forwarder/nsx-forwarder.nro\" "
               "--handoff=/config/nsx-manager/staging/handoff.json");
```

**The path is absolute and passed explicitly.** The predecessor opened `"forwarder.conf"`
relative to the current directory (`app-forwarder/source/main.cpp:28`), which worked only because
hbmenu happens to `chdir`, and which on failure produced empty strings that were then passed to
`rename()` and `remove()` with no error path at all.

Both binaries link the **same** `nsx::core::Handoff`. The predecessor had two separate ad-hoc
`KEY=value` implementations - a writer at `utils.cpp:539-553` and a reader at
`app-forwarder/source/main.cpp:26-46` - which could drift apart silently.

## The swap

```
1. parse handoff              attempts >= max?  -> ROLLBACK
2. attempts++, rewrite atomically (tmp -> fsync -> rename)
3. re-verify staged_sha256    mismatch?         -> abort, target untouched
4. copy   staged      -> target.new, fsync
5. rename target      -> backup
6. rename target.new  -> target
7. verify target exists with the expected size
8. delete backup, staged, handoff
9. envSetNextLoad(target)
```

**Why steps 5 and 6 are two renames:** FatFs cannot rename onto an existing file. There is
therefore a sub-millisecond window in which `target` does not exist. It is covered twice - by the
boot-time recovery table below, and by the repair NRO, which is a different file and unaffected.

**Why step 2 comes before step 3:** the attempt counter must be durable *before* anything risky
happens. Otherwise a crash inside step 3 loops forever.

**Why step 3 re-verifies a hash the application already checked:** it catches corruption between
the download and the swap - a bad SD card, an unclean shutdown, or another program writing to the
staging directory.

## Recovery

Checked at startup by both binaries, from the files that exist:

| `handoff.json` | `target` | `backup` | Meaning | Action |
|---|---|---|---|---|
| absent | present | absent | Normal | Nothing |
| present, attempts 0 | present | absent | Staged, not started | Proceed |
| present, 0 < attempts < max | present | present | Interrupted | Retry |
| present, attempts >= max | either | present | Repeated failure | **Roll back**, report, keep the log |
| present | **absent** | present | Power loss between the renames | Restore backup |
| present | absent | absent | Worst case | Re-verify staged; install if it matches, else direct the user to the zip |
| absent | absent | - | No application at all | User launches **NSX Manager (Repair)** |

The state machine is host-tested against
[`tests/fixtures/handoff/`](../../tests/fixtures/handoff/) - including `relative-path.json`,
which encodes the predecessor's bug as a permanent regression test.

## What the user sees

* **Success:** a progress screen, a restart, then the new version - roughly ten seconds.
* **Rollback:** "The update failed and your previous version was restored", with an error code
  and a pointer to `log.txt`.
* **Repair needed:** the application does not launch, so the user picks **NSX Manager (Repair)**
  in hbmenu, which restores and relaunches.

The third case is why the repair entry point is visible by default rather than hidden. A recovery
tool that a user must first install is not a recovery tool.

## Testing it

You cannot trust this path because it looks correct. From
[`../contributing/testing.md`](../contributing/testing.md):

1. Update from N-1 on a real console.
2. Pull the power during the download.
3. Pull the power between the two renames (build with `NSX_DEBUG_CRASH_AT_SWAP=1`).
4. Corrupt the staged NRO by hand and confirm the swap refuses.
5. Set `attempts` to `max_attempts` by hand and confirm rollback.
6. Delete the application NRO and recover using the repair entry.
