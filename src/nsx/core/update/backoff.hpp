// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nsx::core {

/// @brief How quickly repeated failures are backed off.
///
/// @details The defaults come from `docs/architecture/update-pipeline.md`:
///          base 60 s, cap 6 h. The cap matches the manifest cache TTL, so the
///          worst case is one failed request per cache lifetime rather than one
///          per launch.
/// @since 0.2.0
struct BackoffPolicy
{
    std::int64_t baseSeconds{60};          ///< Delay after the first failure.
    std::int64_t capSeconds{6 * 60 * 60};  ///< Never wait longer than this.
    int jitterPercent{20};                 ///< Spread, as a percentage either side.
};

/// @brief Persisted backoff state.
///
/// @details Persisted, not in-memory, and that is the whole point. A homebrew
///          application is launched and quits constantly; state held only in
///          memory means every launch is the first failure again, and a broken
///          endpoint is re-hammered forever. This is what survives the quit.
/// @since 0.2.0
struct BackoffState
{
    int consecutiveFailures{};       ///< Reset to zero by a success.
    std::int64_t nextAttemptUnix{};  ///< No attempt before this instant.
    std::int64_t lastFailureUnix{};  ///< For the UI: "last checked N ago".
    std::int64_t lastSuccessUnix{};  ///< For the UI, and for staleness display.
};

/// @brief The delay to wait after a given number of consecutive failures.
///
/// @details Doubles from @ref BackoffPolicy::baseSeconds, clamped to
///          @ref BackoffPolicy::capSeconds, then spread by
///          @ref BackoffPolicy::jitterPercent either side. The result is
///          clamped to the cap again, so jitter can never push a delay past it.
///
///          The jitter is derived from @p jitterSeed rather than a global
///          generator, which keeps the function pure and the tests exact. The
///          caller supplies something that varies per device - the clock is
///          enough, since the point is only to stop many consoles retrying in
///          lockstep after a shared outage.
///
/// @param consecutiveFailures How many failures have happened in a row; zero or
///        negative yields zero.
/// @param policy The growth policy.
/// @param jitterSeed Varies the delay; the same seed always gives the same delay.
/// @return The delay in seconds.
/// @since 0.2.0
[[nodiscard]] std::int64_t backoffDelaySeconds(int consecutiveFailures, const BackoffPolicy& policy,
                                               std::uint64_t jitterSeed);

/// @brief Record a failed attempt.
/// @param state The state before the attempt.
/// @param nowUnix The current time.
/// @param policy The growth policy.
/// @param jitterSeed Varies the delay; see @ref backoffDelaySeconds.
/// @return The state to persist.
/// @since 0.2.0
[[nodiscard]] BackoffState afterFailure(const BackoffState& state, std::int64_t nowUnix,
                                        const BackoffPolicy& policy, std::uint64_t jitterSeed);

/// @brief Record a successful attempt.
/// @param state The state before the attempt.
/// @param nowUnix The current time.
/// @return The state to persist, with the failure count cleared.
/// @since 0.2.0
[[nodiscard]] BackoffState afterSuccess(const BackoffState& state, std::int64_t nowUnix);

/// @brief Whether an attempt may be made now.
///
/// @details Tolerates a clock that moved. The Switch RTC is frequently wrong,
///          and a state written while the clock was far in the future would
///          otherwise lock out update checks until that future arrived. A
///          @ref BackoffState::nextAttemptUnix more than
///          @ref BackoffPolicy::capSeconds ahead of @p nowUnix is therefore
///          treated as nonsense and ignored rather than obeyed.
///
/// @param state The persisted state.
/// @param nowUnix The current time.
/// @param policy The growth policy, for the sanity bound above.
/// @return True when the caller may attempt a request.
/// @since 0.2.0
[[nodiscard]] bool mayAttempt(const BackoffState& state, std::int64_t nowUnix,
                              const BackoffPolicy& policy = {});

/// @brief How long until an attempt is allowed.
/// @param state The persisted state.
/// @param nowUnix The current time.
/// @param policy The growth policy, for the same sanity bound as @ref mayAttempt.
/// @return Seconds remaining, or zero when an attempt may be made now.
/// @since 0.2.0
[[nodiscard]] std::int64_t secondsUntilNextAttempt(const BackoffState& state, std::int64_t nowUnix,
                                                   const BackoffPolicy& policy = {});

/// @brief Render backoff state as JSON.
/// @param state The state to render.
/// @return A document that @ref parseBackoff accepts.
/// @since 0.2.0
[[nodiscard]] std::string serializeBackoff(const BackoffState& state);

/// @brief Parse persisted backoff state.
///
/// @details Returns `std::nullopt` for anything unusable rather than an error
///          type, because there is exactly one sane response to a corrupt
///          backoff file: forget it and start from a clean state. Refusing to
///          check for updates because a throttling hint could not be read would
///          be the wrong trade in every case.
///
///          **Never throws.**
///
/// @param json The raw document.
/// @return The parsed state, or `std::nullopt`.
/// @since 0.2.0
[[nodiscard]] std::optional<BackoffState> parseBackoff(std::string_view json);

}  // namespace nsx::core
