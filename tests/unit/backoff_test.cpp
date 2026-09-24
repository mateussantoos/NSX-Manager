// SPDX-License-Identifier: GPL-3.0-only
//
// Persisted exponential backoff.
//
// The whole value of this module is that it survives the application quitting,
// so the properties worth pinning are the ones that only show up across
// launches: that the delay actually grows, that it stops growing at the cap,
// that a corrupt or clock-damaged state file cannot lock a console out of
// checking for updates ever again.

#include "nsx/core/update/backoff.hpp"

#include <set>

#include <doctest.h>

using namespace nsx::core;

namespace {

constexpr std::int64_t kNow = 1'760'000'000;  // an arbitrary, plausible instant

/// No jitter, so growth can be asserted exactly.
BackoffPolicy exact()
{
    BackoffPolicy p;
    p.jitterPercent = 0;
    return p;
}

}  // namespace

// ---------------------------------------------------------------------------
// Growth
// ---------------------------------------------------------------------------

TEST_CASE("no failures means no delay")
{
    CHECK(backoffDelaySeconds(0, exact(), 1) == 0);
    CHECK(backoffDelaySeconds(-5, exact(), 1) == 0);
}

TEST_CASE("the delay doubles from the base")
{
    const BackoffPolicy p = exact();
    CHECK(backoffDelaySeconds(1, p, 1) == 60);
    CHECK(backoffDelaySeconds(2, p, 1) == 120);
    CHECK(backoffDelaySeconds(3, p, 1) == 240);
    CHECK(backoffDelaySeconds(4, p, 1) == 480);
}

TEST_CASE("the delay stops at the cap and stays there")
{
    const BackoffPolicy p = exact();
    CHECK(backoffDelaySeconds(9, p, 1) == 15360);
    CHECK(backoffDelaySeconds(10, p, 1) == p.capSeconds);
    CHECK(backoffDelaySeconds(30, p, 1) == p.capSeconds);
}

TEST_CASE("an absurd failure count does not overflow into a short delay")
{
    // A persisted file is user-writable. `base << failures` would wrap long
    // before this and hand back something small - turning a long backoff into
    // an immediate retry, which is the opposite of what the file asked for.
    const BackoffPolicy p = exact();
    for (const int failures : {63, 64, 65, 1000, 1000000}) {
        CAPTURE(failures);
        CHECK(backoffDelaySeconds(failures, p, 1) == p.capSeconds);
    }
}

// ---------------------------------------------------------------------------
// Jitter
// ---------------------------------------------------------------------------

TEST_CASE("jitter stays inside the requested spread")
{
    BackoffPolicy p;
    p.jitterPercent = 20;

    for (std::uint64_t seed = 0; seed < 500; ++seed) {
        const std::int64_t d = backoffDelaySeconds(3, p, seed);
        CAPTURE(seed);
        CHECK(d >= 240 - 48);
        CHECK(d <= 240 + 48);
    }
}

TEST_CASE("jitter never pushes a delay past the cap")
{
    BackoffPolicy p;
    p.jitterPercent = 50;
    for (std::uint64_t seed = 0; seed < 500; ++seed) {
        CAPTURE(seed);
        CHECK(backoffDelaySeconds(20, p, seed) <= p.capSeconds);
        CHECK(backoffDelaySeconds(20, p, seed) >= 1);
    }
}

TEST_CASE("jitter actually varies, and is reproducible from its seed")
{
    // Both halves matter: identical delays across devices would defeat the
    // point, and a non-reproducible delay could not be asserted here at all.
    BackoffPolicy p;
    p.jitterPercent = 20;

    std::set<std::int64_t> seen;
    for (std::uint64_t seed = 0; seed < 100; ++seed) {
        seen.insert(backoffDelaySeconds(5, p, seed));
    }
    CHECK(seen.size() > 10);

    CHECK(backoffDelaySeconds(5, p, 42) == backoffDelaySeconds(5, p, 42));
}

// ---------------------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------------------

TEST_CASE("a failure schedules the next attempt and records when it happened")
{
    const BackoffState after = afterFailure(BackoffState{}, kNow, exact(), 1);
    CHECK(after.consecutiveFailures == 1);
    CHECK(after.lastFailureUnix == kNow);
    CHECK(after.nextAttemptUnix == kNow + 60);
    CHECK_FALSE(mayAttempt(after, kNow));
    CHECK(mayAttempt(after, kNow + 60));
}

