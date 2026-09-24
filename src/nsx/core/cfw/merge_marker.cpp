// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/cfw/merge_marker.hpp"

#include <nlohmann/json.hpp>

namespace nsx::core {

namespace detail {
namespace {

using Json = nlohmann::json;

/// Absolute, and free of any parent reference. A recovery DELETES from the
/// directories this file names, so a `..` here would let a file on a
/// user-writable card aim that deletion somewhere else.
bool isSafeAbsolutePath(std::string_view path)
{
    if (path.empty() || path.front() != '/') {
        return false;
    }
    if (path.find("..") != std::string_view::npos) {
        return false;
    }
    return path.find('\\') == std::string_view::npos;
}

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
    return id.front() != '.';
}

}  // namespace
}  // namespace detail

Result<MergeMarker, MergeMarkerError> parseMergeMarker(std::string_view json)
{
    using R = Result<MergeMarker, MergeMarkerError>;
    using Json = detail::Json;

    const Json doc = Json::parse(json, nullptr, false);
    if (doc.is_discarded()) {
        return R::err(MergeMarkerError::NotJson);
    }
    if (!doc.is_object()) {
        return R::err(MergeMarkerError::NotObject);
    }

    if (!doc.contains("schema_version")) {
        return R::err(MergeMarkerError::MissingField);
    }
    if (!doc["schema_version"].is_number_integer()) {
        return R::err(MergeMarkerError::WrongType);
    }
    if (doc["schema_version"].get<int>() != kSupportedMergeMarkerSchema) {
        return R::err(MergeMarkerError::UnsupportedSchemaVersion);
    }

    for (const char* key :
         {"created_at_unix", "item_id", "staging_dir", "destination_root", "backup_dir"}) {
        if (!doc.contains(key)) {
            return R::err(MergeMarkerError::MissingField);
        }
    }

    MergeMarker out;
    out.schemaVersion = kSupportedMergeMarkerSchema;

    if (!doc["created_at_unix"].is_number_integer()) {
        return R::err(MergeMarkerError::WrongType);
    }
    out.createdAtUnix = doc["created_at_unix"].get<std::int64_t>();

    if (!doc["item_id"].is_string()) {
        return R::err(MergeMarkerError::WrongType);
    }
    out.itemId = doc["item_id"].get<std::string>();
    if (!detail::isValidId(out.itemId)) {
        return R::err(MergeMarkerError::InvalidId);
    }

    if (doc.contains("item_name")) {
        if (!doc["item_name"].is_string()) {
            return R::err(MergeMarkerError::WrongType);
        }
        out.itemName = doc["item_name"].get<std::string>();
    }

    for (const auto& [key, field] : {std::pair{"staging_dir", &out.stagingDir},
                                     std::pair{"destination_root", &out.destinationRoot},
                                     std::pair{"backup_dir", &out.backupDir}}) {
        if (!doc[key].is_string()) {
            return R::err(MergeMarkerError::WrongType);
        }
        *field = doc[key].get<std::string>();

        if (field->empty() || field->front() != '/') {
            return R::err(MergeMarkerError::RelativePath);
        }
        if (!detail::isSafeAbsolutePath(*field)) {
            return R::err(MergeMarkerError::PathTraversal);
        }
    }

    return R::ok(std::move(out));
}

std::string serializeMergeMarker(const MergeMarker& marker)
{
    detail::Json doc;
    doc["schema_version"] = marker.schemaVersion;
    doc["created_at_unix"] = marker.createdAtUnix;
    doc["item_id"] = marker.itemId;
    doc["item_name"] = marker.itemName;
    doc["staging_dir"] = marker.stagingDir;
    doc["destination_root"] = marker.destinationRoot;
    doc["backup_dir"] = marker.backupDir;
    return doc.dump(2) + "\n";
}

std::string_view describe(MergeMarkerError error)
{
    switch (error) {
        case MergeMarkerError::NotJson:
            return "the merge marker is not valid JSON";
        case MergeMarkerError::NotObject:
            return "the merge marker top level is not an object";
        case MergeMarkerError::UnsupportedSchemaVersion:
            return "the merge marker is newer than this version understands";
        case MergeMarkerError::MissingField:
            return "the merge marker is missing a required field";
        case MergeMarkerError::WrongType:
            return "a merge marker field has the wrong type";
        case MergeMarkerError::RelativePath:
            return "a merge marker path is not absolute";
        case MergeMarkerError::PathTraversal:
            return "a merge marker path contains a parent reference";
        case MergeMarkerError::InvalidId:
            return "the merge marker item id is empty or malformed";
    }
    return "unknown merge marker error";
}

MergeRecoveryDecision decideMergeRecovery(const std::optional<MergeMarker>& marker,
                                          const MergePresence& present)
{
    MergeRecoveryDecision out;

    if (!present.marker) {
        out.action = MergeRecovery::Nothing;
        out.reason = "no merge was in flight";
        return out;
    }

    if (!marker.has_value()) {
        // The marker is there but unreadable. Something WAS in flight and we
        // cannot tell what, so guessing at directories to delete from is the
        // one thing not to do.
        out.action = MergeRecovery::Unrecoverable;
        out.reason = "a merge was interrupted but its marker could not be read";
        return out;
    }

    if (present.backup) {
        out.action = MergeRecovery::RollBack;
        out.reason = "restoring what '" +
                     (marker->itemName.empty() ? marker->itemId : marker->itemName) + "' replaced";
        return out;
    }

    if (present.staging) {
        // A marker and a staged tree but no backup: the merge was interrupted
        // before it displaced anything, so there is nothing to put back and the
        // destination is untouched.
        out.action = MergeRecovery::CleanUpOnly;
        out.reason = "the merge stopped before it changed anything; clearing the staging area";
        return out;
    }

    out.action = MergeRecovery::CleanUpOnly;
    out.reason = "nothing of the interrupted merge remains; clearing the marker";
    return out;
}

std::string_view describe(MergeRecovery action)
{
    switch (action) {
        case MergeRecovery::Nothing:
            return "nothing to do";
        case MergeRecovery::RollBack:
            return "undo an interrupted installation";
        case MergeRecovery::CleanUpOnly:
            return "clear an abandoned staging area";
        case MergeRecovery::Unrecoverable:
            return "an interrupted installation cannot be undone automatically";
    }
    return "unknown";
}

}  // namespace nsx::core
