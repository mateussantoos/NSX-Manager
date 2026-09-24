// SPDX-License-Identifier: GPL-3.0-only
//
// preserve.txt and the extraction policy.
//
// A CFW pack overwrites large parts of the SD card, and some of what it
// overwrites belongs to the user: their boot entries, their emuMMC pointer,
// their overlays. Getting this wrong does not crash anything - it quietly
// replaces a working configuration, which the user discovers at the next boot.

#include "nsx/core/paths/extraction_policy.hpp"
#include "nsx/core/paths/preserve_rules.hpp"

#include <string>

#include <doctest.h>

using namespace nsx::core;

namespace {

constexpr const char* kRoot = "/config/nsx-manager/staging/pack";

PreserveRules rules(const std::string& text)
{
    return PreserveRules::parse(text);
}

}  // namespace

// ---------------------------------------------------------------------------
// The matcher
// ---------------------------------------------------------------------------

TEST_CASE("a literal pattern matches exactly that path")
{
    CHECK(globMatch("bootloader/hekate_ipl.ini", "bootloader/hekate_ipl.ini"));
    CHECK_FALSE(globMatch("bootloader/hekate_ipl.ini", "bootloader/hekate_ipl.ini.bak"));
    CHECK_FALSE(globMatch("bootloader/hekate_ipl.ini", "bootloader/other.ini"));
}

TEST_CASE("a single star stays inside one component")
{
    CHECK(globMatch("bootloader/*.ini", "bootloader/hekate_ipl.ini"));
    CHECK(globMatch("*.ini", "hekate_ipl.ini"));
    CHECK_FALSE(globMatch("bootloader/*.ini", "bootloader/ini/nested.ini"));
    CHECK_FALSE(globMatch("*.ini", "bootloader/hekate_ipl.ini"));
}

TEST_CASE("a double star crosses separators")
{
    CHECK(globMatch("bootloader/**", "bootloader/ini/nested.ini"));
    CHECK(globMatch("**/*.ini", "bootloader/ini/deep/nested.ini"));
    CHECK(globMatch("**", "anything/at/all.txt"));
}

TEST_CASE("a leading double star also matches zero directories")
{
    // `**/x` must match a top-level `x`, or every rule needs writing twice.
    CHECK(globMatch("**/hekate_ipl.ini", "hekate_ipl.ini"));
    CHECK(globMatch("**/hekate_ipl.ini", "bootloader/hekate_ipl.ini"));
}

TEST_CASE("a question mark matches one character but never a separator")
{
    CHECK(globMatch("file?.txt", "file1.txt"));
    CHECK_FALSE(globMatch("file?.txt", "file12.txt"));
    CHECK_FALSE(globMatch("a?b", "a/b"));
}

TEST_CASE("matching is case-insensitive, because the file system is")
{
    // On FAT these ARE the same file. A case-sensitive comparison would fail to
    // preserve a file the user plainly named.
    CHECK(globMatch("bootloader/hekate_ipl.ini", "Bootloader/Hekate_IPL.ini"));
    CHECK(globMatch("EMUMMC/*.ini", "emuMMC/emummc.ini"));
}