TEST_CASE("consecutive failures push the deadline further out")
{
    const BackoffPolicy p = exact();
    BackoffState s;
    s = afterFailure(s, kNow, p, 1);
    s = afterFailure(s, kNow, p, 1);
    s = afterFailure(s, kNow, p, 1);
    CHECK(s.consecutiveFailures == 3);
    CHECK(s.nextAttemptUnix == kNow + 240);
}

TEST_CASE("a success clears the budget immediately")
{
    BackoffState s = afterFailure(BackoffState{}, kNow, exact(), 1);
    s = afterFailure(s, kNow, exact(), 1);
    const BackoffState ok = afterSuccess(s, kNow + 5);

    CHECK(ok.consecutiveFailures == 0);
    CHECK(ok.nextAttemptUnix == 0);
    CHECK(ok.lastSuccessUnix == kNow + 5);
    CHECK(mayAttempt(ok, kNow + 5));

    // The last failure time survives: the UI still wants to say when the last
    // failure was, even though the throttle is gone.
    CHECK(ok.lastFailureUnix == kNow);
}

TEST_CASE("secondsUntilNextAttempt counts down and then reports zero")
{
    const BackoffState s = afterFailure(BackoffState{}, kNow, exact(), 1);
    CHECK(secondsUntilNextAttempt(s, kNow) == 60);
    CHECK(secondsUntilNextAttempt(s, kNow + 59) == 1);
    CHECK(secondsUntilNextAttempt(s, kNow + 60) == 0);
    CHECK(secondsUntilNextAttempt(s, kNow + 600) == 0);
}

// ---------------------------------------------------------------------------
// A clock that moved
// ---------------------------------------------------------------------------

TEST_CASE("a deadline further out than the cap is ignored, not obeyed")
{
    // Written while the console clock was years ahead. Honouring it would mean
    // never checking for updates again until that date arrived - a single bad
    // RTC reading permanently stranding the device on an old build.
    BackoffState s;
    s.consecutiveFailures = 2;
    s.nextAttemptUnix = kNow + (10LL * 365 * 24 * 60 * 60);

    CHECK(mayAttempt(s, kNow));
    CHECK(secondsUntilNextAttempt(s, kNow) == 0);
}

TEST_CASE("a deadline just inside the cap is still honoured")
{
    const BackoffPolicy p;
    BackoffState s;
    s.nextAttemptUnix = kNow + p.capSeconds - 1;
    CHECK_FALSE(mayAttempt(s, kNow));
    CHECK(secondsUntilNextAttempt(s, kNow) == p.capSeconds - 1);
}

TEST_CASE("a clock that jumped backwards does not deadlock the check")
{
    // The RTC reset to its epoch. Everything persisted now looks like the
    // distant future.
    const BackoffState s = afterFailure(BackoffState{}, kNow, exact(), 1);
    CHECK(mayAttempt(s, 0));
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

TEST_CASE("state round-trips through JSON")
{
    BackoffState s;
    s.consecutiveFailures = 4;
    s.nextAttemptUnix = kNow + 480;
    s.lastFailureUnix = kNow;
    s.lastSuccessUnix = kNow - 9000;

    const std::optional<BackoffState> back = parseBackoff(serializeBackoff(s));
    REQUIRE(back.has_value());
    CHECK(back->consecutiveFailures == s.consecutiveFailures);
    CHECK(back->nextAttemptUnix == s.nextAttemptUnix);
    CHECK(back->lastFailureUnix == s.lastFailureUnix);
    CHECK(back->lastSuccessUnix == s.lastSuccessUnix);
}

TEST_CASE("an unusable state file is forgotten rather than fatal")
{
    // There is exactly one sane response to a corrupt throttling hint: start
    // clean. Refusing to check for updates because a hint could not be read
    // would be the wrong trade every single time.
    for (const char* bad : {"{ truncated", "[]", "null", "\"text\"", ""}) {
        CAPTURE(bad);
        CHECK_NOTHROW((void)parseBackoff(bad));
        CHECK_FALSE(parseBackoff(bad).has_value());
    }
}

TEST_CASE("a state file with missing or wrong-typed fields still yields a usable state")
{
    const std::optional<BackoffState> partial =
        parseBackoff(R"({"consecutive_failures": 2, "next_attempt_unix": "soon"})");
    REQUIRE(partial.has_value());
    CHECK(partial->consecutiveFailures == 2);
    CHECK(partial->nextAttemptUnix == 0);  // the string was ignored, not guessed at
    CHECK(mayAttempt(*partial, kNow));
}

TEST_CASE("a negative failure count is clamped rather than trusted")
{
    const std::optional<BackoffState> s = parseBackoff(R"({"consecutive_failures": -7})");
    REQUIRE(s.has_value());
    CHECK(s->consecutiveFailures == 0);
}
