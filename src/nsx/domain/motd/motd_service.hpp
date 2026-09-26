// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace nsx::domain::motd {

/// @brief Message of the Day model parsed from the remote update manifest.
struct MessageOfTheDay
{
    std::string id{};              ///< Unique identifier for the bulletin.
    std::string severity{"info"};  ///< "info", "warning", or "critical".
    std::string title{};           ///< Bulletin header title.
    std::string message{};         ///< Announcement body text.
    std::string minAppVersion{};   ///< Optional minimum application version required.
};

/// @brief Service managing dynamic community bulletins, caching, and dismissal states.
class MotdService
{
public:
    /// @brief Construct service with current application version for version gating.
    /// @param currentAppVersion Installed application version.
    explicit MotdService(std::string currentAppVersion = "");

    virtual ~MotdService() = default;

    /// @brief Parse the optional "motd" payload from raw manifest JSON and cache it.
    /// @param jsonManifest Content of update.json.
    /// @return True if a valid bulletin was parsed.
    bool parseAndCache(std::string_view jsonManifest);

    /// @brief Retrieve the current active bulletin, or nullopt if none or dismissed.
    [[nodiscard]] std::optional<MessageOfTheDay> currentBulletin() const;

    /// @brief Check if a specific bulletin ID was marked as dismissed.
    [[nodiscard]] bool isDismissed(std::string_view bulletinId) const;

    /// @brief Dismiss a bulletin by ID so it is suppressed in the UI.
    void dismiss(std::string_view bulletinId);

    /// @brief Reset all dismissal records.
    void resetDismissals();

private:
    std::string m_appVersion;
    std::optional<MessageOfTheDay> m_cachedBulletin;
    std::unordered_set<std::string> m_dismissedIds;
};

}  // namespace nsx::domain::motd
