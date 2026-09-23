# Source tree

Every directory, one line each - plus where the predecessor's two god-files went.

## Top level

| Path | Contents |
|---|---|
| `.github/` | Workflows, issue and PR templates, CODEOWNERS, dependabot |
| `apps/forwarder/` | `nsx-forwarder.nro` - completes the self-update swap |
| `apps/rcm-payload/` | `nsx_rcm.bin` - hekate/BDK derived, GPL-2.0-only, **isolated** |
| `assets/` | Staged into romfs at build time |
| `cmake/` | `NsxVersion`, `NsxLayer`, `NsxRomfs`, `NsxRcmPayload` |
| `docs/` | Everything in this directory |
| `src/nsx/` | The application |
| `tests/` | Host unit tests and fixtures |
| `third_party/` | Pinned submodules and vendored single headers |
| `tools/` | `hooks/`, `lint/`, `release/`, `deps/`, `cacert/` |

## `src/nsx/core/` - pure C++20, host-compilable

| Module | Purpose |
|---|---|
| `result/` | `Result<T, E>`, error codes, error formatting |
| `version/` | `semver` parse and compare; generated `version.hpp` |
| `update/` | `manifest`, `update_policy`, `handoff`, `channel` |
| `paths/` | SD path builders, `traversal_guard` |
| `text/` | Trimming, case, ellipsis, byte sizes, durations |
| `json/` | Safe typed getters over nlohmann |
| `hash/` | `sha256` - self-contained, so it works on host and device without mbedTLS |
| `net/` | URL building and validation; Mega link key derivation (pure maths) |
| `log/` | The logger interface and sink concept. No I/O. |

## `src/nsx/platform/` - thin libnx wrappers

| Module | Purpose |
|---|---|
| `fs/` | SD operations, free space, archive bit, concatenation attribute |
| `power/` | Shutdown, reboot, reboot-to-payload |
| `system/` | Model detection (Erista/Mariko), applet mode, exFAT probe |
| `network/` | `nifm` connection status |
| `romfs/` | Mounting and asset reads |
| `launch/` | `envSetNextLoad` |

## `src/nsx/infra/` - adapters

| Module | Purpose |
|---|---|
| `http/` | curl client, TLS configuration, progress, generated CA bundle |
| `github/` | Release client, URL building, ETag cache, backoff |
| `archive/` | zipper wrapper, traversal-guarded extraction |
| `storage/` | Settings store, cache store, file logger |
| `mega/` | AES-CTR streaming (the device half of Mega support) |

## `src/nsx/domain/` - use-cases

| Module | Purpose |
|---|---|
| `selfupdate/` | Update service, staging, rollback |
| `cfw/` | CFW detection and pack updates |
| `firmware/` | Firmware listing and Daybreak handoff |
| `catalog/` | Ports, translations, mods, cheats |
| `motd/` | Message of the day |

## `src/nsx/ui/` and `src/nsx/app/`

| Module | Purpose |
|---|---|
| `ui/app_shell/` | Main frame, splash, theme, sound |
| `ui/tabs/` | One directory per tab |
| `ui/pages/` | Confirm, worker, dialogue, warning, changelog |
| `ui/widgets/` | Reusable components |
| `ui/i18n/` | Key constants and loader |
| `app/` | `main.cpp`, `bootstrap`, `container` |

## Where the predecessor's god-files went

### `utils.cpp` (822 lines) -> nine modules

| Predecessor symbols | New home |
|---|---|
| `formatListItemTitle:250`, `lowerCase:328`, `upperCase:337`, `trim:662-681`, `getErrorMessage:346` | `core/text/` - **host-tested** |
| `getBoolValue:399`, `getValueFromKey:406` | `core/json/` - **host-tested** |
| `readConfFile:555`, `createForwarderConfig:539` | `core/update/handoff` - **host-tested** |
| `showDialogBoxInfo:97`, `showDialogBoxBlocking:109` | `ui/pages/dialogue/` |
| `shutDown:267`, `rebootToPayload:273` | `platform/power/` |
| `isErista:312`, `isApplet:363`, `isSdExFAT:805` | `platform/system/` |
| `fixArchiveBit:735`, `cleanFiles:577`, `doDelete:447`, `saveToFile:296` | `platform/fs/` and `infra/storage/` |
| `getLatestTag:278`, `getGithubJSONBody:691`, `getLatestCFWPack:598` | `infra/github/` |
| `getMOTD:515`, `wasMOTDAlreadyDisplayed:482` | `domain/motd/` |
| `getContentsPath:382`, `getNANDType:633`, `getGMPackVersion:425` | `domain/cfw/` |
| `writeLog:415` | `infra/storage/file_logger`, behind `core/log` |

### `download.cpp` (565 lines) -> five modules

| Predecessor symbols | New home |
|---|---|
| `WriteMemoryCallback:49`, `checkSize:134`, `downloadFile:317`, `downloadPage:444` | `infra/http/curl_client` |
| `mega_id:154`, `mega_node_key:235`, `mega_key:287`, `mega_iv:301` | `core/net/mega_link` - **pure maths, now host-testable with known vectors** |
| `mega_url:178` and AES-CTR streaming | `infra/mega/` |
| `getRequest:488`, `getLinks:505`, `getLinksFromGitHubReleases:529` | `infra/github/release_client` |
| `fetchTitle:408` - regex-scraped `<title>` | **Deleted.** The manifest replaces it. |
| `API_AGENT:22` - hardcoded version | `core/version` generated constant |

And `main_frame.cpp:53-73`, the digit-stripping version comparison, becomes
`core/version/semver` plus `core/update/update_policy` - both pure, both tested.

## What is deliberately absent

* **No `util/`, `common/`, `helpers/` or `misc/`.** Those names are where god-files come from. If
  something does not fit, the concept has not been named yet.
* **No dead code.** The predecessor compiled and linked a 5,209-line `icons_page.cpp` that
  nothing instantiated into every shipped binary. If nothing references it, delete it.
* **No mirrored `include/` tree.** Headers sit beside their sources; see
  [ADR-0003](../adr/0003-use-a-layered-source-tree-with-co-located-headers.md).
