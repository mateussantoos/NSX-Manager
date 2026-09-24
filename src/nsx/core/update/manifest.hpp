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

/// @brief The manifest schema version this client understands.
/// @details A manifest declaring anything higher is refused outright rather
///          than partially interpreted - that refusal is the forward
///          compatibility escape hatch described in ADR-0005.
inline constexpr int kSupportedManifestSchema = 1;

/// @brief Release channel.
/// @since 0.2.0
enum class Channel
{
    Stable,
    Beta
};

/// @brief What a release asset is for.
/// @details The client selects by kind and **never** by parsing a filename.
///          An unknown kind is skipped rather than treated as an error, so a
///          future asset type does not break an older client.
/// @since 0.2.0
enum class AssetKind
{
    AppNro,        ///< The application binary the in-app updater downloads.
    ForwarderNro,  ///< The forwarder / repair binary.
    SdOverlayZip   ///< First-install archive; never used by the in-app path.
};

/// @brief Why a manifest was rejected.
/// @since 0.2.0
enum class ManifestError
{
    NotJson,                   ///< Not parseable as JSON at all.
    NotObject,                 ///< Top level was not a JSON object.
    UnsupportedSchemaVersion,  ///< Newer than this client understands.
    WrongProduct,              ///< `product` is not this application.
    MissingField,              ///< A required field was absent.
    WrongType,                 ///< A field had the wrong JSON type.
    InvalidVersion,            ///< `version` or `min_supported` is not SemVer.
    InvalidTag,                ///< `tag` missing, or not `v` + version.
    InvalidTimestamp,          ///< `published_at` is not ISO-8601 UTC.
    InvalidChannel,            ///< `channel` is not stable or beta.
    NoAssets,                  ///< The asset list was empty.
    InvalidAssetName,          ///< A name that could escape its directory.
    InsecureUrl,               ///< Not https, or not a GitHub release asset.
    InvalidDigest,             ///< Not 64 lowercase hex characters.
    InvalidSize,               ///< Not a positive integer.
    NoAppAsset                 ///< Nothing of kind `app-nro` to download.
};

/// @brief One downloadable artefact described by the manifest.
/// @since 0.2.0
struct ManifestAsset
{
    std::string name;         ///< Bare filename, validated to be traversal-free.
    AssetKind kind{};         ///< What the client should use it for.
    std::string url;          ///< https GitHub release download URL.
    std::uint64_t size{};     ///< Exact byte count; checked before hashing.
    Sha256::Digest sha256{};  ///< Expected digest; checked before use.
};

/// @brief A parsed and validated `update.json`.
/// @see docs/reference/update-manifest.md
/// @since 0.2.0
struct UpdateManifest
{
    int schemaVersion{};
    SemVer version;           ///< The released version.
    std::string tag;          ///< The git tag; always `v` + version.
    std::string publishedAt;  ///< ISO-8601 UTC.
    Channel channel{Channel::Stable};
    bool mandatory{};                     ///< Block the menu until updated.
    SemVer minSupported;                  ///< Oldest version allowed to update in-app.
    std::optional<SemVer> minAtmosphere;  ///< Warned about, never enforced.
    std::string changelogUrl;
    std::string releaseNotesUrl;
    std::vector<ManifestAsset> assets;

    /// @brief Find the first asset of a given kind.
    /// @param kind The kind to look for.
    /// @return The asset, or nullptr when the manifest carries none.
    [[nodiscard]] const ManifestAsset* findAsset(AssetKind kind) const;
};

/// @brief Parse and validate a manifest document.
///
/// @details Validation is deliberately stricter than the JSON Schema, because
///          the schema cannot express the rules that actually protect the
///          device: an asset name must be a bare filename, a URL must be an
///          https GitHub release download, and a digest must be present and
///          well formed. An asset failing any of those is a rejection of the
///          whole manifest, not a skipped entry - a manifest containing one
///          hostile asset is not a manifest to be trusted in part.
///
///          Unknown top-level fields and unknown asset kinds are tolerated so
///          the format can grow within schema version 1.
///
///          **Never throws.** A hostile manifest is a rejection, not an
///          exception.
///
/// @param json The raw document.
/// @return The parsed manifest, or the reason it was rejected.
/// @since 0.2.0
[[nodiscard]] Result<UpdateManifest, ManifestError> parseManifest(std::string_view json);

/// @brief A human-readable reason for a rejection.
/// @param error The error to describe.
/// @return A short English description, suitable for a log line.
/// @since 0.2.0
[[nodiscard]] std::string_view describe(ManifestError error);

/// @brief Render an asset kind as it appears in the manifest.
/// @param kind The kind to render.
/// @return The wire spelling, e.g. `app-nro`.
/// @since 0.2.0
[[nodiscard]] std::string_view toString(AssetKind kind);

}  // namespace nsx::core
