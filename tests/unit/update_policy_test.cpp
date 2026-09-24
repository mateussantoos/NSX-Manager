// SPDX-License-Identifier: GPL-3.0-only
//
// The rule that decides whether a user is offered an update. The predecessor
// got this wrong twice over: it compared versions by stripping non-digits, and
// it hid every tab whenever any newer tag existed. Both are pinned here.

#include "nsx/core/update/update_policy.hpp"

#include <doctest.h>

using namespace nsx::core;

namespace {

SemVer v(std::string_view text)
{
    const Result<SemVer, SemVerError> r = parseSemVer(text);
    REQUIRE(r.hasValue());
    return r.value();
}

/// A manifest with only the fields the policy actually reads.
UpdateManifest manifestFor(std::string_view version, std::string_view minSupported = "0.0.0",
                           bool mandatory = false, Channel channel = Channel::Stable)
{
    UpdateManifest m;
    m.schemaVersion = 1;
    m.version = v(version);
    m.minSupported = v(minSupported);
    m.mandatory = mandatory;
    m.channel = channel;
    return m;
}

}  // namespace

TEST_CASE("a newer release is offered")
{
    const UpdateDecision d = decideUpdate(v("0.1.0"), manifestFor("0.2.0"));
    CHECK(d.action == UpdateAction::Optional);
    CHECK(d.offersUpdate());
    REQUIRE(d.target.has_value());
    CHECK(d.target->toString() == "0.2.0");
    CHECK_FALSE(d.reason.empty());
}

TEST_CASE("the same version is up to date")
{
    const UpdateDecision d = decideUpdate(v("0.2.0"), manifestFor("0.2.0"));
    CHECK(d.action == UpdateAction::UpToDate);
    CHECK_FALSE(d.offersUpdate());
    CHECK_FALSE(d.target.has_value());
}

TEST_CASE("an older published version is never offered")
{
    // A rollback of the published manifest must not drag a user backwards.
    const UpdateDecision d = decideUpdate(v("0.3.0"), manifestFor("0.2.0"));
    CHECK(d.action == UpdateAction::UpToDate);
    CHECK_FALSE(d.offersUpdate());
}

TEST_CASE("mandatory is a per-release decision, not a compile-time one")
{
    SUBCASE("flag set")
    {
        const UpdateDecision d = decideUpdate(v("0.1.0"), manifestFor("0.2.0", "0.0.0", true));
        CHECK(d.action == UpdateAction::Mandatory);
        CHECK(d.offersUpdate());
    }
    SUBCASE("flag set but already current - still nothing to do")
    {
        // The predecessor hid all ten tabs whenever a newer tag existed
        // (main_frame.cpp:94-133). Being up to date must never lock the UI,
        // whatever the flag says.
        const UpdateDecision d = decideUpdate(v("0.2.0"), manifestFor("0.2.0", "0.0.0", true));
        CHECK(d.action == UpdateAction::UpToDate);
        CHECK_FALSE(d.offersUpdate());
    }
}

TEST_CASE("min_supported gates the in-app path")
{
    SUBCASE("below the floor")
    {
        const UpdateDecision d = decideUpdate(v("0.1.0"), manifestFor("0.5.0", "0.3.0"));
        CHECK(d.action == UpdateAction::UnsupportedPath);
        CHECK_FALSE(d.offersUpdate());
        CHECK(d.reason.find("0.3.0") != std::string::npos);
    }
    SUBCASE("exactly at the floor is allowed")
    {
        const UpdateDecision d = decideUpdate(v("0.3.0"), manifestFor("0.5.0", "0.3.0"));
        CHECK(d.action == UpdateAction::Optional);
    }
    SUBCASE("the floor is checked before mandatory")
    {
        // Otherwise a too-old client would be locked out of its own UI by an
        // update it is not permitted to install.
        const UpdateDecision d = decideUpdate(v("0.1.0"), manifestFor("0.5.0", "0.3.0", true));
        CHECK(d.action == UpdateAction::UnsupportedPath);
    }
}

TEST_CASE("a stable client ignores a beta manifest entirely")
{
    const UpdateDecision d = decideUpdate(
        v("0.1.0"), manifestFor("9.9.9", "0.0.0", true, Channel::Beta), Channel::Stable);
    CHECK(d.action == UpdateAction::WrongChannel);
    CHECK_FALSE(d.offersUpdate());
    CHECK_FALSE(d.target.has_value());
}

TEST_CASE("a beta client accepts a beta manifest")
{
    const UpdateDecision d = decideUpdate(
        v("0.1.0"), manifestFor("0.2.0-rc.1", "0.0.0", false, Channel::Beta), Channel::Beta);
    CHECK(d.action == UpdateAction::Optional);
}

TEST_CASE("an unreadable manifest is never reported as up to date")
{
    // The predecessor's defining bug: getLatestTag() returned "" on a 404
    // (download.cpp:493-500), the comparison read that as "no update", and the
    // updater was silently inert for its entire life.
    const UpdateDecision d = unreadableManifest("HTTP 404 from the release endpoint");
    CHECK(d.action == UpdateAction::ManifestUnreadable);
    CHECK(d.action != UpdateAction::UpToDate);
    CHECK_FALSE(d.offersUpdate());
    CHECK(d.reason.find("404") != std::string::npos);
}

TEST_CASE("regression: a prerelease is never offered over its release")
{
    // Digit-stripping made "1.0.0-rc.1" -> 1001 and "1.0.0" -> 100, so an RC
    // was offered to users already on the stable release.
    const UpdateDecision d = decideUpdate(v("1.0.0"), manifestFor("1.0.0-rc.1"));
    CHECK(d.action == UpdateAction::UpToDate);
}

TEST_CASE("regression: a hotfix prerelease is not an upgrade over its release")
{
    const UpdateDecision d = decideUpdate(v("4.2.1"), manifestFor("4.2.1-hotfix2"));
    CHECK(d.action == UpdateAction::UpToDate);
}

TEST_CASE("regression: two-digit components order correctly")
{
    // "4.2.10" -> 4210 beat "4.3.0" -> 430 under digit concatenation, so a real
    // upgrade looked like a downgrade.
    CHECK(decideUpdate(v("4.2.10"), manifestFor("4.3.0")).action == UpdateAction::Optional);
    CHECK(decideUpdate(v("4.9.0"), manifestFor("4.10.0")).action == UpdateAction::Optional);
    CHECK(decideUpdate(v("9.9.9"), manifestFor("10.0.0")).action == UpdateAction::Optional);
}

TEST_CASE("every action has a description")
{
    for (const UpdateAction a : {UpdateAction::UpToDate, UpdateAction::Optional,
                                 UpdateAction::Mandatory, UpdateAction::UnsupportedPath,
                                 UpdateAction::WrongChannel, UpdateAction::ManifestUnreadable}) {
        CHECK_FALSE(describe(a).empty());
        CHECK(describe(a) != "unknown");
    }
}
