# Installing NSX Manager

## Before you start

You need a Nintendo Switch already running custom firmware (Atmosphere). NSX Manager manages a
CFW installation; it does not create one.

> **Coming from NSX Updater 4.2.1?** NSX Manager is a different application with a new name, a
> new binary and new folders. Its version number starts at **0.1.0** - that is not a downgrade,
> it is a fresh start. Install it alongside the old one; nothing is shared or overwritten.

## Install

1. Download `nsx-manager-<version>.zip` from the
   [latest release](https://github.com/mateussantoos/nsx-manager/releases/latest).
2. Extract it to the **root** of your SD card.
3. Put the SD card back and boot into CFW.
4. Open the **homebrew menu** (hold R while launching a game, or use the album).
5. Launch **NSX Manager**.

The archive contains exactly one file:

```
switch/
└── nsx-manager/
    └── nsx-manager.nro
```

Nothing else. It cannot overwrite your settings, themes or any other homebrew - and reinstalling
later is safe for the same reason.

## Verify your download (recommended)

Every release publishes `SHA256SUMS`. NSX Manager writes executable code to your SD card, so
checking it is worth thirty seconds.

**Windows (PowerShell):**

```powershell
Get-FileHash nsx-manager-0.1.0.zip -Algorithm SHA256
```

**macOS / Linux:**

```sh
sha256sum -c SHA256SUMS --ignore-missing
```

Compare against the line in `SHA256SUMS` from the same release. If they differ, **do not use the
file** - download it again, and if it differs a second time, [report it](https://github.com/mateussantoos/nsx-manager/security/advisories/new).

## Two entries in the homebrew menu

After the first launch you will see two:

| Entry | What it is |
|---|---|
| **NSX Manager** | The application |
| **NSX Manager (Repair)** | Recovery. Use it only if NSX Manager stops launching after an update. |

The repair entry is deliberately visible. A recovery tool you have to install after something
breaks is not a recovery tool. See [`updating.md`](updating.md).

## What gets created

On first launch, NSX Manager creates `/config/nsx-manager/` for your settings, logs and update
cache. Nothing else on your SD card is touched until you ask it to install something.

Full list: [`../reference/sd-layout.md`](../reference/sd-layout.md).

## Uninstalling

Delete these two folders:

```
/switch/nsx-manager/
/config/nsx-manager/
```

Content NSX Manager installed for you - firmware, cheats, translations - belongs to those systems
and is left alone. Removing it is a separate, deliberate action.

## Trouble

| Problem | Try |
|---|---|
| Not in the homebrew menu | Check the path is exactly `/switch/nsx-manager/nsx-manager.nro` |
| Crashes on launch | Confirm your Atmosphere version; check `/config/nsx-manager/log.txt` |
| "Set your console clock" | Your console's date is wrong, which breaks secure connections. Fix it in System Settings. |
| Something else | [`troubleshooting.md`](troubleshooting.md) |
