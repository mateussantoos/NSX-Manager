// SPDX-License-Identifier: GPL-3.0-only

#include "swap.hpp"

#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#include "nsx/core/hash/sha256.hpp"

namespace nsx::forwarder {

namespace {

using core::Handoff;
using core::RecoveryAction;
using core::Sha256;

constexpr std::size_t kIoChunk = 64 * 1024;

bool exists(const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    std::fclose(f);
    return true;
}

std::optional<std::string> readFile(const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return std::nullopt;
    }
    std::string out;
    std::vector<char> buf(kIoChunk);
    while (const std::size_t n = std::fread(buf.data(), 1, buf.size(), f)) {
        out.append(buf.data(), n);
    }
    const bool ok = std::ferror(f) == 0;
    std::fclose(f);
    return ok ? std::optional<std::string>(std::move(out)) : std::nullopt;
}

/// Write, flush, and only then rename into place.
///
/// The temporary-then-rename shape matters for the handoff specifically: a
/// half-written handoff after a power cut would be unparseable, and an
/// unparseable handoff is indistinguishable from no handoff at all - which
/// would silently abandon an update that was actually in flight.
bool writeFileAtomic(const std::string& path, const std::string& data)
{
    const std::string tmp = path + ".tmp";

    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    const bool written = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    const bool flushed = std::fflush(f) == 0;
    std::fclose(f);

    if (!written || !flushed) {
        std::remove(tmp.c_str());
        return false;
    }

    // FatFs cannot rename onto an existing file, so clear the way first.
    std::remove(path.c_str());
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

bool copyFile(const std::string& from, const std::string& to)
{
    FILE* in = std::fopen(from.c_str(), "rb");
    if (in == nullptr) {
        return false;
    }
    FILE* out = std::fopen(to.c_str(), "wb");
    if (out == nullptr) {
        std::fclose(in);
        return false;
    }

    std::vector<char> buf(kIoChunk);
    bool ok = true;
    while (true) {
        const std::size_t n = std::fread(buf.data(), 1, buf.size(), in);
        if (n == 0) {
            ok = std::ferror(in) == 0;
            break;
        }
        if (std::fwrite(buf.data(), 1, n, out) != n) {
            ok = false;
            break;
        }
    }

    ok = ok && (std::fflush(out) == 0);
    std::fclose(in);
    std::fclose(out);

    if (!ok) {
        std::remove(to.c_str());
    }
    return ok;
}

/// Hash a file in fixed-size chunks rather than reading it whole: an NRO is
/// megabytes and this runs on a console with a modest homebrew heap.
std::optional<Sha256::Digest> hashFile(const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return std::nullopt;
    }

    Sha256 hasher;
    std::vector<char> buf(kIoChunk);
    while (const std::size_t n = std::fread(buf.data(), 1, buf.size(), f)) {
        hasher.update(buf.data(), n);
    }
    const bool ok = std::ferror(f) == 0;
    std::fclose(f);

    if (!ok) {
        return std::nullopt;
    }
    return hasher.finish();
}

/// FatFs cannot overwrite, so every rename clears its destination first.
bool renameOver(const std::string& from, const std::string& to)
{
    std::remove(to.c_str());
    return std::rename(from.c_str(), to.c_str()) == 0;
}

void cleanUp(const Handoff& h, const std::string& handoffPath)
{
    std::remove(h.backupNro.c_str());
    std::remove(h.stagedNro.c_str());
    std::remove(handoffPath.c_str());
}

SwapOutcome fail(SwapResult result, std::string detail)
{
    return SwapOutcome{result, std::move(detail)};
}

/// Put the previous binary back and abandon the update.
SwapOutcome rollBack(const Handoff& h, const std::string& handoffPath, const std::string& why)
{
    if (exists(h.backupNro) && !exists(h.targetNro)) {
        if (!renameOver(h.backupNro, h.targetNro)) {
            return fail(SwapResult::IoFailed, "could not restore the backup: " + h.backupNro);
        }
    }
    cleanUp(h, handoffPath);
    return SwapOutcome{SwapResult::RolledBack, why};
}

}  // namespace

