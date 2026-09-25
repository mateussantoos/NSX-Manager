// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <memory>
#include <string>

#include "nsx/infra/network/dns_block_checker.hpp"

namespace nsx::domain {

/// @brief High-level service providing telemetry protection status to the UI.
/// @since 0.4.0
class TelemetryService
{
public:
    explicit TelemetryService(std::unique_ptr<infra::DnsBlockChecker> checker =
                                  std::make_unique<infra::DnsBlockChecker>());

    [[nodiscard]] infra::TelemetryReport checkProtection() const;

private:
    std::unique_ptr<infra::DnsBlockChecker> m_checker;
};

}  // namespace nsx::domain
