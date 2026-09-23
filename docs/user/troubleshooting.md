# Troubleshooting

## Get the log first

`/config/nsx-manager/log.txt` on your SD card. Attach it to any bug report - it usually contains
the answer.

Error messages include a code like `NSX-UPD-0203`. Quote it; it identifies the exact failure
([`../reference/error-codes.md`](../reference/error-codes.md)).

## Launching

**It does not appear in the homebrew menu.**
The path must be exactly `/switch/nsx-manager/nsx-manager.nro`. A nested folder from extracting
the zip in the wrong place is the usual cause.

**It crashes immediately.**
Check your Atmosphere version - very old versions lack services NSX Manager needs. If you just
updated and it now crashes, launch **NSX Manager (Repair)** from the homebrew menu.

**It is slow, or runs out of memory.**
You are probably in applet mode (launched from the album). Hold R while launching a game instead;
that gives homebrew far more memory.

## Network

**"Set your console clock".**
Your console's date is wrong, so secure connections cannot be verified. Fix it in
System Settings > System > Date and Time. NSX Manager will not skip this check - the date is part
of how a secure connection is proved genuine.

**"Update check unavailable".**
It could not reach GitHub. Check Wi-Fi in System Settings. If other homebrew works but NSX Manager
does not, a DNS-blocking configuration (90DNS) may be blocking more than intended.

**Downloads are very slow.**
The Switch's Wi-Fi is weak. Move closer to the router and prefer 5 GHz. Large downloads take a
while; this is the hardware.

## Updates

**"Download failed - the file did not match".**
The download was corrupted or altered. NSX Manager retried once and stopped rather than install
something unverified. Try again later. If it keeps happening on a good connection,
[report it](https://github.com/mateussantoos/nsx-manager/issues).

**"This version is too old to update itself".**
Too much changed for the in-app path to be safe. Reinstall from the zip - the message has the
link. Your settings survive.

**"The update failed and your previous version was restored".**
Working as designed. You are on your previous, working version. `log.txt` says why.

**NSX Manager will not launch after an update.**
Homebrew menu -> **NSX Manager (Repair)**. It restores your working version. If that fails,
reinstall from the zip; settings are unaffected.

## Storage

**"Not enough space".**
Checked before anything downloads. The message says how much is needed. Use **Tools > Clean
temporary files**.

**Files will not install, or the console does not see them.**
Usually the archive bit on exFAT cards. Use **Tools > Fix archive bit**.

**Large files fail on a 4 GB+ download.**
A FAT32 card cannot hold a single file over 4 GB. exFAT can, but is more prone to corruption on
an unclean shutdown. Prefer FAT32 unless you specifically need large files.

## Reporting a bug

[Open an issue](https://github.com/mateussantoos/nsx-manager/issues/new/choose) with:

- the NSX Manager version (About screen) - please not "latest"
- your console model (Erista / Mariko)
- your firmware and Atmosphere version
- what you expected and what happened
- the error code, if there was one
- `/config/nsx-manager/log.txt`

**Found a security problem?** Do not open an issue. Follow
[`SECURITY.md`](../../SECURITY.md).
