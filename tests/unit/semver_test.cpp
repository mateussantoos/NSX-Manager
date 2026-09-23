// SPDX-License-Identifier: GPL-3.0-only
//
// The first suite in the project, and deliberately so: the predecessor's
// version comparison is the bug that made its updater worse than useless.
//
//   main_frame.cpp:53-73 stripped every non-digit character and compared the
//   result as a single integer, so "v4.2.1" became 421.
//
// The REGRESSION section below is that bug, encoded permanently.

#include "nsx/core/version/semver.hpp"

#include <doctest.h>

using namespace nsx::core;

namespace {

SemVer parse(std::string_view s)
{
    const Result<SemVer, SemVerError> r = parseSemVer(s);
    REQUIRE(r.hasValue());
    return r.value();
}

SemVerError errorOf(std::string_view s)
{
    const Result<SemVer, SemVerError> r = parseSemVer(s);
    REQUIRE_FALSE(r.hasValue());
    return r.error();
}

// doctest decomposes a binary comparison through templates, which loses the
// "literal 0" property that comparing a std::strong_ordering against 0
// requires. These keep the assertions boolean - and more readable.
bool orderedAbove(const SemVer& a, const SemVer& b)
{
    return compare(a, b) > 0;
}

bool samePrecedence(const SemVer& a, const SemVer& b)
{
    return compare(a, b) == 0;
}

}  // namespace

TEST_CASE("parses a well-formed version")
{
    const SemVer v = parse("1.2.3");
    CHECK(v.major == 1);
    CHECK(v.minor == 2);
    CHECK(v.patch == 3);
    CHECK(v.prerelease.empty());
    CHECK(v.build.empty());
}

TEST_CASE("accepts one leading v, because release tags carry it")
{
    CHECK(parse("v0.1.0").minor == 1);
    CHECK(parse("V0.1.0").minor == 1);
    CHECK(errorOf("vv0.1.0") == SemVerError::NonNumericField);
}

TEST_CASE("splits prerelease and build metadata")
{
    SUBCASE("prerelease only")
    {
        const SemVer v = parse("1.0.0-rc.1");
        CHECK(v.prerelease == "rc.1");
        CHECK(v.build.empty());
    }
    SUBCASE("build only")
    {
        const SemVer v = parse("1.0.0+20260923");
        CHECK(v.prerelease.empty());
        CHECK(v.build == "20260923");
    }
    SUBCASE("both, and a build containing a hyphen is not read as a prerelease")
    {
        const SemVer v = parse("1.0.0-rc.1+build-7");
        CHECK(v.prerelease == "rc.1");
        CHECK(v.build == "build-7");
    }
}

TEST_CASE("strict parsing rejects rather than guessing")
{
    CHECK(errorOf("") == SemVerError::Empty);
    CHECK(errorOf("1") == SemVerError::MissingMinor);
    CHECK(errorOf("1.2") == SemVerError::MissingPatch);
    CHECK(errorOf("1.2.3.4") == SemVerError::TrailingGarbage);
    CHECK(errorOf("1.2.x") == SemVerError::NonNumericField);
    CHECK(errorOf("01.2.3") == SemVerError::LeadingZero);
    CHECK(errorOf("1.02.3") == SemVerError::LeadingZero);
    CHECK(errorOf("1.2.3-") == SemVerError::InvalidPrerelease);
    CHECK(errorOf("1.2.3-rc..1") == SemVerError::InvalidPrerelease);
    CHECK(errorOf("1.2.3+") == SemVerError::InvalidBuild);
}

TEST_CASE("0.0.0 is valid and a single zero is not a leading zero")
{
    const SemVer v = parse("0.0.0");
    CHECK(v.major == 0);
    CHECK(v.patch == 0);
}

TEST_CASE("a component larger than 32 bits is rejected, not wrapped")
{
    CHECK(parse("4294967295.0.0").major == 4294967295U);
    CHECK(errorOf("4294967296.0.0") == SemVerError::NumericOverflow);
    CHECK(errorOf("99999999999999999999.0.0") == SemVerError::NumericOverflow);
}

TEST_CASE("toString round-trips and never emits a leading v")
{
    for (const char* s : {"0.1.0", "1.2.3", "1.0.0-rc.1", "1.0.0+build", "1.0.0-rc.1+build"}) {
        CHECK(parse(s).toString() == s);
    }
    CHECK(parse("v2.0.0").toString() == "2.0.0");
}

TEST_CASE("numeric components order before anything else")
{
    CHECK(orderedAbove(parse("2.0.0"), parse("1.9.9")));
    CHECK(orderedAbove(parse("1.10.0"), parse("1.9.0")));  // not lexical
    CHECK(orderedAbove(parse("1.0.10"), parse("1.0.9")));  // not lexical
    CHECK(samePrecedence(parse("1.2.3"), parse("1.2.3")));
}