TEST_CASE("a pathological pattern does not hang")
{
    // Naive recursive globbing goes exponential here. This must return.
    CHECK_FALSE(globMatch("**/**/**/**/**/**/**/x",
                          "a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/y"));
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

TEST_CASE("comments and blank lines are ignored")
{
    const PreserveRules r = rules("# a comment\n\n   \nbootloader/hekate_ipl.ini\n# another\n");
    CHECK(r.rules().size() == 1);
    CHECK(r.warnings().empty());
}

TEST_CASE("a trailing slash marks a directory rule")
{
    const PreserveRules r = rules("bootloader/ini/\n");
    REQUIRE(r.rules().size() == 1);
    CHECK(r.rules()[0].directory);
    CHECK(r.rules()[0].pattern == "bootloader/ini");
}

TEST_CASE("a leading bang negates")
{
    const PreserveRules r = rules("bootloader/**\n!bootloader/payloads/**\n");
    REQUIRE(r.rules().size() == 2);
    CHECK_FALSE(r.rules()[0].negated);
    CHECK(r.rules()[1].negated);
}

TEST_CASE("a leading separator is normalised away")
{
    // /bootloader/x and bootloader/x are the same rule; a user should not have
    // to know which one this file wants.
    const PreserveRules r = rules("/bootloader/hekate_ipl.ini\n");
    REQUIRE(r.rules().size() == 1);
    CHECK(r.rules()[0].pattern == "bootloader/hekate_ipl.ini");
    CHECK(r.preserves("bootloader/hekate_ipl.ini"));
}

TEST_CASE("carriage returns from a Windows-edited file are tolerated")
{
    // A user edits this on a PC. It must not stop working because of it.
    const PreserveRules r = rules("bootloader/hekate_ipl.ini\r\nemuMMC/emummc.ini\r\n");
    CHECK(r.rules().size() == 2);
    CHECK(r.preserves("bootloader/hekate_ipl.ini"));
}

TEST_CASE("parsing never fails; bad lines are dropped and reported")
{
    // A typo in a user's config must not stop a pack installing. The safe
    // direction is that something gets overwritten which they can restore.
    const PreserveRules r = rules("!\n../escape\nbootloader/ok.ini\n");
    CHECK(r.rules().size() == 1);
    CHECK(r.warnings().size() == 2);
    CHECK(r.preserves("bootloader/ok.ini"));
}

TEST_CASE("a parent reference in a pattern is dropped, not honoured")
{
    const PreserveRules r = rules("../../bootloader/x\n");
    CHECK(r.empty());
    REQUIRE(r.warnings().size() == 1);
    CHECK(r.warnings()[0].find("..") != std::string::npos);
}

TEST_CASE("a file with no trailing newline still parses its last line")
{
    const PreserveRules r = rules("bootloader/hekate_ipl.ini");
    CHECK(r.rules().size() == 1);
    CHECK(r.preserves("bootloader/hekate_ipl.ini"));
}

TEST_CASE("an empty file preserves nothing")
{
    CHECK(rules("").empty());
    CHECK_FALSE(rules("").preserves("anything"));
    CHECK(rules("# only a comment\n").empty());
}

// ---------------------------------------------------------------------------
// Matching rules against paths
// ---------------------------------------------------------------------------

TEST_CASE("a directory rule covers everything beneath it")
{
    const PreserveRules r = rules("bootloader/ini/\n");
    CHECK(r.preserves("bootloader/ini/config.ini"));
    CHECK(r.preserves("bootloader/ini/deep/nested.ini"));
    CHECK(r.preserves("bootloader/ini"));
    CHECK_FALSE(r.preserves("bootloader/hekate_ipl.ini"));
}

TEST_CASE("a bare directory name covers its contents too")
{
    // A user who omits the trailing slash means the same thing.
    const PreserveRules r = rules("bootloader/ini\n");
    CHECK(r.preserves("bootloader/ini/config.ini"));
}

TEST_CASE("last match wins, so a negation can carve an exception")
{
    const PreserveRules r = rules("bootloader/**\n!bootloader/payloads/**\n");
    CHECK(r.preserves("bootloader/hekate_ipl.ini"));
    CHECK(r.preserves("bootloader/ini/x.ini"));
    CHECK_FALSE(r.preserves("bootloader/payloads/fusee.bin"));
}

TEST_CASE("order is meaningful in both directions")
{
    // Re-preserving after a negation must also work, or the rule is only half
    // a feature.
    const PreserveRules r =
        rules("bootloader/**\n!bootloader/payloads/**\nbootloader/payloads/keep.bin\n");
    CHECK_FALSE(r.preserves("bootloader/payloads/fusee.bin"));
    CHECK(r.preserves("bootloader/payloads/keep.bin"));
}

TEST_CASE("the defaults keep the boot configuration and the emummc pointer")
{
    const PreserveRules r = PreserveRules::defaults();
    CHECK(r.preserves("bootloader/hekate_ipl.ini"));
    CHECK(r.preserves("bootloader/ini/my-entries.ini"));
    CHECK(r.preserves("emuMMC/emummc.ini"));

    // ... and nothing else. A pack's own files are the pack's to replace.
    CHECK_FALSE(r.preserves("atmosphere/package3"));
    CHECK_FALSE(r.preserves("bootloader/payloads/fusee.bin"));
    CHECK_FALSE(r.preserves("switch/nsx-manager/nsx-manager.nro"));
}

// ---------------------------------------------------------------------------
// The policy: traversal and preserve together
// ---------------------------------------------------------------------------

TEST_CASE("an ordinary entry is written under the root")
{
    const ExtractionPolicy policy(kRoot, PreserveRules::defaults());
    const EntryDecision d = policy.decide("atmosphere/package3");

    CHECK(d.action == EntryAction::Write);
    CHECK(d.destination == std::string(kRoot) + "/atmosphere/package3");
    CHECK(d.writes());
}

TEST_CASE("a directory entry is created rather than written")
{
    const ExtractionPolicy policy(kRoot, PreserveRules::defaults());
    const EntryDecision d = policy.decide("atmosphere/contents/");

    CHECK(d.action == EntryAction::CreateDirectory);
    CHECK(d.writes());
}

TEST_CASE("a preserved entry is skipped, and says why")
{
    const ExtractionPolicy policy(kRoot, PreserveRules::defaults());
    const EntryDecision d = policy.decide("bootloader/hekate_ipl.ini");

    CHECK(d.action == EntryAction::SkipPreserved);
    CHECK_FALSE(d.writes());
    CHECK(d.reason.find("preserve.txt") != std::string::npos);
}

TEST_CASE("a traversal entry is rejected whatever the preserve rules say")
{
    const ExtractionPolicy policy(kRoot, PreserveRules::defaults());
    for (const char* evil :
         {"../../bootloader/hekate_ipl.ini", "/etc/passwd", "sdmc:/atmosphere/x", "a\\b", "a//b"}) {
        CAPTURE(evil);
        const EntryDecision d = policy.decide(evil);
        CHECK(d.action == EntryAction::Reject);
        CHECK_FALSE(d.writes());
        CHECK(d.destination.empty());
        CHECK_FALSE(d.reason.empty());
    }
}

TEST_CASE("ORDER: traversal is decided before preserve, not after")
{
    // `../../bootloader/hekate_ipl.ini` matches a preserve rule as text while
    // escaping the destination. If preserve ran first it would be reported as
    // a harmless skip, and a reviewer reading the log would see nothing wrong.
    // It must be a rejection.
    const ExtractionPolicy policy(kRoot, PreserveRules::defaults());
    const EntryDecision d = policy.decide("../../bootloader/hekate_ipl.ini");

    CHECK(d.action == EntryAction::Reject);
    CHECK(d.error == PathError::ParentTraversal);
    CHECK(d.action != EntryAction::SkipPreserved);
}

TEST_CASE("every decision either rejects or lands inside the root")
{
    const ExtractionPolicy policy(kRoot, PreserveRules::defaults());
    for (const char* entry : {"a.txt", "a/b/c.txt", "dir/", "bootloader/hekate_ipl.ini",
                              "../evil", "/abs", "sdmc:/x", "a\\b", "", "..", "foo."}) {
        CAPTURE(entry);
        const EntryDecision d = policy.decide(entry);
        if (d.action != EntryAction::Reject) {
            CHECK(isWithin(kRoot, d.destination));
        }
    }
}

TEST_CASE("a policy with no preserve rules writes everything safe")
{
    const ExtractionPolicy policy(kRoot, PreserveRules::parse(""));
    CHECK(policy.decide("bootloader/hekate_ipl.ini").action == EntryAction::Write);
    CHECK(policy.decide("../evil").action == EntryAction::Reject);
}

TEST_CASE("every entry action has a description")
{
    for (const EntryAction a : {EntryAction::Write, EntryAction::CreateDirectory,
                                EntryAction::SkipPreserved, EntryAction::Reject}) {
        CHECK_FALSE(describe(a).empty());
        CHECK(describe(a) != "unknown");
    }
}
