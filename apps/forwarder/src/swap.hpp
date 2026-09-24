// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>
#include <string_view>

#include "nsx/core/update/handoff.hpp"

/// @brief The forwarder's filesystem side.
/// @details Device-only: everything here touches the SD card. The decisions it
///          acts on come from @c nsx::core::decideRecovery, which is pure and
///          host-tested.
namespace nsx::forwarder {

/// @brief What happened when the forwarder tried to act.
/// @since 0.2.0
enum class SwapResult
{
    NothingToDo,     ///< No update in flight.
    Installed,       ///< The new binary is in place.
    RolledBack,      ///< The previous version was restored.
    Restored,        ///< A backup was put back after an interrupted swap.
    DigestMismatch,  ///< The staged binary did not match its recorded digest.
    IoFailed,        ///< A copy, rename or fsync failed.
    Unrecoverable    ///< Nothing usable remains; the user must reinstall.
};

/// @brief Outcome plus a line for the log and the screen.
/// @since 0.2.0
struct SwapOutcome
{
    SwapResult result{SwapResult::NothingToDo};  ///< What happened.
    std::string detail;                          ///< One line for the log and the screen.

    /// @brief Whether the application binary is present and usable afterwards.
    /// @return True unless the installation needs manual repair.
    [[nodiscard]] bool applicationUsable() const { return result != SwapResult::Unrecoverable; }
};

/// @brief Read the handoff, decide, and act.
///
/// @details The ordering is the contract, and it is chosen so the target is
///          never absent for longer than a single rename:
///
///          1. parse the handoff; an exhausted budget takes the rollback branch;
///          2. increment `attempts` and rewrite the handoff **atomically**,
///             before anything risky - otherwise a crash inside step 3 loops
///             forever;
///          3. re-verify the staged digest, catching corruption between the
///             download and now;
///          4. copy staged to `target.new`, fsync;
///          5. rename target to backup;
///          6. rename `target.new` to target;
///          7. verify, then delete backup, staged and handoff.
///
///          Steps 5 and 6 are two renames because FatFs cannot rename onto an
///          existing file. The window between them is what the backup and the
///          boot-time recovery exist to cover.
///
/// @param handoffPath Absolute path to the handoff document.
/// @return What happened, and why.
/// @since 0.2.0
[[nodiscard]] SwapOutcome runSwap(const std::string& handoffPath);

/// @brief A short English description of an outcome.
/// @param result The result to describe.
/// @return Text for the screen.
/// @since 0.2.0
[[nodiscard]] std::string_view describe(SwapResult result);

}  // namespace nsx::forwarder
