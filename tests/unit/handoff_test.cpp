// SPDX-License-Identifier: GPL-3.0-only
//
// The handoff and the recovery state machine.
//
// These are the states that only occur after a crash or a power cut, which
// makes them the hardest to reach deliberately on hardware and the easiest to
// get wrong. Keeping the decision pure is what lets every row of the recovery
// table in docs/architecture/self-update-forwarder.md be exercised here.

#include "nsx/core/update/handoff.hpp"

#include <fstream>
#include <sstream>

#include <doctest.h>

using namespace nsx::core;

namespace {

std::string fixture(const std::string& name)
{
    const std::string path = std::string(NSX_FIXTURE_DIR) + "/handoff/" + name;
    std::ifstream in(path, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "fixture not found: " << path);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

Handoff valid()
{
    const Result<Handoff, HandoffError> r = parseHandoff(fixture("valid.json"));
    REQUIRE(r.hasValue());
    return r.value();
}

}  // namespace

// ---------------------------------------------------------------------------
// Parsing - the golden fixtures
// ---------------------------------------------------------------------------

TEST_CASE("fixture valid.json parses")
{
    const Handoff h = valid();
    CHECK(h.schemaVersion == 1);
    CHECK(h.fromVersion == "0.1.0");
    CHECK(h.toVersion == "0.2.0");
    CHECK(h.stagedNro == "/config/nsx-manager/staging/nsx-manager.nro");
    CHECK(h.targetNro == "/switch/nsx-manager/nsx-manager.nro");
    CHECK(h.attempts == 0);
    CHECK(h.maxAttempts == 3);
    CHECK_FALSE(h.attemptsExhausted());
    CHECK(toHex(h.stagedSha256) ==
          "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08");
}

TEST_CASE("fixture exhausted.json reports a spent budget")
{
    const Result<Handoff, HandoffError> r = parseHandoff(fixture("exhausted.json"));
    REQUIRE(r.hasValue());
    CHECK(r.value().attempts == 3);
    CHECK(r.value().maxAttempts == 3);
    CHECK(r.value().attemptsExhausted());
}

TEST_CASE("fixture relative-path.json is rejected")
{
    // The predecessor's bug, pinned. It opened "forwarder.conf" relative to the
    // working directory, and on failure handed empty strings to rename() and
    // remove(). A relative path in a handoff is not something to tolerate.
    const Result<Handoff, HandoffError> r = parseHandoff(fixture("relative-path.json"));
    REQUIRE_FALSE(r.hasValue());
    CHECK((r.error() == HandoffError::RelativePath || r.error() == HandoffError::PathTraversal));
}

// ---------------------------------------------------------------------------
// Parsing - field validation
// ---------------------------------------------------------------------------

TEST_CASE("malformed documents are rejected, never thrown from")
{
    CHECK_NOTHROW((void)parseHandoff("{ truncated"));
    CHECK(parseHandoff("{ truncated").error() == HandoffError::NotJson);
    CHECK(parseHandoff("[]").error() == HandoffError::NotObject);
    CHECK(parseHandoff("{}").error() == HandoffError::MissingField);
    CHECK(parseHandoff(R"({"schema_version": 2})").error() ==
          HandoffError::UnsupportedSchemaVersion);
}

TEST_CASE("every path must be absolute and free of parent references")
{
    for (const char* bad :
         {"relative/path.nro", "", "./x.nro", "/config/../../switch/x.nro", "C:\\x.nro"}) {
        CAPTURE(bad);
        Handoff h = valid();
        h.stagedNro = bad;
        const Result<Handoff, HandoffError> r = parseHandoff(serializeHandoff(h));
        REQUIRE_FALSE(r.hasValue());
        CHECK(
            (r.error() == HandoffError::RelativePath || r.error() == HandoffError::PathTraversal));
    }
}

TEST_CASE("attempt counters must be sane")
{
    Handoff h = valid();

    h.attempts = -1;
    CHECK(parseHandoff(serializeHandoff(h)).error() == HandoffError::InvalidAttempts);

    h = valid();
    h.maxAttempts = 0;  // an unbounded retry of a failing swap
    CHECK(parseHandoff(serializeHandoff(h)).error() == HandoffError::InvalidAttempts);

    h = valid();
    h.maxAttempts = 100000;
    CHECK(parseHandoff(serializeHandoff(h)).error() == HandoffError::InvalidAttempts);
}

TEST_CASE("the digest must be 64 lowercase hex characters")
{
    const std::string doc = fixture("valid.json");
    const std::string good = "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08";
    for (const std::string bad : {std::string(63, 'a'), std::string(64, 'A'), std::string("")}) {
        std::string corrupted = doc;
        corrupted.replace(corrupted.find(good), good.size(), bad);
        CHECK(parseHandoff(corrupted).error() == HandoffError::InvalidDigest);
    }
}

TEST_CASE("serialize and parse round-trip exactly")
{
    const Handoff original = valid();
    const Result<Handoff, HandoffError> back = parseHandoff(serializeHandoff(original));
    REQUIRE(back.hasValue());
    const Handoff& h = back.value();

    CHECK(h.schemaVersion == original.schemaVersion);
    CHECK(h.createdAtUnix == original.createdAtUnix);
    CHECK(h.fromVersion == original.fromVersion);
    CHECK(h.toVersion == original.toVersion);
    CHECK(h.stagedNro == original.stagedNro);
    CHECK(h.targetNro == original.targetNro);
    CHECK(h.backupNro == original.backupNro);
    CHECK(h.forwarderNro == original.forwarderNro);
    CHECK(h.attempts == original.attempts);
    CHECK(h.maxAttempts == original.maxAttempts);
    CHECK(digestsEqual(h.stagedSha256, original.stagedSha256));
}

TEST_CASE("an incremented attempt count survives a round-trip")
{
    // The forwarder rewrites the handoff before each attempt. If that number
    // did not persist, the retry budget would never be spent and a crashing
    // swap would loop forever.
    Handoff h = valid();
    h.attempts = 2;
    const Result<Handoff, HandoffError> back = parseHandoff(serializeHandoff(h));
    REQUIRE(back.hasValue());
    CHECK(back.value().attempts == 2);
}

// ---------------------------------------------------------------------------
// The recovery table - docs/architecture/self-update-forwarder.md
// ---------------------------------------------------------------------------

TEST_CASE("no handoff and a present target: nothing to do")
{
    const RecoveryDecision d = decideRecovery(std::nullopt, {false, false, true, false});
    CHECK(d.action == RecoveryAction::Nothing);
    CHECK_FALSE(d.mutatesFilesystem());
}

TEST_CASE("staged and untried: proceed")
{
    const RecoveryDecision d = decideRecovery(valid(), {true, true, true, false});
    CHECK(d.action == RecoveryAction::ProceedWithSwap);
    CHECK(d.mutatesFilesystem());
}

TEST_CASE("interrupted mid-swap with budget remaining: retry")
{
    Handoff h = valid();
    h.attempts = 1;
    const RecoveryDecision d = decideRecovery(h, {true, true, true, true});
    CHECK(d.action == RecoveryAction::RetrySwap);
    CHECK(d.reason.find("2 of 3") != std::string::npos);
}

TEST_CASE("budget spent with a backup: roll back")
{
    Handoff h = valid();
    h.attempts = 3;
    const RecoveryDecision d = decideRecovery(h, {true, true, true, true});
    CHECK(d.action == RecoveryAction::RollBack);
    CHECK(d.mutatesFilesystem());
}

TEST_CASE("target vanished between the two renames: restore the backup")
{
    // FatFs cannot rename onto an existing file, so the swap is two renames and
    // the target is briefly absent. This row is why the backup exists.
    const RecoveryDecision d = decideRecovery(valid(), {true, true, false, true});
    CHECK(d.action == RecoveryAction::RestoreBackup);
}

TEST_CASE("no target and no backup, but the staged binary survives: install it")
{
    const RecoveryDecision d = decideRecovery(valid(), {true, true, false, false});
    CHECK(d.action == RecoveryAction::InstallStaged);
}

TEST_CASE("nothing usable left: unrecoverable, and say so")
{
    const RecoveryDecision d = decideRecovery(valid(), {true, false, false, false});
    CHECK(d.action == RecoveryAction::Unrecoverable);
    CHECK_FALSE(d.mutatesFilesystem());
    CHECK_FALSE(d.reason.empty());
}

TEST_CASE("no application at all and no handoff: unrecoverable")
{
    const RecoveryDecision d = decideRecovery(std::nullopt, {false, false, false, false});
    CHECK(d.action == RecoveryAction::Unrecoverable);
}

TEST_CASE("a backup with no handoff still restores")
{
    // A swap that moved the target aside and then lost its instruction file.
    const RecoveryDecision d = decideRecovery(std::nullopt, {false, false, false, true});
    CHECK(d.action == RecoveryAction::RestoreBackup);
}

TEST_CASE("an exhausted budget never retries, whatever else is present")
{
    Handoff h = valid();
    h.attempts = h.maxAttempts;
    for (const FilePresence p :
         {FilePresence{true, true, true, true}, FilePresence{true, true, true, false},
          FilePresence{true, true, false, true}, FilePresence{true, false, true, true}}) {
        const RecoveryDecision d = decideRecovery(h, p);
        CAPTURE(static_cast<int>(d.action));
        CHECK(d.action != RecoveryAction::ProceedWithSwap);
        CHECK(d.action != RecoveryAction::RetrySwap);
    }
}

TEST_CASE("a handoff pointing at a missing staged file does not proceed")
{
    const RecoveryDecision d = decideRecovery(valid(), {true, false, true, false});
    CHECK(d.action == RecoveryAction::Unrecoverable);
    CHECK(d.reason.find("staged") != std::string::npos);
}

TEST_CASE("every error and action has a description")
{
    for (const HandoffError e :
         {HandoffError::NotJson, HandoffError::NotObject, HandoffError::UnsupportedSchemaVersion,
          HandoffError::MissingField, HandoffError::WrongType, HandoffError::RelativePath,
          HandoffError::PathTraversal, HandoffError::InvalidDigest,
          HandoffError::InvalidAttempts}) {
        CHECK_FALSE(describe(e).empty());
        CHECK(describe(e) != "unknown handoff error");
    }
    for (const RecoveryAction a :
         {RecoveryAction::Nothing, RecoveryAction::ProceedWithSwap, RecoveryAction::RetrySwap,
          RecoveryAction::RollBack, RecoveryAction::RestoreBackup, RecoveryAction::InstallStaged,
          RecoveryAction::Unrecoverable}) {
        CHECK_FALSE(describe(a).empty());
        CHECK(describe(a) != "unknown");
    }
}
