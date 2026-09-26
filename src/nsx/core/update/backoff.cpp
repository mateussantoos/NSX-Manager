// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/update/backoff.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

namespace nsx::core {

namespace detail {
namespace {

using Json = nlohmann::json;

/// SplitMix64. Chosen because the jitter has to be reproducible from a seed for
/// the tests to assert exact values, and because a real generator would drag
/// <random> and a global into a module that is otherwise pure arithmetic.
std::uint64_t mix(std::uint64_t seed)
{
    std::uint64_t z = seed + 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30u)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27u)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31u);
}

/// Double `base` `steps` times without overflowing, stopping at `cap`.
///
/// A naive `base << steps` is undefined once steps reaches 64, and wraps to a
/// small number well before that - which would silently turn a long backoff
/// into an immediate retry. Growing by doubling and breaking at the cap has no
/// such edge.
std::int64_t growCapped(std::int64_t base, int steps, std::int64_t cap)
{
    std::int64_t value = base;
    for (int i = 0; i < steps; ++i) {
        if (value >= cap - value) {
            return cap;
        }
        value *= 2;
    }
    return value > cap ? cap : value;
}

}  // namespace
}  // namespace detail

std::int64_t backoffDelaySeconds(int consecutiveFailures, const BackoffPolicy& policy,
                                 std::uint64_t jitterSeed)
{
    if (consecutiveFailures <= 0) {
        return 0;
    }

    const std::int64_t base = policy.baseSeconds > 0 ? policy.baseSeconds : 1;
    const std::int64_t cap = policy.capSeconds > base ? policy.capSeconds : base;

    // Cap the exponent too: a persisted file could claim a million failures, and
    // the loop should not run a million times to arrive at the same answer.
    const int steps = consecutiveFailures > 40 ? 40 : consecutiveFailures - 1;
    const std::int64_t plain = detail::growCapped(base, steps, cap);

    const int spread = std::clamp(policy.jitterPercent, 0, 100);
    if (spread == 0) {
        return plain;
    }

    // Jitter either side: [plain - spread%, plain + spread%], clamped so it can
    // never exceed the cap or drop below a second.
    const std::int64_t span = (plain * spread) / 100;
    if (span <= 0) {
        return plain;
    }
    const std::int64_t offset =
        static_cast<std::int64_t>(detail::mix(jitterSeed) % static_cast<std::uint64_t>(2 * span)) -
        span;

    std::int64_t jittered = plain + offset;
    if (jittered < 1) {
        jittered = 1;
    }
    if (jittered > cap) {
        jittered = cap;
    }
    return jittered;
}

BackoffState afterFailure(const BackoffState& state, std::int64_t nowUnix,
                          const BackoffPolicy& policy, std::uint64_t jitterSeed)
{
    BackoffState out = state;
    if (out.consecutiveFailures < 1000000) {
        out.consecutiveFailures += 1;
    }
    out.lastFailureUnix = nowUnix;
    out.nextAttemptUnix =
        nowUnix + backoffDelaySeconds(out.consecutiveFailures, policy, jitterSeed);
    return out;
}

BackoffState afterSuccess(const BackoffState& state, std::int64_t nowUnix)
{
    BackoffState out = state;
    out.consecutiveFailures = 0;
    out.nextAttemptUnix = 0;
    out.lastSuccessUnix = nowUnix;
    return out;
}

bool mayAttempt(const BackoffState& state, std::int64_t nowUnix, const BackoffPolicy& policy)
{
    return secondsUntilNextAttempt(state, nowUnix, policy) == 0;
}

std::int64_t secondsUntilNextAttempt(const BackoffState& state, std::int64_t nowUnix,
                                     const BackoffPolicy& policy)
{
    if (state.nextAttemptUnix <= nowUnix) {
        return 0;
    }

    // The console clock moved, or was wrong when this was written. Either way a
    // deadline further out than the longest delay we would ever choose is not a
    // deadline we set, so it is not one to honour - the alternative is a device
    // that silently stops checking for updates until some arbitrary future.
    const std::int64_t remaining = state.nextAttemptUnix - nowUnix;
    const std::int64_t cap = policy.capSeconds > 0 ? policy.capSeconds : 1;
    if (remaining > cap) {
        return 0;
    }
    return remaining;
}

std::string serializeBackoff(const BackoffState& state)
{
    detail::Json doc;
    doc["schema_version"] = 1;
    doc["consecutive_failures"] = state.consecutiveFailures;
    doc["next_attempt_unix"] = state.nextAttemptUnix;
    doc["last_failure_unix"] = state.lastFailureUnix;
    doc["last_success_unix"] = state.lastSuccessUnix;
    return doc.dump(2) + "\n";
}

std::optional<BackoffState> parseBackoff(std::string_view json)
{
    const detail::Json doc = detail::Json::parse(json, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return std::nullopt;
    }

    BackoffState out;

    // Every field is optional on read. A backoff file is a hint, and a hint
    // that has lost a field is still more useful than no hint at all.
    if (doc.contains("consecutive_failures") && doc["consecutive_failures"].is_number_integer()) {
        out.consecutiveFailures = doc["consecutive_failures"].get<int>();
    }
    if (doc.contains("next_attempt_unix") && doc["next_attempt_unix"].is_number_integer()) {
        out.nextAttemptUnix = doc["next_attempt_unix"].get<std::int64_t>();
    }
    if (doc.contains("last_failure_unix") && doc["last_failure_unix"].is_number_integer()) {
        out.lastFailureUnix = doc["last_failure_unix"].get<std::int64_t>();
    }
    if (doc.contains("last_success_unix") && doc["last_success_unix"].is_number_integer()) {
        out.lastSuccessUnix = doc["last_success_unix"].get<std::int64_t>();
    }

    if (out.consecutiveFailures < 0) {
        out.consecutiveFailures = 0;
    }
    return out;
}

}  // namespace nsx::core
