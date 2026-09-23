# Updating NSX Manager

NSX Manager updates itself. This page explains what it does, and what to do if it goes wrong.

## Normal update

1. NSX Manager checks for updates on launch (at most once every six hours).
2. When one is available it tells you, with the changes.
3. You accept.
4. It downloads the new version and **verifies it** before doing anything with it.
5. The app closes, swaps itself, and reopens on the new version.

Around ten seconds. You can also check manually from the **About** screen.

## What it checks

Before the new version replaces anything:

- The download came over a **verified secure connection**.
- Its **SHA-256 fingerprint matches** what the release published.
- The size matches.
- The fingerprint is **checked a second time** immediately before the swap.

If any check fails, the update stops and your existing version stays exactly as it was. NSX
Manager never runs or installs something it could not verify.

## Your previous version is kept

Until the new version is confirmed in place, the old one is kept as a backup. If the swap fails
- a crash, a power loss, a bad SD card - it is restored automatically on the next launch, and you
are told what happened.

## If NSX Manager stops launching

This should not happen. If it does, you are not stuck.

1. Open the homebrew menu.
2. Launch **NSX Manager (Repair)**.
3. It restores your working version and relaunches it.

That entry exists precisely for this, which is why it is installed from the start rather than
being something you would have to go and fetch.

If repair does not work, reinstall from the zip
([`installation.md`](installation.md)). **Your settings are not affected** - the archive contains
only the application file.

## Messages you may see

| Message | Meaning | What to do |
|---|---|---|
| **Set your console clock** | Your console's date is wrong, so secure connections cannot be verified. | Fix the date in System Settings. NSX Manager will not skip this check. |
| **Update check unavailable** | It could not reach GitHub. | Check your connection. It will retry automatically. |
| **Download failed - the file did not match** | The download was corrupted or altered. It retried once and stopped. | Try later. If it repeats, [report it](https://github.com/mateussantoos/nsx-manager/issues). |
| **This version is too old to update itself** | Too much changed since your version for an in-app update to be safe. | Reinstall from the zip - the message includes the link. |
| **The update failed and your previous version was restored** | The swap did not complete. | Nothing. You are on your working version. `log.txt` has the detail. |
| **Not enough space** | Checked before downloading anything. | Free some space. |

## Not enough space

NSX Manager checks free space **before** it starts downloading and tells you how much it needs.
It will not fill your card and fail halfway.

## Updating manually

You never have to use the in-app updater.

1. Download `nsx-manager-<version>.zip` from the
   [latest release](https://github.com/mateussantoos/nsx-manager/releases/latest).
2. Extract it to your SD card root, replacing the existing file.

Settings are unaffected.

## A note on releases

Every release publishes `SHA256SUMS` so you can verify any download by hand. Releases are not yet
cryptographically signed - a planned improvement, documented openly in
[`SECURITY.md`](../../SECURITY.md) rather than glossed over.
