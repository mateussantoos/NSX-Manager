// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "nsx/core/result/result.hpp"

namespace nsx::core {

/// @brief The merge marker schema version this build understands.
inline constexpr int kSupportedMergeMarkerSchema = 1;

/// @brief Why a merge marker was rejected.
/// @since 0.3.0
enum class MergeMarkerError
{
    NotJson,                   ///< Not parseable as JSON.
    NotObject,                 ///< Top level was not an object.
    UnsupportedSchemaVersion,  ///< Newer than this build understands.
    MissingField,              ///< A required field was absent.
    WrongType,                 ///< A field had the wrong JSON type.
    RelativePath,              ///< A path was not absolute.
    PathTraversal,             ///< A path contained `..`.
    InvalidId                  ///< The item id is empty or has odd characters.
};

/// @brief Written before a merge begins, deleted once it finishes.
///
/// @details A CFW pack is thousands of files and takes minutes. Its merge is
///          therefore the longest window in this application where the SD card
///          is half one thing and half another, and a console can be powered
///          off in the middle of it.
///
///          This file existing at startup means exactly one thing: a merge
///          started and did not finish. The three directories it names are
///          everything needed to undo it - the backup holds what was replaced,
///          the staging tree holds what was being installed, and their
///          difference is what was newly created and must be removed.
///
///          **Deliberately not a per-file journal.** Recording each file as it
///          moved would mean thousands of writes to the card during the merge,
///          and the information it would hold is already recoverable by walking
///          the two directories.
///
/// @see docs/architecture/cfw-install.md
/// @since 0.3.0
struct MergeMarker
{
    int schemaVersion{kSupportedMergeMarkerSchema};  ///< Always the supported version once parsed.
    std::int64_t createdAtUnix{};                    ///< When the merge began.
    std::string itemId;                              ///< The catalogue item being installed.
    std::string itemName;                            ///< Display name, for the recovery message.

    std::string stagingDir;       ///< The extracted tree waiting to be merged.
    std::string destinationRoot;  ///< Where it is being merged into.
    std::string backupDir;        ///< What was displaced, mirroring the destination.
};

/// @brief Parse a merge marker.
/// @param json The raw document.
/// @return The marker, or why it was rejected.
/// @note **Never throws.** Every path must be absolute and free of `..`, for
///       the same reason the handoff demands it: this file lives on a
///       user-writable card and names directories a recovery will delete from.
/// @since 0.3.0
[[nodiscard]] Result<MergeMarker, MergeMarkerError> parseMergeMarker(std::string_view json);

/// @brief Render a merge marker as JSON.
/// @param marker The marker to render.
/// @return A document that @ref parseMergeMarker accepts.
/// @since 0.3.0
[[nodiscard]] std::string serializeMergeMarker(const MergeMarker& marker);

/// @brief A human-readable reason for a rejection.
/// @param error The error to describe.
/// @return A short English description.
/// @since 0.3.0
[[nodiscard]] std::string_view describe(MergeMarkerError error);

/// @brief What a startup check should do about an unfinished merge.
/// @since 0.3.0
enum class MergeRecovery
{
    Nothing,       ///< No merge was in flight.
    RollBack,      ///< Restore the backup and remove what was newly written.
    CleanUpOnly,   ///< The marker is stale; nothing left to restore.
    Unrecoverable  ///< The marker is unreadable, or its directories are gone.
};

/// @brief Which of the directories a recovery needs still exist.
/// @since 0.3.0
struct MergePresence
{
    bool marker{};       ///< The marker file itself.
    bool staging{};      ///< The extracted tree.
    bool backup{};       ///< The displaced originals.
    bool destination{};  ///< The destination root.
};

/// @brief The decision, with a reason fit for a log line and for the UI.
/// @since 0.3.0
struct MergeRecoveryDecision
{
    MergeRecovery action{MergeRecovery::Nothing};  ///< What to do.
    std::string reason;                            ///< Why, in English.
};

/// @brief Decide what to do about an interrupted merge.
///
/// @details Pure, so every combination is reachable in a test. These states
///          only occur after a power cut during the longest write this
///          application performs, which makes them the hardest to produce
///          deliberately on hardware and the most expensive to get wrong.
///
/// @param marker The parsed marker, or nullopt when there is none or it was
///        unreadable.
/// @param present Which directories exist.
/// @return The action and why.
/// @since 0.3.0
[[nodiscard]] MergeRecoveryDecision decideMergeRecovery(const std::optional<MergeMarker>& marker,
                                                        const MergePresence& present);

/// @brief A short English description of a recovery action.
/// @param action The action to describe.
/// @return A description suitable for a log line.
/// @since 0.3.0
[[nodiscard]] std::string_view describe(MergeRecovery action);

}  // namespace nsx::core
