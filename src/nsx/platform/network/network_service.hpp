// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <utility>

#include "nsx/platform/network/network_types.hpp"

namespace nsx::platform::network {

/// @brief Service interface providing native Horizon OS network telemetry.
class NetworkInterfaceService
{
public:
    virtual ~NetworkInterfaceService() = default;

    /// @brief Query the active connection details and Wi-Fi signal metrics.
    /// @return Current ConnectionInfo.
    [[nodiscard]] virtual ConnectionInfo queryConnectionInfo() const = 0;
};

/// @brief Default production implementation wrapping libnx nifm on Switch.
class DefaultNetworkService final : public NetworkInterfaceService
{
public:
    [[nodiscard]] ConnectionInfo queryConnectionInfo() const override;
};

/// @brief Mock implementation for host testing and controlled environments.
class MockNetworkService final : public NetworkInterfaceService
{
public:
    explicit MockNetworkService(ConnectionInfo info = {}) : m_info(std::move(info)) {}

    void setConnectionInfo(ConnectionInfo info) { m_info = std::move(info); }

    [[nodiscard]] ConnectionInfo queryConnectionInfo() const override { return m_info; }

private:
    ConnectionInfo m_info;
};

}  // namespace nsx::platform::network
