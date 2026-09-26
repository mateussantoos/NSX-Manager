// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>

namespace nsx::platform::network {

/// @brief Physical connection medium.
enum class ConnectionMedium
{
    None,
    Ethernet,
    Wifi
};

/// @brief Network interface status.
enum class InterfaceStatus
{
    Disconnected,
    Connecting,
    Connected
};

/// @brief Comprehensive network telemetry and interface information.
struct ConnectionInfo
{
    bool isConnected{false};
    ConnectionMedium medium{ConnectionMedium::None};
    std::string ipAddress{};
    std::string subnetMask{};
    std::string gateway{};
    std::string ssid{};
    int wifiSignalBars{0};     // 0 to 3 bars
    int wifiSignalPercent{0};  // 0 to 100%
};

}  // namespace nsx::platform::network
