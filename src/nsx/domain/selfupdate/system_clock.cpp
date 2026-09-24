// SPDX-License-Identifier: GPL-3.0-only

#include <ctime>

#include "nsx/domain/ports/ports.hpp"

namespace nsx::domain {

std::int64_t SystemClock::nowUnix() const
{
    const std::time_t now = std::time(nullptr);
    if (now == static_cast<std::time_t>(-1)) {
        // No clock at all. Zero is honest: every freshness rule reads it as
        // "epoch", which makes caches stale and backoff deadlines expired -
        // the conservative direction, since it costs a request rather than
        // suppressing one.
        return 0;
    }
    return static_cast<std::int64_t>(now);
}

}  // namespace nsx::domain
