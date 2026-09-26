// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/update/manifest.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

namespace nsx::core {

namespace detail {
namespace {

using Json = nlohmann::json;

constexpr std::string_view kProduct = "nsx-manager";
constexpr std::string_view kUrlPrefix = "https://github.com/";

/// An asset name reaches the filesystem. It must be a bare filename: no
/// separator, no parent reference, no leading dot. The manifest is remote data,
/// and this is the last place a hostile name can be stopped before it becomes a
/// path.
bool isBareFilename(std::string_view name)
{
    if (name.empty() || name.front() == '.') {
        return false;
    }
    if (name.find('/') != std::string_view::npos || name.find('\\') != std::string_view::npos) {
        return false;
    }
    if (name.find("..") != std::string_view::npos) {
        return false;
    }
    return std::ranges::all_of(name, [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
               c == '.' || c == '_' || c == '-' || c == '+';
    });
}

/// https, and specifically a GitHub release asset. A plaintext URL or a foreign
/// host in a manifest is not a mistake to work around - it is a signal that the
/// manifest is not what it claims to be.
bool isReleaseUrl(std::string_view url)
{
    return url.size() > kUrlPrefix.size() && url.compare(0, kUrlPrefix.size(), kUrlPrefix) == 0;
}

/// `YYYY-MM-DDTHH:MM:SSZ`. Shape only - the value is displayed, never used for
/// a security decision, so a full calendar parse would buy nothing.
bool isIso8601Utc(std::string_view s)
{
    if (s.size() != 20) {
        return false;
    }
    auto digit = [&](std::size_t i) { return s[i] >= '0' && s[i] <= '9'; };
    for (const std::size_t i : {0U, 1U, 2U, 3U, 5U, 6U, 8U, 9U, 11U, 12U, 14U, 15U, 17U, 18U}) {
        if (!digit(i)) {
            return false;
        }
    }
    return s[4] == '-' && s[7] == '-' && s[10] == 'T' && s[13] == ':' && s[16] == ':' &&
           s[19] == 'Z';
}

std::optional<AssetKind> parseKind(std::string_view text)
{
    if (text == "app-nro") {
        return AssetKind::AppNro;
    }
    if (text == "forwarder-nro") {
        return AssetKind::ForwarderNro;
    }
    if (text == "sd-overlay-zip") {
        return AssetKind::SdOverlayZip;
    }
    return std::nullopt;  // tolerated: an unknown kind is skipped, not fatal
}

}  // namespace
}  // namespace detail

const ManifestAsset* UpdateManifest::findAsset(AssetKind kind) const
{
    for (const ManifestAsset& a : assets) {
        if (a.kind == kind) {
            return &a;
        }
    }
    return nullptr;
}

Result<UpdateManifest, ManifestError> parseManifest(std::string_view json)
{
    using R = Result<UpdateManifest, ManifestError>;
    using Json = detail::Json;

    // nlohmann throws by default; this path must not. A truncated or hostile
    // document is a rejection with a reason, never an exception escaping into
    // the UI layer.
    const Json doc = Json::parse(json, nullptr, false);
    if (doc.is_discarded()) {
        return R::err(ManifestError::NotJson);
    }
    if (!doc.is_object()) {
        return R::err(ManifestError::NotObject);
    }

    auto has = [&](const char* key) { return doc.contains(key); };

    // Schema version first: everything below assumes version 1's shape, so
    // checking anything else before this would be interpreting a document we
    // have already decided we do not understand.
    if (!has("schema_version")) {
        return R::err(ManifestError::MissingField);
    }
    if (!doc["schema_version"].is_number_integer()) {
        return R::err(ManifestError::WrongType);
    }
    const int schema = doc["schema_version"].get<int>();
    if (schema != kSupportedManifestSchema) {
        return R::err(ManifestError::UnsupportedSchemaVersion);
    }

    if (!has("product")) {
        return R::err(ManifestError::MissingField);
    }
    if (!doc["product"].is_string()) {
        return R::err(ManifestError::WrongType);
    }
    if (doc["product"].get<std::string>() != detail::kProduct) {
        return R::err(ManifestError::WrongProduct);
    }

    UpdateManifest out;
    out.schemaVersion = schema;

    for (const char* key :
         {"version", "tag", "published_at", "channel", "mandatory", "min_supported", "assets"}) {
        if (!has(key)) {
            return R::err(ManifestError::MissingField);
        }
    }

    if (!doc["version"].is_string() || !doc["min_supported"].is_string()) {
        return R::err(ManifestError::WrongType);
    }
    const Result<SemVer, SemVerError> version = parseSemVer(doc["version"].get<std::string>());
    if (!version) {
        return R::err(ManifestError::InvalidVersion);
    }
    out.version = version.value();

    const Result<SemVer, SemVerError> minSupported =
        parseSemVer(doc["min_supported"].get<std::string>());
    if (!minSupported) {
        return R::err(ManifestError::InvalidVersion);
    }
    out.minSupported = minSupported.value();

    if (!doc["tag"].is_string()) {
        return R::err(ManifestError::WrongType);
    }
    out.tag = doc["tag"].get<std::string>();
    // The tag must be exactly "v" + version. A manifest whose tag and version
    // disagree describes two different releases.
    if (out.tag.size() < 2 || out.tag.front() != 'v' ||
        out.tag.substr(1) != doc["version"].get<std::string>()) {
        return R::err(ManifestError::InvalidTag);
    }

    if (!doc["published_at"].is_string()) {
        return R::err(ManifestError::WrongType);
    }
    out.publishedAt = doc["published_at"].get<std::string>();
    if (!detail::isIso8601Utc(out.publishedAt)) {
        return R::err(ManifestError::InvalidTimestamp);
    }

    if (!doc["channel"].is_string()) {
        return R::err(ManifestError::WrongType);
    }
    const std::string channel = doc["channel"].get<std::string>();
    if (channel == "stable") {
        out.channel = Channel::Stable;
    }
    else if (channel == "beta") {
        out.channel = Channel::Beta;
    }
    else {
        return R::err(ManifestError::InvalidChannel);
    }

    if (!doc["mandatory"].is_boolean()) {
        return R::err(ManifestError::WrongType);
    }
    out.mandatory = doc["mandatory"].get<bool>();

    if (has("min_atmosphere")) {
        if (!doc["min_atmosphere"].is_string()) {
            return R::err(ManifestError::WrongType);
        }
        if (const Result<SemVer, SemVerError> ams =
                parseSemVer(doc["min_atmosphere"].get<std::string>());
            ams) {
            out.minAtmosphere = ams.value();
        }
        else {
            return R::err(ManifestError::InvalidVersion);
        }
    }

    for (const auto& [key, field] : {std::pair{"changelog_url", &out.changelogUrl},
                                     std::pair{"release_notes_url", &out.releaseNotesUrl}}) {
        if (has(key)) {
            if (!doc[key].is_string()) {
                return R::err(ManifestError::WrongType);
            }
            *field = doc[key].get<std::string>();
        }
    }

    if (!doc["assets"].is_array()) {
        return R::err(ManifestError::WrongType);
    }
    if (doc["assets"].empty()) {
        return R::err(ManifestError::NoAssets);
    }

    for (const Json& entry : doc["assets"]) {
        if (!entry.is_object()) {
            return R::err(ManifestError::WrongType);
        }
        for (const char* key : {"name", "kind", "url", "size", "sha256"}) {
            if (!entry.contains(key)) {
                return R::err(ManifestError::MissingField);
            }
        }
        if (!entry["name"].is_string() || !entry["kind"].is_string() || !entry["url"].is_string() ||
            !entry["sha256"].is_string()) {
            return R::err(ManifestError::WrongType);
        }

        ManifestAsset asset;
        asset.name = entry["name"].get<std::string>();
        if (!detail::isBareFilename(asset.name)) {
            return R::err(ManifestError::InvalidAssetName);
        }

        asset.url = entry["url"].get<std::string>();
        if (!detail::isReleaseUrl(asset.url)) {
            return R::err(ManifestError::InsecureUrl);
        }

        if (!fromHex(entry["sha256"].get<std::string>(), asset.sha256)) {
            return R::err(ManifestError::InvalidDigest);
        }

        // is_number_unsigned() rejects a negative, a float and a bool in one
        // go - JSON `true` is not a number, and -1 would wrap catastrophically
        // into a free-space pre-flight.
        if (!entry["size"].is_number_unsigned()) {
            return R::err(ManifestError::InvalidSize);
        }
        asset.size = entry["size"].get<std::uint64_t>();
        if (asset.size == 0) {
            return R::err(ManifestError::InvalidSize);
        }

        // An unrecognised kind is skipped rather than rejected: that is what
        // lets a future release add an asset type without breaking this client.
        const std::optional<AssetKind> kind = detail::parseKind(entry["kind"].get<std::string>());
        if (!kind) {
            continue;
        }
        asset.kind = *kind;

        out.assets.push_back(std::move(asset));
    }

    if (out.findAsset(AssetKind::AppNro) == nullptr) {
        return R::err(ManifestError::NoAppAsset);
    }

    return R::ok(std::move(out));
}

std::string_view describe(ManifestError error)
{
    switch (error) {
        case ManifestError::NotJson:
            return "not valid JSON";
        case ManifestError::NotObject:
            return "top level is not an object";
        case ManifestError::UnsupportedSchemaVersion:
            return "manifest is newer than this version understands";
        case ManifestError::WrongProduct:
            return "manifest is for a different product";
        case ManifestError::MissingField:
            return "a required field is missing";
        case ManifestError::WrongType:
            return "a field has the wrong type";
        case ManifestError::InvalidVersion:
            return "a version field is not valid SemVer";
        case ManifestError::InvalidTag:
            return "tag does not match version";
        case ManifestError::InvalidTimestamp:
            return "published_at is not ISO-8601 UTC";
        case ManifestError::InvalidChannel:
            return "channel is not stable or beta";
        case ManifestError::NoAssets:
            return "no assets listed";
        case ManifestError::InvalidAssetName:
            return "an asset name is not a bare filename";
        case ManifestError::InsecureUrl:
            return "an asset URL is not an https GitHub release";
        case ManifestError::InvalidDigest:
            return "an asset digest is not 64 lowercase hex";
        case ManifestError::InvalidSize:
            return "an asset size is not a positive integer";
        case ManifestError::NoAppAsset:
            return "no application asset to download";
    }
    return "unknown error";
}

std::string_view toString(AssetKind kind)
{
    switch (kind) {
        case AssetKind::AppNro:
            return "app-nro";
        case AssetKind::ForwarderNro:
            return "forwarder-nro";
        case AssetKind::SdOverlayZip:
            return "sd-overlay-zip";
    }
    return "unknown";
}

}  // namespace nsx::core
