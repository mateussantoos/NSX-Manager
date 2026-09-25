// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/domain/network/telemetry_service.hpp"

#include <doctest.h>

using namespace nsx::domain;
using namespace nsx::infra;

namespace {

class FakeDnsBlockChecker : public DnsBlockChecker
{
public:
    explicit FakeDnsBlockChecker(TelemetryStatus overallStatus) : m_status(overallStatus) {}

    [[nodiscard]] TelemetryReport checkEndpoints() const override
    {
        TelemetryReport rep;
        rep.overall = m_status;
        rep.conntest = m_status;
        rep.cdn = m_status;
        rep.detail = (m_status == TelemetryStatus::Protected)
                         ? "Nintendo telemetry endpoints are blocked (safe)"
                         : "Warning: Nintendo telemetry reachable";
        return rep;
    }

private:
    TelemetryStatus m_status;
};

}  // namespace

TEST_CASE("TelemetryService reports protection correctly with fake checker")
{
    SUBCASE("Protected status")
    {
        auto fake = std::make_unique<FakeDnsBlockChecker>(TelemetryStatus::Protected);
        TelemetryService svc(std::move(fake));

        const TelemetryReport rep = svc.checkProtection();
        CHECK(rep.overall == TelemetryStatus::Protected);
        CHECK(rep.conntest == TelemetryStatus::Protected);
        CHECK(rep.cdn == TelemetryStatus::Protected);
    }

    SUBCASE("Unshielded status")
    {
        auto fake = std::make_unique<FakeDnsBlockChecker>(TelemetryStatus::Unshielded);
        TelemetryService svc(std::move(fake));

        const TelemetryReport rep = svc.checkProtection();
        CHECK(rep.overall == TelemetryStatus::Unshielded);
        CHECK(rep.conntest == TelemetryStatus::Unshielded);
        CHECK(rep.cdn == TelemetryStatus::Unshielded);
    }
}
