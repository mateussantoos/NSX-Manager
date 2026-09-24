// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "nsx/core/hash/sha256.hpp"
#include "nsx/core/result/result.hpp"
#include "nsx/core/version/semver.hpp"

namespace nsx::core {

/// @brief The catalogue schema version this client understands.
inline constexpr int kSupportedCatalogSchema = 1;

/// @brief What a catalogue entry is.
/// @details An unknown kind is **skipped**, not an error, so a catalogue can
///          introduce a content type without breaking every older client.
/// @since 0.3.0
enum class ContentKind
{
    CfwPack,      ///< A custom firmware bundle; extracted over the SD card.
    Firmware,     ///< Official Nintendo firmware, handed to Daybreak.
    Tool,         ///< A homebrew application.
    Cheats,       ///< Cheat code archives.
    Translation,  ///< Game translation patches.
    Theme         ///< Home menu themes.
};

/// @brief Where an item is installed.
///
/// @details A **closed set**, deliberately. The catalogue names a destination
///          *kind* and this client maps it to a path; the catalogue never
///          carries a path of its own. A compromised or mistaken catalogue can
///          therefore choose among destinations this application already
///          writes to, and cannot invent a new one - it cannot aim an install
///          at `/atmosphere/exefs_patches/` or anywhere else not listed here.
///
///          `docs/reference/sd-layout.md` holds the mapping.
/// @since 0.3.0
enum class InstallTarget
{
    SdRoot,              ///< The card root; how a CFW pack is laid out.
    AtmosphereContents,  ///< `/atmosphere/contents/`.
    Bootloader,          ///< `/bootloader/`.
    FirmwareStaging,     ///< `/firmware/`, for Daybreak to pick up.
    SwitchApps,          ///< `/switch/`.
    Themes               ///< `/themes/`.
};

/// @brief Why a catalogue was rejected.
/// @since 0.3.0
enum class CatalogError
{
    NotJson,                   ///< Not parseable as JSON.
    NotObject,                 ///< Top level was not an object.
    UnsupportedSchemaVersion,  ///< Newer than this client understands.
    MissingField,              ///< A required field was absent.
    WrongType,                 ///< A field had the wrong JSON type.
    InvalidId,                 ///< Empty, or not `[a-z0-9._-]`.
    DuplicateId,               ///< Two items claim the same id.
    InsecureUrl,               ///< Not an https URL.
    InvalidDigest,             ///< Not 64 lowercase hex characters.
    InvalidSize,               ///< Not a positive integer.
    NoItems                    ///< Nothing usable in the whole catalogue.
};

/// @brief One installable thing.
/// @since 0.3.0
struct CatalogItem
{
    std::string id;       ///< Stable machine identifier; unique in the catalogue.
    std::string name;     ///< Display name.
    std::string summary;  ///< One line, shown under the name.

    ContentKind kind{};             ///< What it is.
    InstallTarget target{};         ///< Where it goes.
    std::string versionText;        ///< As the upstream project publishes it.
    std::optional<SemVer> version;  ///< Parsed leniently; absent when unparseable.

    std::string url;          ///< https download URL.
    std::uint64_t size{};     ///< Exact byte count; checked before hashing.
    Sha256::Digest sha256{};  ///< Expected digest; checked before use.

    /// @brief Whether `preserve.txt` applies when extracting this.
    /// @details True for a CFW pack, which overwrites the user's card. False
    ///          for a self-contained download that touches nothing of theirs.
    bool honourPreserveRules{};

    std::optional<SemVer> minAtmosphere;  ///< Warned about, never enforced.
    std::string notesUrl;                 ///< Human-facing page.
};

/// @brief A parsed content catalogue.
/// @see docs/reference/catalog-manifest.md
/// @since 0.3.0
struct Catalog
{
    int schemaVersion{};             ///< Always kSupportedCatalogSchema once parsed.
    std::string updatedAt;           ///< ISO-8601 UTC.
    std::vector<CatalogItem> items;  ///< Everything understood, in file order.

    /// @brief Every item of one kind.
    /// @param kind The kind to select.
    /// @return The matching items, in catalogue order.
    [[nodiscard]] std::vector<const CatalogItem*> ofKind(ContentKind kind) const;

    /// @brief Find an item by its identifier.
    /// @param id The identifier to look for.
    /// @return The item, or nullptr.
    [[nodiscard]] const CatalogItem* find(std::string_view id) const;
};

/// @brief Parse and validate a content catalogue.
///
/// @details Shares the update manifest's discipline: strict about everything
///          that protects the device, tolerant of everything that does not, so
///          the format can grow within schema version 1. An unknown `kind` or
///          an unknown `target` drops **that item**, not the catalogue - one
///          unreadable entry should not cost the user the other forty.
///
///          One deliberate difference from `update.json`. That manifest
///          requires every asset URL to be on `github.com`, because it
///          describes the application's own binary and we control where that is
///          published. Catalogue content comes from many upstream projects and
///          their mirrors, so the host cannot be fixed - the guarantee here is
///          https plus a mandatory SHA-256, which is what actually decides
///          whether the bytes are the intended ones.
///
///          **Never throws.**
///
/// @param json The raw document.
/// @return The catalogue, or the reason it was rejected.
/// @since 0.3.0
[[nodiscard]] Result<Catalog, CatalogError> parseCatalog(std::string_view json);

/// @brief A human-readable reason for a rejection.
/// @param error The error to describe.
/// @return A short English description.
/// @since 0.3.0
[[nodiscard]] std::string_view describe(CatalogError error);

/// @brief Render a content kind as it appears in the catalogue.
/// @param kind The kind to render.
/// @return The wire spelling, e.g. `cfw-pack`.
/// @since 0.3.0
[[nodiscard]] std::string_view toString(ContentKind kind);

/// @brief Render an install target as it appears in the catalogue.
/// @param target The target to render.
/// @return The wire spelling, e.g. `sd-root`.
/// @since 0.3.0
[[nodiscard]] std::string_view toString(InstallTarget target);

}  // namespace nsx::core
