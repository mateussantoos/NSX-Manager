# Glossary

Switch homebrew vocabulary, for anyone reading this project without that background.

## Hardware

**Erista** - the original 2017 Switch (V1). Vulnerable to the fusee-gelee bootrom exploit, so it
can enter RCM and be booted into custom firmware from an external device.

**Mariko** - every later model (V2, Lite, OLED). The bootrom hole is patched, so CFW requires a
modchip. Some operations differ: reboot-to-payload, for instance, needs a different mechanism.

**RCM** - Recovery Mode. A low-level mode on Erista that accepts a payload over USB. The entry
point for booting CFW.

## Software

**CFW** - custom firmware. Software that runs alongside or instead of Nintendo's, enabling
homebrew.

**Atmosphere** - the CFW NSX Manager targets. The only one supported.

**hekate** - a bootloader that chainloads Atmosphere and offers partition and emuMMC tools.

**BDK** - Bootloader Development Kit, the library hekate is built on. **GPL-2.0-only**, which is
the entire reason `apps/rcm-payload/` is kept isolated
([ADR-0011](adr/0011-license-gplv3-and-isolate-the-gplv2-only-rcm-payload.md)).

**payload** - a small freestanding program run before the OS, over RCM or by a chainloader.
`nsx_rcm.bin` is one.

**emuMMC / emuNAND** - an SD-card copy of the console's internal storage, so CFW runs without
touching the real one.

**sigpatches** - patches allowing unsigned software to run. Part of a CFW pack.

**NSX Pack / CNX Pack** - the Brazilian CFW distribution NSX Manager manages. NSX is the current
name; CNX (by Costela) is the lineage it came from.

## Homebrew

**homebrew** - unofficial software. NSX Manager is homebrew.

**NRO** - the homebrew executable format. `nsx-manager.nro`.

**hbmenu / hbloader** - the homebrew menu, and the loader that runs an NRO. `envSetNextLoad` asks
hbloader to run a different NRO next, which is how the self-update forwarder works.

**NACP** - metadata inside an NRO: title, author, version. What hbmenu displays. Generated from
`/VERSION` so it can never disagree with the running code.

**romfs** - a read-only filesystem embedded in the NRO. Assets, i18n and the RCM payload live
there.

**libnx** - the C library giving homebrew access to Switch services.

**devkitPro / devkitA64 / devkitARM** - the toolchain. devkitA64 targets the Switch's ARMv8
processor; devkitARM targets the ARM7 the RCM payload runs on.

**Borealis** - the UI framework, giving a Switch-native look and gamepad navigation
([ADR-0010](adr/0010-use-the-borealis-fork-as-the-ui-framework.md)).

**Daybreak** - Atmosphere's firmware installer. NSX Manager downloads firmware and hands off to
it rather than installing firmware itself.

**JKSV** - a save manager NSX Manager can install and integrate with.

**FatFs** - the FAT filesystem implementation used on Switch. It **cannot rename onto an existing
file**, which is why the update swap is two renames rather than one.

**archive bit** - a FAT attribute the Switch uses to mark directories it should treat as files.
Wrong values make installed content invisible, which is what "fix archive bit" repairs.

## This project

**forwarder** - `nsx-forwarder.nro`. A running NRO cannot overwrite itself, so it chainloads the
forwarder, which performs the swap and chainloads back
([`architecture/self-update-forwarder.md`](architecture/self-update-forwarder.md)).

**handoff** - `handoff.json`, the file telling the forwarder what to swap
([`reference/handoff-format.md`](reference/handoff-format.md)).

**manifest** - `update.json`, published with every release, describing the version and its assets
with hashes ([`reference/update-manifest.md`](reference/update-manifest.md)).

**staging** - `/config/nsx-manager/staging/`, where a download lives while being verified. A
`.part` file there is never executed, extracted or chainloaded.

**repair NRO** - the forwarder as it appears in hbmenu, **NSX Manager (Repair)**. The guarantee
that a failed update never leaves you with nothing to launch.

## Elsewhere

**ADR** - Architecture Decision Record. See [`adr/README.md`](adr/README.md).

**SemVer** - Semantic Versioning 2.0.0. `MAJOR.MINOR.PATCH`, where a pre-release like
`1.0.0-rc.1` ranks **below** `1.0.0`.

**Conventional Commits** - the commit message format that makes history machine-readable, so
release notes can be generated rather than written.

**zip-slip** - an archive entry whose path escapes the extraction directory via `..`. Guarded
against, and absent entirely from the self-update path.
