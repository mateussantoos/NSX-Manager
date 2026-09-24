// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/update/handoff.hpp"

#include <nlohmann/json.hpp>

namespace nsx::core {

namespace detail {
namespace {

using Json = nlohmann::json;

/// Absolute, and free of any parent reference.
///
/// Both halves matter. A relative path is the predecessor's bug. A `..` segment
/// would let a handoff - which is a file on a user-writable SD card - point the
/// swap at something outside the two directories this application owns.
bool isSafeAbsolutePath(std::string_view path)
{
    if (path.empty() || path.front() != '/') {
        return false;
    }
    if (path.find("..") != std::string_view::npos) {
        return false;
    }
    if (path.find('\\') != std::string_view::npos) {
        return false;
    }
    return true;
}

}  // namespace
}  // namespace detail

Result<Handoff, HandoffError> parseHandoff(std::string_view json)
{
    using R = Result<Handoff, HandoffError>;
    using Json = detail::Json;

    const Json doc = Json::parse(json, nullptr, false);
    if (doc.is_discarded()) {
        return R::err(HandoffError::NotJson);
    }
    if (!doc.is_object()) {
        return R::err(HandoffError::NotObject);
    }

    if (!doc.contains("schema_version")) {
        return R::err(HandoffError::MissingField);
    }
    if (!doc["schema_version"].is_number_integer()) {
        return R::err(HandoffError::WrongType);
    }
    if (doc["schema_version"].get<int>() != kSupportedHandoffSchema) {
        // A forwarder that does not understand the document refuses to act on
        // it and chainloads the existing target unchanged. It never guesses.
        return R::err(HandoffError::UnsupportedSchemaVersion);
    }

    Handoff out;
    out.schemaVersion = kSupportedHandoffSchema;

    for (const char* key :
         {"created_at_unix", "from_version", "to_version", "staged_nro", "staged_sha256",
          "target_nro", "backup_nro", "forwarder_nro", "attempts", "max_attempts"}) {
        if (!doc.contains(key)) {
            return R::err(HandoffError::MissingField);
        }
    }

    if (!doc["created_at_unix"].is_number_integer()) {
        return R::err(HandoffError::WrongType);
    }
    out.createdAtUnix = doc["created_at_unix"].get<std::int64_t>();

    for (const auto& [key, field] :
         {std::pair{"from_version", &out.fromVersion}, std::pair{"to_version", &out.toVersion}}) {
        if (!doc[key].is_string()) {
            return R::err(HandoffError::WrongType);
        }
        *field = doc[key].get<std::string>();
    }

    for (const auto& [key, field] :
         {std::pair{"staged_nro", &out.stagedNro}, std::pair{"target_nro", &out.targetNro},
          std::pair{"backup_nro", &out.backupNro}, std::pair{"forwarder_nro", &out.forwarderNro}}) {
        if (!doc[key].is_string()) {
            return R::err(HandoffError::WrongType);
        }
        *field = doc[key].get<std::string>();

        if (field->empty() || field->front() != '/') {
            return R::err(HandoffError::RelativePath);
        }
        if (!detail::isSafeAbsolutePath(*field)) {
            return R::err(HandoffError::PathTraversal);
        }
    }

    if (!doc["staged_sha256"].is_string()) {
        return R::err(HandoffError::WrongType);
    }
    if (!fromHex(doc["staged_sha256"].get<std::string>(), out.stagedSha256)) {
        return R::err(HandoffError::InvalidDigest);
    }

    if (!doc["attempts"].is_number_integer() || !doc["max_attempts"].is_number_integer()) {
        return R::err(HandoffError::WrongType);
    }
    out.attempts = doc["attempts"].get<int>();
    out.maxAttempts = doc["max_attempts"].get<int>();

    // A negative count, or a limit of zero, would make the retry budget
    // meaningless - and an unbounded retry of a failing swap is exactly the
    // crash loop the counter exists to stop.
    if (out.attempts < 0 || out.maxAttempts < 1 || out.attempts > 1000 || out.maxAttempts > 1000) {
        return R::err(HandoffError::InvalidAttempts);
    }

    return R::ok(std::move(out));
}

std::string serializeHandoff(const Handoff& handoff)
{
    detail::Json doc;
    doc["schema_version"] = handoff.schemaVersion;
    doc["created_at_unix"] = handoff.createdAtUnix;
    doc["from_version"] = handoff.fromVersion;
    doc["to_version"] = handoff.toVersion;
    doc["staged_nro"] = handoff.stagedNro;
    doc["staged_sha256"] = toHex(handoff.stagedSha256);
    doc["target_nro"] = handoff.targetNro;
    doc["backup_nro"] = handoff.backupNro;
    doc["forwarder_nro"] = handoff.forwarderNro;
    doc["attempts"] = handoff.attempts;
    doc["max_attempts"] = handoff.maxAttempts;
    return doc.dump(2) + "\n";
}

std::string_view describe(HandoffError error)
{
    switch (error) {
        case HandoffError::NotJson:
            return "handoff is not valid JSON";
        case HandoffError::NotObject:
            return "handoff top level is not an object";
        case HandoffError::UnsupportedSchemaVersion:
            return "handoff is newer than this build understands";
        case HandoffError::MissingField:
            return "handoff is missing a required field";
        case HandoffError::WrongType:
            return "a handoff field has the wrong type";
        case HandoffError::RelativePath:
            return "a handoff path is not absolute";
        case HandoffError::PathTraversal:
            return "a handoff path contains a parent reference";
        case HandoffError::InvalidDigest:
            return "the staged digest is not 64 lowercase hex";
        case HandoffError::InvalidAttempts:
            return "the attempt counters are out of range";
    }
    return "unknown handoff error";
}

RecoveryDecision decideRecovery(const std::optional<Handoff>& handoff, const FilePresence& present)
{
    RecoveryDecision out;

    // No handoff: either everything is normal, or the application is simply
    // gone and the user launched the repair entry deliberately.
    if (!handoff.has_value() || !present.handoff) {
        if (present.target) {
            out.action = RecoveryAction::Nothing;
            out.reason = "no update in flight";
        }
        else if (present.backup) {
            // A backup with no handoff means a previous swap got far enough to
            // move the target aside and then lost its instruction file.
            out.action = RecoveryAction::RestoreBackup;
            out.reason = "no handoff, but a backup exists and the application is missing";
        }
        else {
            out.action = RecoveryAction::Unrecoverable;
            out.reason = "the application is missing and there is nothing to restore";
        }
        return out;
    }

    const Handoff& h = *handoff;

    // Budget spent. Roll back rather than loop: this is the state a crashing
    // swap converges on, and retrying it forever is how a console ends up
    // unbootable.
    if (h.attemptsExhausted()) {
        if (present.backup) {
            out.action = RecoveryAction::RollBack;
            out.reason = "swap failed " + std::to_string(h.attempts) +
                         " time(s); restoring the previous version";
        }
        else if (present.target) {
            out.action = RecoveryAction::RollBack;
            out.reason = "swap failed " + std::to_string(h.attempts) +
                         " time(s); the application is intact, abandoning the update";
        }
        else if (present.staged) {
            out.action = RecoveryAction::InstallStaged;
            out.reason = "no application and no backup; installing the staged binary";
        }
        else {
            out.action = RecoveryAction::Unrecoverable;
            out.reason = "swap failed and nothing usable remains";
        }
        return out;
    }

    // The target vanished. This is the sub-millisecond window between the two
    // renames that FatFs forces on us, and the reason the backup exists at all.
    if (!present.target) {
        if (present.backup) {
            out.action = RecoveryAction::RestoreBackup;
            out.reason = "the application is missing; restoring the backup";
        }
        else if (present.staged) {
            out.action = RecoveryAction::InstallStaged;
            out.reason = "the application is missing; installing the staged binary";
        }
        else {
            out.action = RecoveryAction::Unrecoverable;
            out.reason = "the application is missing and nothing is staged";
        }
        return out;
    }

    if (!present.staged) {
        // The instruction survived but what it points at did not.
        out.action = RecoveryAction::Unrecoverable;
        out.reason = "the staged binary named by the handoff is missing";
        return out;
    }

    out.action = (h.attempts == 0) ? RecoveryAction::ProceedWithSwap : RecoveryAction::RetrySwap;
    out.reason = (h.attempts == 0)
                     ? ("installing " + h.toVersion)
                     : ("resuming an interrupted swap (attempt " + std::to_string(h.attempts + 1) +
                        " of " + std::to_string(h.maxAttempts) + ")");
    return out;
}

std::string_view describe(RecoveryAction action)
{
    switch (action) {
        case RecoveryAction::Nothing:
            return "nothing to do";
        case RecoveryAction::ProceedWithSwap:
            return "install the staged update";
        case RecoveryAction::RetrySwap:
            return "resume an interrupted update";
        case RecoveryAction::RollBack:
            return "roll back to the previous version";
        case RecoveryAction::RestoreBackup:
            return "restore the backup";
        case RecoveryAction::InstallStaged:
            return "install the staged binary";
        case RecoveryAction::Unrecoverable:
            return "cannot recover automatically";
    }
    return "unknown";
}

}  // namespace nsx::core
