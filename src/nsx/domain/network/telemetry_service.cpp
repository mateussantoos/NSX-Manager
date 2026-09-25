// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/domain/network/telemetry_service.hpp"

#include <utility>

namespace nsx::domain {

TelemetryService::TelemetryService(std::unique_ptr<infra::DnsBlockChecker> checker)
    : m_checker(std::move(checker))
{
    if (!m_checker) {
        m_checker = std::make_unique<infra::DnsBlockChecker>();
    }
}

infra::TelemetryReport TelemetryService::checkProtection() const
{
    return m_checker->checkEndpoints();
}

}  // namespace nsx::domain