SwapOutcome runSwap(const std::string& handoffPath)
{
    const std::optional<std::string> raw = readFile(handoffPath);

    std::optional<Handoff> handoff;
    if (raw.has_value()) {
        const core::Result<Handoff, core::HandoffError> parsed = core::parseHandoff(*raw);
        if (parsed.hasValue()) {
            handoff = parsed.value();
        }
        else {
            // A malformed handoff is not a licence to guess. Leave the
            // installation exactly as it is and report why.
            return fail(SwapResult::Unrecoverable, std::string(core::describe(parsed.error())));
        }
    }

    core::FilePresence present;
    present.handoff = raw.has_value();
    if (handoff.has_value()) {
        present.staged = exists(handoff->stagedNro);
        present.target = exists(handoff->targetNro);
        present.backup = exists(handoff->backupNro);
    }

    const core::RecoveryDecision decision = core::decideRecovery(handoff, present);

    switch (decision.action) {
        case RecoveryAction::Nothing:
            return SwapOutcome{SwapResult::NothingToDo, decision.reason};

        case RecoveryAction::Unrecoverable:
            return fail(SwapResult::Unrecoverable, decision.reason);

        case RecoveryAction::RollBack:
            return rollBack(*handoff, handoffPath, decision.reason);

        case RecoveryAction::RestoreBackup:
            if (!renameOver(handoff->backupNro, handoff->targetNro)) {
                return fail(SwapResult::IoFailed, "could not restore the backup");
            }
            cleanUp(*handoff, handoffPath);
            return SwapOutcome{SwapResult::Restored, decision.reason};

        case RecoveryAction::ProceedWithSwap:
        case RecoveryAction::RetrySwap:
        case RecoveryAction::InstallStaged:
            break;  // handled below
    }

    Handoff h = *handoff;

    // Step 2, BEFORE anything risky. If this does not persist, a crash in the
    // verification or the copy leaves the counter where it was and the same
    // failing swap repeats forever.
    h.attempts += 1;
    if (!writeFileAtomic(handoffPath, core::serializeHandoff(h))) {
        return fail(SwapResult::IoFailed, "could not record the attempt counter");
    }

    // Step 3. The application already verified this file after downloading it;
    // re-checking here catches corruption that happened since - a bad card, an
    // unclean shutdown, another program writing into the staging directory.
    const std::optional<Sha256::Digest> actual = hashFile(h.stagedNro);
    if (!actual.has_value()) {
        return fail(SwapResult::IoFailed, "could not read the staged binary");
    }
    if (!core::digestsEqual(*actual, h.stagedSha256)) {
        // Do not touch the target. The installation stays exactly as it was.
        return fail(SwapResult::DigestMismatch,
                    "staged binary does not match its recorded digest; the update was "
                    "discarded and your current version is untouched");
    }

    // Step 4.
    const std::string incoming = h.targetNro + ".new";
    if (!copyFile(h.stagedNro, incoming)) {
        std::remove(incoming.c_str());
        return fail(SwapResult::IoFailed, "could not stage the new binary next to the target");
    }

    // Steps 5 and 6. Two renames, because FatFs cannot rename onto an existing
    // file. The target is briefly absent between them; that window is covered
    // by the backup and by decideRecovery on the next boot.
    if (exists(h.targetNro) && !renameOver(h.targetNro, h.backupNro)) {
        std::remove(incoming.c_str());
        return fail(SwapResult::IoFailed, "could not move the current version aside");
    }
    if (!renameOver(incoming, h.targetNro)) {
        // The target is gone and the new one did not land. Put the old one back
        // immediately rather than waiting for the next boot to notice.
        if (exists(h.backupNro)) {
            renameOver(h.backupNro, h.targetNro);
        }
        std::remove(incoming.c_str());
        return fail(SwapResult::IoFailed, "could not move the new version into place");
    }

    // Step 7. Confirm before destroying the only copy of the old binary.
    const std::optional<Sha256::Digest> installed = hashFile(h.targetNro);
    if (!installed.has_value() || !core::digestsEqual(*installed, h.stagedSha256)) {
        if (exists(h.backupNro)) {
            renameOver(h.backupNro, h.targetNro);
        }
        return fail(SwapResult::IoFailed,
                    "the installed binary did not verify; the previous version was restored");
    }

    cleanUp(h, handoffPath);
    return SwapOutcome{SwapResult::Installed, "updated to " + h.toVersion};
}

std::string_view describe(SwapResult result)
{
    switch (result) {
        case SwapResult::NothingToDo:
            return "nothing to do";
        case SwapResult::Installed:
            return "update installed";
        case SwapResult::RolledBack:
            return "update failed and was rolled back";
        case SwapResult::Restored:
            return "previous version restored";
        case SwapResult::DigestMismatch:
            return "the staged update did not verify";
        case SwapResult::IoFailed:
            return "a file operation failed";
        case SwapResult::Unrecoverable:
            return "cannot recover automatically";
    }
    return "unknown";
}

}  // namespace nsx::forwarder
