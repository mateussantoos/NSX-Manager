// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/infra/network/dns_block_checker.hpp"

#include <cstdint>
#include <cstring>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace nsx::infra {

namespace {

TelemetryStatus checkHost(const char* hostname)
{
    struct addrinfo hints
    {
    };

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    const int ret = ::getaddrinfo(hostname, nullptr, &hints, &res);

    if (ret != 0 || res == nullptr) {
        // Non-resolvable (NXDOMAIN, EAI_NONAME, EAI_NODATA, etc.) - blocked by DNS.
        return TelemetryStatus::Protected;
    }

    bool allBlocked = true;
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            const auto* sin = reinterpret_cast<const struct sockaddr_in*>(p->ai_addr);
            const std::uint32_t ip = ntohl(sin->sin_addr.s_addr);
            const bool isNull = (ip == 0);
            const bool isLoopback = ((ip & 0xFF000000U) == 0x7F000000U);
            if (!isNull && !isLoopback) {
                allBlocked = false;
                break;
            }
        }
    }

    ::freeaddrinfo(res);
    return allBlocked ? TelemetryStatus::Protected : TelemetryStatus::Unshielded;
}

}  // namespace

TelemetryReport DnsBlockChecker::checkEndpoints() const
{
    TelemetryReport report;
    report.conntest = checkHost("conntest.nintendowifi.net");
    report.cdn = checkHost("ctest.cdn.nintendo.net");

    if (report.conntest == TelemetryStatus::Protected && report.cdn == TelemetryStatus::Protected) {
        report.overall = TelemetryStatus::Protected;
        report.detail = "Nintendo telemetry endpoints are blocked (90DNS / Hosts active)";
    }
    else {
        report.overall = TelemetryStatus::Unshielded;
        report.detail = "Warning: Nintendo telemetry is reachable! Unshielded connection.";
    }

    return report;
}

}  // namespace nsx::infra
