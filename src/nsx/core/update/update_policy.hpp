// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <optional>
#include <string>

#include "nsx/core/update/manifest.hpp"
#include "nsx/core/version/semver.hpp"

namespace nsx::core {

/// @brief What the application should do about an available release.
/// @since 0.2.0
enum class UpdateAction
{
    UpToDate,           ///< Nothing to do.
    Optional,           ///< Offer it; the user may decline.
    Mandatory,          ///< Block the main menu until the user updates.
    UnsupportedPath,    ///< Too old to update in-app; direct to the zip.
    WrongChannel,       ///< The manifest is for a channel we are not on.
    ManifestUnreadable  ///< No usable manifest; say so rather than "up to date".
};

/// @brief The decision, with a reason fit for a log line.
/// @since 0.2.0
struct UpdateDecision
{
    UpdateAction action{UpdateAction::ManifestUnreadable};
    std::optional<SemVer> target;  ///< The offered version, when there is one.
    std::string reason;            ///< Why, in English.

    /// @brief Whether the user should be shown an update prompt.
    /// @return True for an optional or mandatory update.
    [[nodiscard]] bool offersUpdate() const
    {
        return action == UpdateAction::Optional || action == UpdateAction::Mandatory;
    }
};

/// @brief Decide what to do, given what is installed and what is published.
///
/// @details Separated from both parsing and the UI so the rule can be tested
///          exhaustively without a network or a console. The ordering of the
///          checks is itself part of the contract:
///
///          1. channel mismatch - a stable client ignores a beta manifest
///             entirely, before it even looks at versions;
///          2. `min_supported` - too old to update safely in-app;
///          3. not newer - up to date;
///          4. `mandatory` - the publisher's per-release decision.
///
///          Replaces the predecessor's behaviour on two counts. It compared
///          versions by stripping non-digits (`main_frame.cpp:53-73`), and it
///          hid all ten tabs whenever any newer tag existed
///          (`main_frame.cpp:94-133`) - an unconditional lockout compiled into
///          the binary rather than a choice made per release.
///
/// @param installed The running version.
/// @param manifest The parsed manifest.
/// @param channel The channel this client is configured for.
/// @return The decision and the reason for it.
/// @since 0.2.0
[[nodiscard]] UpdateDecision decideUpdate(const SemVer& installed, const UpdateManifest& manifest,
                                          Channel channel = Channel::Stable);

/// @brief The decision to take when no manifest could be read.
/// @param why What went wrong, for the log and the UI.
/// @return A decision whose action is @ref UpdateAction::ManifestUnreadable.
/// @note Exists so an unreadable manifest can never be mistaken for "you are up
///       to date" - the predecessor swallowed a 404 into an empty string
///       (`download.cpp:493-500`) and reported exactly that for its entire life.
/// @since 0.2.0
[[nodiscard]] UpdateDecision unreadableManifest(std::string_view why);

/// @brief A short English description of an action.
/// @param action The action to describe.
/// @return A description suitable for a log line.
/// @since 0.2.0
[[nodiscard]] std::string_view describe(UpdateAction action);

}  // namespace nsx::core
