// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>

namespace nsx::domain::motd {

/// @brief Message of the Day model.
struct MessageOfTheDay
{
    std::string id{};       ///< Unique identifier for the bulletin.
    std::string title{};    ///< Title header.
    std::string content{};  ///< Markdown or plain-text announcement body.
};

/// @brief Service interface placeholder for fetching announcement bulletins.
class MotdService
{
public:
    virtual ~MotdService() = default;

    /// @brief Retrieve the latest cached or fetched bulletin.
    /// @return Current bulletin announcement.
    [[nodiscard]] virtual MessageOfTheDay currentBulletin() const = 0;
};

}  // namespace nsx::domain::motd
