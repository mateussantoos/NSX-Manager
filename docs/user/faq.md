# FAQ

**What is NSX Manager?**
A homebrew application that runs on your Switch and manages a custom firmware installation - the
CFW pack, official firmware, saves, cheats, ports, translations and maintenance tools - and keeps
itself up to date.

**How is it different from NSX Updater?**
It is a complete rewrite. The old version's self-update pointed at a repository that does not
exist, so it never once updated anyone. It also disabled certificate verification on every
download. Both are fixed, along with the structure that made them hard to find.

**Why does the version number go down, from 4.2.1 to 0.1.0?**
Because `4.2.1` was inherited from an upstream project and described none of this code. This is a
new codebase with a new binary and new folders, honestly numbered. `0.x` means it is not yet
feature-complete, which is true. See
[ADR-0004](../adr/0004-adopt-semantic-versioning-from-0-1-0-with-v-prefixed-tags.md).

**Can I keep NSX Updater installed?**
Yes. Different binary, different folders, nothing shared.

**Does it download games?**
No, and it never will. NSX Manager does not download, install, index or link to commercial game
content. See [non-goals](../PROJECT_SCOPE.md#4-non-goals).

**Will it get me banned?**
NSX Manager does not connect to Nintendo's services. Running custom firmware at all carries ban
risk; that is a decision you made before installing this. It does not increase it.

**Why two entries in my homebrew menu?**
The second, **NSX Manager (Repair)**, is a recovery tool used only if an update leaves the app
unable to launch. It is installed from the start because a recovery tool you have to go and fetch
after something breaks is not a recovery tool.

**Why does it insist my console clock is right?**
A secure connection is proved genuine partly by checking certificate dates. A wrong clock makes
that impossible. The alternative is to skip verification, which is exactly the flaw being fixed -
so NSX Manager asks you to fix the clock instead.

**Is my data sent anywhere?**
No. NSX Manager has no analytics and no telemetry. It contacts GitHub to check for updates and to
download what you ask for. Nothing else.

**Which consoles work?**
Any Switch running Atmosphere - Erista (V1), Mariko (V2, Lite, OLED). Atmosphere only; ReiNX and
SX OS are not supported.

**Is it safe? It writes to my SD card.**
It does, which is why: every download is over a verified secure connection, every file is
SHA-256 checked before use, a failed update restores your previous version, and nothing is
extracted to your SD card root. Releases are not yet cryptographically signed - a planned
improvement, documented openly in [`SECURITY.md`](../../SECURITY.md).

**What language is it in?**
English and Brazilian Portuguese, both fully translated. The old version claimed twelve languages
but eight were identical copies of Portuguese.

**Can I help translate it?**
Yes. Copy `assets/i18n/en-US/`, translate the values, open a pull request. CI checks that no key
is missing and that placeholders match, so a partial translation cannot ship by accident.

**Where is the source?**
[github.com/mateussantoos/nsx-manager](https://github.com/mateussantoos/nsx-manager). GPL-3.0.

**When is 1.0.0?**
When it does everything the old version did, correctly. The checklist is public:
[road to 1.0.0](../PROJECT_SCOPE.md#19-road-to-100---parity-checklist).
