// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>
#include <string_view>

namespace nsx::infra {

/// @brief Status of a telemetry endpoint check.
/// @since 0.4.0
enum class TelemetryStatus
{
    Protected,   ///< Blocked via DNS redirect (0.0.0.0, 127.0.0.1, or NXDOMAIN). Safe.
    Unshielded,  ///< Resolved to public IP. Unshielded telemetry risk!
    Offline      ///< Network interface is down or unreachable.
};

/// @brief Combined telemetry test report.
/// @since 0.4.0
struct TelemetryReport
{
    TelemetryStatus overall{TelemetryStatus::Protected};
    TelemetryStatus conntest{TelemetryStatus::Protected};
    TelemetryStatus cdn{TelemetryStatus::Protected};
    std::string detail;
};

/// @brief Checks DNS blocking for Nintendo telemetry and connection test endpoints.
/// @details Verifies conntest.nintendowifi.net and ctest.cdn.nintendo.net.
/// @since 0.4.0
class DnsBlockChecker
{
public:
    virtual ~DnsBlockChecker() = default;

    /// @brief Perform check against telemetry endpoints.
    /// @return Report summarizing status.
    [[nodiscard]] virtual TelemetryReport checkEndpoints() const;
};

}  // namespace nsx::infra
