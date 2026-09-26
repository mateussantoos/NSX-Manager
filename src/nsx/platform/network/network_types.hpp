// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>

namespace nsx::platform::network {

/// @brief Network interface status placeholder for connectivity extensions.
enum class InterfaceStatus
{
    Disconnected,
    Connecting,
    Connected
};

/// @brief Connection details representation.
struct ConnectionInfo
{
    bool isConnected{false};  ///< Whether an active interface is up.
    std::string ipAddress{};  ///< Local IP address string.
};

}  // namespace nsx::platform::network
