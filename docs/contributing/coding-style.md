# Coding style

Most of this is enforced by `.clang-format` and `.clang-tidy`, so you rarely have to think about
it. The parts a tool cannot check are the parts worth reading.

Naming decision: [`PROJECT_SCOPE.md` section 7](../PROJECT_SCOPE.md#7-naming-conventions).

## Naming

| Entity | Convention | Example |
|---|---|---|
| Directories | `snake_case`, singular | `src/nsx/core/update/` |
| Files | `snake_case`, stem is the primary type | `SemVer` -> `semver.hpp` |
| Namespaces | lowercase, `nsx::<layer>::<module>` | `nsx::infra::http` |
| Types | `PascalCase` | `UpdateManifest` |
| Enum constants | `PascalCase` | `UpdateAction::UpToDate` |
| **Functions and methods** | **`camelCase`** | `parseSemVer()` |
| Locals and parameters | `camelCase` | `statusCode` |
| Private and protected members | `m_camelCase` | `m_statusText` |
| Constants and `constexpr` | `kPascalCase` | `kMaxFetchLinks` |
| Macros | `UPPER_SNAKE_CASE`, avoid | `NSX_HANDOFF_PATH` |

`camelCase` for functions is a single answer to something the predecessor was inconsistent about
- `CreateDownloadItems()` and `createList()` appear in the same file.
`readability-identifier-naming` in `.clang-tidy` enforces this table, with warnings as errors.

## Headers

`#pragma once`, never include guards.

Include order (clang-format regroups automatically):

```cpp
#include "nsx/core/version/semver.hpp"   // 1. own header first

#include <stdint.h>                      // 2. C standard library

#include <string>                        // 3. C++ standard library
#include <string_view>

#include <nlohmann/json.hpp>             // 4. third-party

#include "nsx/core/result/result.hpp"    // 5. project
```

Project includes always use the full path from the include root -
`"nsx/<layer>/<module>/<file>.hpp"`. Never relative, never `../`.

## Errors

**Return errors; do not throw across a layer boundary.** `core` uses `Result<T, E>` /
`std::expected`. Exceptions are for genuinely exceptional conditions, never for control flow.

```cpp
// yes
[[nodiscard]] std::expected<SemVer, SemVerError> parseSemVer(std::string_view text);

// no - the predecessor's unguarded std::stoi threw std::out_of_range on a
// date-shaped tag and terminated the app (main_frame.cpp:53-73)
SemVer parseSemVer(std::string_view text);   // throws on bad input
```

**Never swallow an error into a default value.** The predecessor turned a 404 into an empty
string (`download.cpp:493-500`), which then meant "you are up to date" - and the self-updater was
silently broken for its entire life.

**Never crash from a lower layer.** `download.cpp:381` called `brls::Application::crash()` from
inside the network layer. Return the error; let `ui` decide how to present it.

## Documentation

Every public entity in a header under `src/nsx/` needs a Doxygen comment. Doxygen runs with
`EXTRACT_ALL = NO` and `WARN_AS_ERROR`, so an undocumented public entity **fails the build**.

```cpp
/// @brief Parse a strict SemVer 2.0.0 string.
/// @param text Version text; one optional leading 'v' is accepted.
/// @return The parsed version, or an error describing why it was rejected.
/// @note Never throws. Rejects rather than guessing.
/// @since 0.1.0
[[nodiscard]] std::expected<SemVer, SemVerError> parseSemVer(std::string_view text);
```

Exempt: `detail::` namespaces, `.cpp`-local statics, `third_party/`, `apps/rcm-payload/`.

## Comments

Explain **why**. The code already says what.

```cpp
// Two renames, not one: FatFs cannot rename onto an existing file. The window
// where the target is absent is covered by the boot-time recovery check.
renameOrFail(target, backup);
renameOrFail(staged, target);
```

Delete commented-out code. Git remembers it. The predecessor left dead blocks in at least six
files, plus two sources renamed to `.meh` to exclude them from the build.

## Things that are actually forbidden

| Rule | Checked by |
|---|---|
| No version literals | `forbid_hardcoded_version.sh` |
| No disabling TLS verification, no plaintext URLs | `forbid_insecure_curl.sh` |
| No upward or sideways layer dependency | `check_layering.sh` |
| No source shared with `apps/rcm-payload/` | `check_license_isolation.sh` |
| Every first-party file has an SPDX line | `check_license_isolation.sh` |
| No hardcoded user-visible strings | review, then `check_i18n.py` |
| UTF-8, LF, no BOM | `check_encoding.sh` |

## Style notes a tool cannot check

* **Prefer `std::string_view` for parameters you do not own.**
* **Mark single-argument constructors `explicit`.**
* **`[[nodiscard]]` on anything returning a `Result` or an `expected`.**
* **Do not inject into a third-party namespace.** The predecessor opened `namespace brls { }` in
  its own files, so nobody could tell whose code was whose.
* **No header-only implementations for non-templates.** `search_list_item.hpp` was 295 lines of
  implementation duplicated into every translation unit that included it.
* **One class per file**, unless two types are genuinely inseparable.