TEST_CASE("prerelease precedence follows SemVer section 11")
{
    SUBCASE("a release outranks its own prerelease")
    {
        CHECK(orderedAbove(parse("1.0.0"), parse("1.0.0-rc.1")));
    }
    SUBCASE("numeric identifiers compare by value, not lexically")
    {
        CHECK(orderedAbove(parse("1.0.0-rc.10"), parse("1.0.0-rc.9")));
    }
    SUBCASE("alphanumeric outranks numeric")
    {
        CHECK(orderedAbove(parse("1.0.0-alpha"), parse("1.0.0-1")));
    }
    SUBCASE("more identifiers outrank fewer when the shared ones match")
    {
        CHECK(orderedAbove(parse("1.0.0-alpha.1"), parse("1.0.0-alpha")));
    }
    SUBCASE("the full example chain from the specification")
    {
        const char* chain[] = {"1.0.0-alpha",  "1.0.0-alpha.1", "1.0.0-alpha.beta", "1.0.0-beta",
                               "1.0.0-beta.2", "1.0.0-beta.11", "1.0.0-rc.1",       "1.0.0"};
        for (std::size_t i = 1; i < std::size(chain); ++i) {
            CAPTURE(chain[i - 1]);
            CAPTURE(chain[i]);
            CHECK(orderedAbove(parse(chain[i]), parse(chain[i - 1])));
        }
    }
}

TEST_CASE("build metadata is ignored when ordering")
{
    CHECK(samePrecedence(parse("1.0.0+a"), parse("1.0.0+b")));
    CHECK(samePrecedence(parse("1.0.0+zzz"), parse("1.0.0")));
}

TEST_CASE("tolerant parsing salvages an upstream tag")
{
    CHECK(tryParseSemVer("4.2.1-nsx")->patch == 1);
    CHECK(tryParseSemVer("v1.2")->minor == 2);
    CHECK(tryParseSemVer("v1.2")->patch == 0);
    CHECK(tryParseSemVer("3")->major == 3);
    CHECK(tryParseSemVer("01.02.03")->minor == 2);  // lenient about leading zeros
    CHECK_FALSE(tryParseSemVer("").has_value());
    CHECK_FALSE(tryParseSemVer("not-a-version").has_value());
    CHECK_FALSE(tryParseSemVer("v").has_value());
}

// ---------------------------------------------------------------------------
// REGRESSION - the predecessor's actual failures.
// ---------------------------------------------------------------------------

TEST_CASE("regression: a hotfix prerelease is never offered as an upgrade")
{
    // main_frame.cpp:53-73 -> "4.2.1-hotfix2" became 4212, "4.2.1" became 421,
    // so 4212 > 421 and the app offered a DOWNGRADE as an update.
    const SemVer installed = parse("4.2.1");
    const SemVer offered = *tryParseSemVer("4.2.1-hotfix2");
    CHECK_FALSE(isNewerThan(offered, installed));
    CHECK(isNewerThan(installed, offered));
}

TEST_CASE("regression: a release candidate never outranks its release")
{
    // Same mechanism: "1.0.0-rc.1" -> 1001 > "1.0.0" -> 100.
    CHECK_FALSE(isNewerThan(parse("1.0.0-rc.1"), parse("1.0.0")));
}

TEST_CASE("regression: a date-shaped tag does not terminate the application")
{
    // "v4.2.1 (2026-09-23)" digit-stripped to "42120260923", which overflowed
    // std::stoi and threw an uncaught std::out_of_range.
    CHECK_NOTHROW((void)tryParseSemVer("v4.2.1 (2026-09-23)"));
    CHECK_NOTHROW((void)parseSemVer("v4.2.1 (2026-09-23)"));

    const std::optional<SemVer> salvaged = tryParseSemVer("v4.2.1 (2026-09-23)");
    REQUIRE(salvaged.has_value());
    CHECK(salvaged->major == 4);
    CHECK(salvaged->patch == 1);

    CHECK_NOTHROW((void)tryParseSemVer("2026.09.23.1"));
    CHECK_NOTHROW((void)tryParseSemVer("99999999999999999999999"));
    CHECK_FALSE(tryParseSemVer("99999999999999999999999").has_value());
}

TEST_CASE("regression: two-digit components do not corrupt ordering")
{
    // Digit concatenation made "4.2.10" -> 4210 and "4.3.0" -> 430, so a real
    // upgrade to 4.3.0 looked like a downgrade from 4.2.10.
    CHECK(isNewerThan(parse("4.3.0"), parse("4.2.10")));
    CHECK(isNewerThan(parse("4.10.0"), parse("4.9.0")));
    CHECK(isNewerThan(parse("10.0.0"), parse("9.9.9")));
}

TEST_CASE("regression: an empty or absent tag is not silently up to date")
{
    // getLatestTag() returned "" on a 404 (download.cpp:493-500), which the
    // comparison read as "no update available" - the failure that made the
    // predecessor's updater inert for its entire life.
    CHECK_FALSE(parseSemVer("").hasValue());
    CHECK(parseSemVer("").error() == SemVerError::Empty);
    CHECK_FALSE(tryParseSemVer("").has_value());
}

TEST_CASE("every error has a human-readable description")
{
    const SemVerError all[] = {SemVerError::Empty,
                               SemVerError::MissingMinor,
                               SemVerError::MissingPatch,
                               SemVerError::NonNumericField,
                               SemVerError::LeadingZero,
                               SemVerError::NumericOverflow,
                               SemVerError::InvalidPrerelease,
                               SemVerError::InvalidBuild,
                               SemVerError::TrailingGarbage};
    for (const SemVerError e : all) {
        CAPTURE(static_cast<int>(e));
        CHECK_FALSE(describe(e).empty());
        CHECK(describe(e) != "unknown error");
    }
}
