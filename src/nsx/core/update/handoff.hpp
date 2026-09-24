// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "nsx/core/hash/sha256.hpp"
#include "nsx/core/result/result.hpp"

namespace nsx::core {

/// @brief The handoff schema version this build understands.
inline constexpr int kSupportedHandoffSchema = 1;

/// @brief Why a handoff document was rejected.
/// @since 0.2.0
enum class HandoffError
{
    NotJson,                   ///< Not parseable as JSON.
    NotObject,                 ///< Top level was not an object.
    UnsupportedSchemaVersion,  ///< Newer than this build understands.
    MissingField,              ///< A required field was absent.
    WrongType,                 ///< A field had the wrong JSON type.
    RelativePath,              ///< A path was not absolute.
    PathTraversal,             ///< A path contained `..`.
    InvalidDigest,             ///< Not 64 lowercase hex characters.
    InvalidAttempts            ///< Negative, or a limit that makes no sense.
};

/// @brief The instruction the application leaves for the forwarder.
///
/// @details Written by the application after a download verifies, read by the
///          forwarder after chainload. **One implementation, linked into both
///          binaries** - the predecessor had two divergent ad-hoc `KEY=value`
///          parsers for the same file (`utils.cpp:539-575` wrote it,
///          `app-forwarder/source/main.cpp:26-46` read it) and nothing kept
///          them agreeing.
///
/// @see docs/reference/handoff-format.md, ADR-0007
/// @since 0.2.0
struct Handoff
{
    int schemaVersion{kSupportedHandoffSchema};  ///< Always the supported version once parsed.
    std::int64_t createdAtUnix{};                ///< Written by the app; used for staleness.
    std::string fromVersion;                     ///< The version being replaced.
    std::string toVersion;                       ///< The version being installed.

    std::string stagedNro;          ///< The verified new binary.
    Sha256::Digest stagedSha256{};  ///< Re-checked by the forwarder before the swap.
    std::string targetNro;          ///< Where it must end up.
    std::string backupNro;          ///< Where the outgoing binary is kept.
    std::string forwarderNro;       ///< The forwarder's own location.

    int attempts{};      ///< Incremented before each attempt.
    int maxAttempts{3};  ///< Reaching this triggers rollback.

    /// @brief Whether the retry budget is spent.
    /// @return True when another attempt must not be made.
    [[nodiscard]] bool attemptsExhausted() const { return attempts >= maxAttempts; }
};

/// @brief Parse a handoff document.
///
/// @details Every path must be **absolute** and free of `..`. This is not
///          defensive padding: the predecessor opened `"forwarder.conf"`
///          relative to the working directory
///          (`app-forwarder/source/main.cpp:28`), which worked only because
///          hbmenu happens to `chdir`, and on failure produced empty strings
///          that were then passed to `rename()` and `remove()`.
///          `tests/fixtures/handoff/relative-path.json` pins that behaviour.
///
///          **Never throws.**
///
/// @param json The raw document.
/// @return The parsed handoff, or the reason it was rejected.
/// @since 0.2.0
[[nodiscard]] Result<Handoff, HandoffError> parseHandoff(std::string_view json);

/// @brief Render a handoff back to JSON.
/// @param handoff The handoff to render.
/// @return A document that @ref parseHandoff accepts.
/// @since 0.2.0
[[nodiscard]] std::string serializeHandoff(const Handoff& handoff);

/// @brief A human-readable reason for a rejection.
/// @param error The error to describe.
/// @return A short English description.
/// @since 0.2.0
[[nodiscard]] std::string_view describe(HandoffError error);

/// @brief Which of the files involved in a swap currently exist.
/// @details Supplied by the caller so the decision itself stays pure and can be
///          tested exhaustively without a filesystem.
/// @since 0.2.0
struct FilePresence
{
    bool handoff{};  ///< The handoff document itself exists.
    bool staged{};   ///< The staged new binary exists.
    bool target{};   ///< The application binary exists at its final path.
    bool backup{};   ///< A backup of the outgoing binary exists.
};

/// @brief What the forwarder should do on startup.
/// @since 0.2.0
enum class RecoveryAction
{
    Nothing,          ///< No update in flight; just chainload the target.
    ProceedWithSwap,  ///< Staged and untried: perform the swap.
    RetrySwap,        ///< Interrupted mid-swap, budget remains.
    RollBack,         ///< Budget spent: restore the backup and report failure.
    RestoreBackup,    ///< Target vanished between the two renames.
    InstallStaged,    ///< No target and no backup; the staged file is all we have.
    Unrecoverable     ///< Nothing usable left. Direct the user to the zip.
};

/// @brief The decision, with a reason fit for a log line and for the UI.
/// @since 0.2.0
struct RecoveryDecision
{
    RecoveryAction action{RecoveryAction::Nothing};  ///< What the forwarder should do.
    std::string reason;                              ///< Why, in English.

    /// @brief Whether this decision writes to the filesystem.
    /// @return True for any action that moves a file.
    [[nodiscard]] bool mutatesFilesystem() const
    {
        return action != RecoveryAction::Nothing && action != RecoveryAction::Unrecoverable;
    }
};

/// @brief Decide what to do from the handoff and what exists on disk.
///
/// @details Implements the recovery table in
///          `docs/architecture/self-update-forwarder.md` exactly. Pure, so
///          every row is testable without a console - which matters because
///          these are the states that only occur after a crash or a power cut,
///          and are therefore the hardest to reach deliberately on hardware.
///
/// @param handoff The parsed handoff, or nullopt when there is none (or it was
///        unreadable).
/// @param present Which files exist.
/// @return The action and why.
/// @since 0.2.0
[[nodiscard]] RecoveryDecision decideRecovery(const std::optional<Handoff>& handoff,
                                              const FilePresence& present);

/// @brief A short English description of a recovery action.
/// @param action The action to describe.
/// @return A description suitable for a log line.
/// @since 0.2.0
[[nodiscard]] std::string_view describe(RecoveryAction action);

}  // namespace nsx::core
