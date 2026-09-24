// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/catalog/catalog.hpp"

#include <optional>

#include <nlohmann/json.hpp>

namespace nsx::core {

namespace detail {
namespace {

using Json = nlohmann::json;

std::optional<ContentKind> parseKind(std::string_view text)
{
    if (text == "cfw-pack") {
        return ContentKind::CfwPack;
    }
    if (text == "firmware") {
        return ContentKind::Firmware;
    }
    if (text == "tool") {
        return ContentKind::Tool;
    }
    if (text == "cheats") {
        return ContentKind::Cheats;
    }
    if (text == "translation") {
        return ContentKind::Translation;
    }
    if (text == "theme") {
        return ContentKind::Theme;
    }
    return std::nullopt;
}

std::optional<InstallTarget> parseTarget(std::string_view text)
{
    if (text == "sd-root") {
        return InstallTarget::SdRoot;
    }
    if (text == "atmosphere-contents") {
        return InstallTarget::AtmosphereContents;
    }
    if (text == "bootloader") {
        return InstallTarget::Bootloader;
    }
    if (text == "firmware-staging") {
        return InstallTarget::FirmwareStaging;
    }
    if (text == "switch-apps") {
        return InstallTarget::SwitchApps;
    }
    if (text == "themes") {
        return InstallTarget::Themes;
    }
    return std::nullopt;
}

/// Identifiers end up in log lines, in settings files and potentially in paths.
/// Restricting them to an unambiguous alphabet now is cheaper than auditing
/// every future use of one.
bool isValidId(std::string_view id)
{
    if (id.empty() || id.size() > 64) {
        return false;
    }
    for (const char c : id) {
        const bool lower = c >= 'a' && c <= 'z';
        const bool digit = c >= '0' && c <= '9';
        if (!lower && !digit && c != '-' && c != '_' && c != '.') {
            return false;
        }
    }
    // A leading dot would make it a hidden file if it ever reached a path.
    return id.front() != '.';
}

bool isHttpsUrl(std::string_view url)
{
    constexpr std::string_view kPrefix = "https://";
    return url.size() > kPrefix.size() && url.compare(0, kPrefix.size(), kPrefix) == 0;
}

}  // namespace
}  // namespace detail

std::string_view toString(ContentKind kind)
{
    switch (kind) {
        case ContentKind::CfwPack:
            return "cfw-pack";
        case ContentKind::Firmware:
            return "firmware";
        case ContentKind::Tool:
            return "tool";
        case ContentKind::Cheats:
            return "cheats";
        case ContentKind::Translation:
            return "translation";
        case ContentKind::Theme:
            return "theme";
    }
    return "unknown";
}

std::string_view toString(InstallTarget target)
{
    switch (target) {
        case InstallTarget::SdRoot:
            return "sd-root";
        case InstallTarget::AtmosphereContents:
            return "atmosphere-contents";
        case InstallTarget::Bootloader:
            return "bootloader";
        case InstallTarget::FirmwareStaging:
            return "firmware-staging";
        case InstallTarget::SwitchApps:
            return "switch-apps";
        case InstallTarget::Themes:
            return "themes";
    }
    return "unknown";
}

std::string_view describe(CatalogError error)
{
    switch (error) {
        case CatalogError::NotJson:
            return "the catalogue is not valid JSON";
        case CatalogError::NotObject:
            return "the catalogue top level is not an object";
        case CatalogError::UnsupportedSchemaVersion:
            return "the catalogue is newer than this version understands";
        case CatalogError::MissingField:
            return "a required catalogue field is missing";
        case CatalogError::WrongType:
            return "a catalogue field has the wrong type";
        case CatalogError::InvalidId:
            return "an item id is empty or contains unexpected characters";
        case CatalogError::DuplicateId:
            return "two catalogue items share an id";
        case CatalogError::InsecureUrl:
            return "an item url is not https";
        case CatalogError::InvalidDigest:
            return "an item digest is not 64 lowercase hex";
        case CatalogError::InvalidSize:
            return "an item size is not a positive integer";
        case CatalogError::NoItems:
            return "the catalogue contains nothing this version can install";
    }
    return "unknown catalogue error";
}

std::vector<const CatalogItem*> Catalog::ofKind(ContentKind kind) const
{
    std::vector<const CatalogItem*> out;
    for (const CatalogItem& item : items) {
        if (item.kind == kind) {
            out.push_back(&item);
        }
    }
    return out;
}

const CatalogItem* Catalog::find(std::string_view id) const
{
    for (const CatalogItem& item : items) {
        if (item.id == id) {
            return &item;
        }
    }
    return nullptr;
}

Result<Catalog, CatalogError> parseCatalog(std::string_view json)
{
    using R = Result<Catalog, CatalogError>;
    using Json = detail::Json;

    const Json doc = Json::parse(json, nullptr, false);
    if (doc.is_discarded()) {
        return R::err(CatalogError::NotJson);
    }
    if (!doc.is_object()) {
        return R::err(CatalogError::NotObject);
    }

    if (!doc.contains("schema_version")) {
        return R::err(CatalogError::MissingField);
    }
    if (!doc["schema_version"].is_number_integer()) {
        return R::err(CatalogError::WrongType);
    }
    if (doc["schema_version"].get<int>() != kSupportedCatalogSchema) {
        return R::err(CatalogError::UnsupportedSchemaVersion);
    }

    Catalog out;
    out.schemaVersion = kSupportedCatalogSchema;

    if (doc.contains("updated_at")) {
        if (!doc["updated_at"].is_string()) {
            return R::err(CatalogError::WrongType);
        }
        out.updatedAt = doc["updated_at"].get<std::string>();
    }

    if (!doc.contains("items")) {
        return R::err(CatalogError::MissingField);
    }
    if (!doc["items"].is_array()) {
        return R::err(CatalogError::WrongType);
    }

    for (const Json& entry : doc["items"]) {
        if (!entry.is_object()) {
            return R::err(CatalogError::WrongType);
        }

        // An unknown kind or destination drops THIS item and no other. A
        // catalogue that grows a content type must not cost an older client
        // everything else in the file.
        if (!entry.contains("kind") || !entry["kind"].is_string()) {
            return R::err(entry.contains("kind") ? CatalogError::WrongType
                                                 : CatalogError::MissingField);
        }
        const std::optional<ContentKind> kind = detail::parseKind(entry["kind"].get<std::string>());
        if (!kind.has_value()) {
            continue;
        }

        if (!entry.contains("target") || !entry["target"].is_string()) {
            return R::err(entry.contains("target") ? CatalogError::WrongType
                                                   : CatalogError::MissingField);
        }
        const std::optional<InstallTarget> target =
            detail::parseTarget(entry["target"].get<std::string>());
        if (!target.has_value()) {
            continue;
        }

        for (const char* key : {"id", "name", "url", "size", "sha256"}) {
            if (!entry.contains(key)) {
                return R::err(CatalogError::MissingField);
            }
        }
        if (!entry["id"].is_string() || !entry["name"].is_string() || !entry["url"].is_string() ||
            !entry["sha256"].is_string()) {
            return R::err(CatalogError::WrongType);
        }

        CatalogItem item;
        item.kind = *kind;
        item.target = *target;

        item.id = entry["id"].get<std::string>();
        if (!detail::isValidId(item.id)) {
            return R::err(CatalogError::InvalidId);
        }
        if (out.find(item.id) != nullptr) {
            // Silently taking the first or the last would make which one wins
            // depend on file order, and a catalogue with two entries claiming
            // one id is wrong in a way worth reporting.
            return R::err(CatalogError::DuplicateId);
        }

        item.name = entry["name"].get<std::string>();

        item.url = entry["url"].get<std::string>();
        if (!detail::isHttpsUrl(item.url)) {
            return R::err(CatalogError::InsecureUrl);
        }

        if (!entry["size"].is_number_unsigned()) {
            return R::err(CatalogError::InvalidSize);
        }
        item.size = entry["size"].get<std::uint64_t>();
        if (item.size == 0) {
            return R::err(CatalogError::InvalidSize);
        }

        if (!fromHex(entry["sha256"].get<std::string>(), item.sha256)) {
            return R::err(CatalogError::InvalidDigest);
        }

        if (entry.contains("summary")) {
            if (!entry["summary"].is_string()) {
                return R::err(CatalogError::WrongType);
            }
            item.summary = entry["summary"].get<std::string>();
        }

        if (entry.contains("version")) {
            if (!entry["version"].is_string()) {
                return R::err(CatalogError::WrongType);
            }
            item.versionText = entry["version"].get<std::string>();
            // Lenient: upstream projects tag things like "4.2.1-nsx" and
            // "2026.09". A version we cannot parse is still worth displaying,
            // so this never rejects - it only gives up on ordering.
            item.version = tryParseSemVer(item.versionText);
        }

        if (entry.contains("min_atmosphere")) {
            if (!entry["min_atmosphere"].is_string()) {
                return R::err(CatalogError::WrongType);
            }
            item.minAtmosphere = tryParseSemVer(entry["min_atmosphere"].get<std::string>());
        }

        if (entry.contains("notes_url")) {
            if (!entry["notes_url"].is_string()) {
                return R::err(CatalogError::WrongType);
            }
            item.notesUrl = entry["notes_url"].get<std::string>();
            if (!item.notesUrl.empty() && !detail::isHttpsUrl(item.notesUrl)) {
                return R::err(CatalogError::InsecureUrl);
            }
        }

        if (entry.contains("honour_preserve_rules")) {
            if (!entry["honour_preserve_rules"].is_boolean()) {
                return R::err(CatalogError::WrongType);
            }
            item.honourPreserveRules = entry["honour_preserve_rules"].get<bool>();
        }
        else {
            // A CFW pack extracts over the user's whole card, so their
            // preserve.txt applies unless the catalogue says otherwise.
            // Everything else lands somewhere self-contained.
            item.honourPreserveRules = *kind == ContentKind::CfwPack;
        }

        out.items.push_back(std::move(item));
    }

    if (out.items.empty()) {
        return R::err(CatalogError::NoItems);
    }

    return R::ok(std::move(out));
}

}  // namespace nsx::core
