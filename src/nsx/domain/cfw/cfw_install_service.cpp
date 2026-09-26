// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/domain/cfw/cfw_install_service.hpp"

#include <set>
#include <utility>

namespace nsx::domain {

namespace {

using core::ExtractionPolicy;
using core::MergeMarker;
using core::PreserveRules;

/// Join a relative path onto a root without producing a doubled separator.
std::string join(const std::string& root, const std::string& relative)
{
    if (root.empty() || root == "/") {
        return "/" + relative;
    }
    if (root.back() == '/') {
        return root + relative;
    }
    return root + "/" + relative;
}

/// Reported when a rollback could not put everything back. Claiming a clean
/// undo when the restore itself failed would be worse than the original
/// failure: the user would stop looking.
const std::string kPartialRestore =
    "the installation failed and some files could not be put back - reinstall the pack to be "
    "sure";

/// The directory part of a path, or empty when there is none.
std::string parentOf(const std::string& path)
{
    const std::string::size_type slash = path.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
        return {};
    }
    return path.substr(0, slash);
}

}  // namespace

std::string CfwInstallConfig::resolve(core::InstallTarget target) const
{
    switch (target) {
        case core::InstallTarget::SdRoot:
            return sdRoot;
        case core::InstallTarget::AtmosphereContents:
            return atmosphereContents;
        case core::InstallTarget::Bootloader:
            return bootloader;
        case core::InstallTarget::FirmwareStaging:
            return firmwareStaging;
        case core::InstallTarget::SwitchApps:
            return switchApps;
        case core::InstallTarget::Themes:
            return themes;
    }
    // Unreachable: the catalogue parser drops an item whose target it does not
    // recognise, so a value outside the enum never arrives here.
    return sdRoot;
}

std::string_view describe(InstallResult result)
{
    switch (result) {
        case InstallResult::Installed:
            return "installed";
        case InstallResult::InsufficientSpace:
            return "not enough free space";
        case InstallResult::DownloadFailed:
            return "the download failed";
        case InstallResult::VerifyFailed:
            return "the download did not verify";
        case InstallResult::UnsafeArchive:
            return "the package contains a file that would be written outside its destination";
        case InstallResult::ExtractFailed:
            return "the package could not be expanded";
        case InstallResult::MergeFailed:
            return "the installation could not be completed";
        case InstallResult::RolledBack:
            return "the installation failed and was undone";
        case InstallResult::Cancelled:
            return "cancelled";
        case InstallResult::Busy:
            return "another installation is already in progress";
    }
    return "unknown";
}

CfwInstallService::CfwInstallService(HttpGateway& http, ArchiveGateway& archives, FileStore& files,
                                     const Clock& clock, CfwInstallConfig config)
    : m_http(http),
      m_archives(archives),
      m_files(files),
      m_clock(clock),
      m_config(std::move(config))
{
}

// ---------------------------------------------------------------------------
// install()
// ---------------------------------------------------------------------------

InstallOutcome CfwInstallService::install(const core::CatalogItem& item,
                                          const PreserveRules& preserve,
                                          const InstallCallback& onProgress)
{
    InstallOutcome out;

    auto report = [&onProgress](InstallStage stage, std::uint64_t done, std::uint64_t total,
                                std::string detail) {
        if (!onProgress) {
            return true;
        }
        InstallProgress p;
        p.stage = stage;
        p.done = done;
        p.total = total;
        p.detail = std::move(detail);
        return onProgress(p);
    };

    // A merge already in flight means the card is half-changed. Starting a
    // second one would interleave two backups into a state neither could undo.
    if (m_files.exists(m_config.markerPath())) {
        out.result = InstallResult::Busy;
        out.detail = "an interrupted installation must be resolved first";
        return out;
    }

    const std::string stagingDir = m_config.stagingFor(item.id);
    const std::string archivePath = m_config.archiveFor(item.id);
    const std::string backupDir = m_config.backupFor(item.id);
    const std::string destination = m_config.resolve(item.target);

    if (!report(InstallStage::Preflight, 0, 0, item.name)) {
        out.result = InstallResult::Cancelled;
        out.detail = "cancelled";
        return out;
    }

    // Leftovers from a previous attempt: unverified, and nothing resumes them.
    discardStaging(item.id);
    (void)m_files.removeTree(backupDir);

    if (!m_files.makeDirectories(m_config.stagingRoot)) {
        out.result = InstallResult::MergeFailed;
        out.detail = "could not create " + m_config.stagingRoot;
        return out;
    }

    // 1. Pre-flight, from the size the catalogue already published - no extra
    //    request, and refused before the first byte.
    const std::optional<std::uint64_t> free = m_files.freeSpaceBytes(m_config.stagingRoot);
    const std::uint64_t needed =
        item.size +
        (item.size * static_cast<std::uint64_t>(m_config.freeSpaceHeadroomPercent) / 100u);
    if (free.has_value() && *free < needed) {
        out.result = InstallResult::InsufficientSpace;
        out.detail = "need about " + std::to_string(needed / 1024u / 1024u) + " MB free, have " +
                     std::to_string(*free / 1024u / 1024u) + " MB";
        return out;
    }

    // 2. Download. The gateway verifies size and digest before the file gets a
    //    name anything acts on.
    if (!report(InstallStage::Downloading, 0, item.size, item.name)) {
        out.result = InstallResult::Cancelled;
        out.detail = "cancelled";
        return out;
    }

    infra::ExpectedArtifact expected;
    expected.size = item.size;
    expected.sha256 = item.sha256;

    infra::ProgressCallback downloadProgress;
    if (onProgress) {
        downloadProgress = [&report, &item](const infra::Progress& p) {
            return report(InstallStage::Downloading, p.received, p.total, item.name);
        };
    }

    const core::Result<std::monostate, infra::HttpError> downloaded =
        m_http.download(item.url, archivePath, expected, downloadProgress);

    if (!downloaded.hasValue()) {
        const infra::HttpError error = downloaded.error();
        out.transport = error;
        out.result =
            (error == infra::HttpError::DigestMismatch || error == infra::HttpError::SizeMismatch)
                ? InstallResult::VerifyFailed
                : (error == infra::HttpError::Aborted ? InstallResult::Cancelled
                                                      : InstallResult::DownloadFailed);
        out.detail = std::string(infra::describe(error));
        discardStaging(item.id);
        return out;
    }

    // 3. Extract into staging. Still nothing outside staging/ has changed, so a
    //    hostile archive is found while the installation is untouched.
    if (!report(InstallStage::Extracting, 0, 0, item.name)) {
        discardStaging(item.id);
        out.result = InstallResult::Cancelled;
        out.detail = "cancelled";
        return out;
    }

    if (!m_files.makeDirectories(stagingDir)) {
        discardStaging(item.id);
        out.result = InstallResult::ExtractFailed;
        out.detail = "could not create " + stagingDir;
        return out;
    }

    // Preserve rules are applied at the MERGE, not here: the staging tree is a
    // faithful copy of the package, and what the user keeps is decided when
    // something is about to be replaced. Extracting a reduced tree would make
    // the backup incomplete and the rollback wrong.
    const ExtractionPolicy policy(stagingDir, PreserveRules::parse(""));

    infra::ExtractionCallback extractProgress;
    if (onProgress) {
        extractProgress = [&report, &item](const infra::ExtractionProgress& p) {
            return report(InstallStage::Extracting, p.entriesDone, p.entriesTotal,
                          p.currentEntry.empty() ? item.name : p.currentEntry);
        };
    }

    const core::Result<infra::ExtractionReport, infra::ArchiveError> extracted =
        m_archives.extract(archivePath, policy, m_config.maxUncompressedBytes, extractProgress);

    if (!extracted.hasValue()) {
        const infra::ArchiveError error = extracted.error();
        out.result = error == infra::ArchiveError::UnsafeEntry ? InstallResult::UnsafeArchive
                     : error == infra::ArchiveError::Aborted   ? InstallResult::Cancelled
                                                               : InstallResult::ExtractFailed;
        out.detail = std::string(infra::describe(error));
        discardStaging(item.id);
        return out;
    }

    // 4 and 5. An item that does not honour preserve rules gets an empty set
    // rather than a flag checked later: a self-contained download needs to
    // replace its own files, and a rule that skipped one would leave a half
    // installed package behind.
    return mergeStagedTree(item, item.honourPreserveRules ? preserve : PreserveRules::parse(""),
                           stagingDir, destination, onProgress);
}

// ---------------------------------------------------------------------------
// The merge
// ---------------------------------------------------------------------------

InstallOutcome CfwInstallService::mergeStagedTree(const core::CatalogItem& item,
                                                  const PreserveRules& preserve,
                                                  const std::string& stagingDir,
                                                  const std::string& destination,
                                                  const InstallCallback& onProgress)
{
    InstallOutcome out;

    const std::vector<std::string> staged = m_files.listFilesRecursive(stagingDir);
    if (staged.empty()) {
        out.result = InstallResult::ExtractFailed;
        out.detail = "the package expanded to nothing";
        discardStaging(item.id);
        return out;
    }

    MergeMarker marker;
    marker.createdAtUnix = m_clock.nowUnix();
    marker.itemId = item.id;
    marker.itemName = item.name;
    marker.stagingDir = stagingDir;
    marker.destinationRoot = destination;
    marker.backupDir = m_config.backupFor(item.id);

    const std::string document = core::serializeMergeMarker(marker);

    // Read back with the same parser a recovery uses. A configured path this
    // build would reject must fail here, where it can be reported, rather than
    // after a power cut when the marker is all there is.
    if (!core::parseMergeMarker(document).hasValue()) {
        out.result = InstallResult::MergeFailed;
        out.detail = "refusing to start a merge this build could not undo";
        discardStaging(item.id);
        return out;
    }

    // Decide what will actually be written BEFORE writing any of it, and record
    // only that. Written before the first move: a marker with no merge behind
    // it costs a cleanup; a merge with no marker cannot be undone at all.
    //
    // A preserved file must NOT appear in this list. It is in the staging tree
    // and in the package, but the merge deliberately leaves the user's copy
    // alone - and a rollback removes every planned file it did not displace, so
    // listing it here would delete the very file preserving it was meant to
    // protect.
    std::vector<std::string> toWrite;
    toWrite.reserve(staged.size());
    for (const std::string& relative : staged) {
        if (preserve.preserves(relative) && m_files.exists(join(destination, relative))) {
            ++out.filesPreserved;
            continue;
        }
        toWrite.push_back(relative);
    }

    std::string planned;
    for (const std::string& relative : toWrite) {
        planned += relative;
        planned += '\n';
    }
    if (!m_files.writeAtomic(m_config.mergeFileListPath(), planned)) {
        out.result = InstallResult::MergeFailed;
        out.detail = "could not record what the installation would write";
        discardStaging(item.id);
        return out;
    }

    if (!m_files.writeAtomic(m_config.markerPath(), document)) {
        out.result = InstallResult::MergeFailed;
        out.detail = "could not record the installation";
        (void)m_files.remove(m_config.mergeFileListPath());
        discardStaging(item.id);
        return out;
    }

    std::uint64_t done = 0;
    const std::uint64_t total = toWrite.size();

    for (const std::string& relative : toWrite) {
        ++done;

        if (onProgress) {
            InstallProgress p;
            p.stage = InstallStage::Merging;
            p.done = done;
            p.total = total;
            p.detail = relative;
            if (!onProgress(p)) {
                const bool restored = rollBack(marker, toWrite);
                out.result = restored ? InstallResult::Cancelled : InstallResult::MergeFailed;
                out.detail = restored ? std::string("cancelled; the previous files were restored")
                                      : kPartialRestore;
                return out;
            }
        }

        const std::string source = join(stagingDir, relative);
        const std::string target = join(destination, relative);

        if (m_files.exists(target)) {
            const std::string backup = join(marker.backupDir, relative);
            const std::string backupParent = parentOf(backup);
            if (!backupParent.empty() && !m_files.makeDirectories(backupParent)) {
                const bool restored = rollBack(marker, toWrite);
                out.result = restored ? InstallResult::RolledBack : InstallResult::MergeFailed;
                out.detail = restored ? ("could not back up " + relative + "; nothing was changed")
                                      : kPartialRestore;
                return out;
            }
            if (!m_files.rename(target, backup)) {
                const bool restored = rollBack(marker, toWrite);
                out.result = restored ? InstallResult::RolledBack : InstallResult::MergeFailed;
                out.detail = restored
                                 ? ("could not set aside " + relative + "; nothing was changed")
                                 : kPartialRestore;
                return out;
            }
            ++out.filesReplaced;
        }

        const std::string targetParent = parentOf(target);
        if (!targetParent.empty() && !m_files.makeDirectories(targetParent)) {
            const bool restored = rollBack(marker, toWrite);
            out.result = restored ? InstallResult::RolledBack : InstallResult::MergeFailed;
            out.detail =
                restored ? ("could not create a directory for " + relative) : kPartialRestore;
            return out;
        }

        if (!m_files.rename(source, target)) {
            const bool restored = rollBack(marker, toWrite);
            out.result = restored ? InstallResult::RolledBack : InstallResult::MergeFailed;
            out.detail =
                restored ? ("could not install " + relative + "; the previous files were restored")
                         : kPartialRestore;
            return out;
        }

        ++out.filesMerged;
    }

    commit(marker);

    out.result = InstallResult::Installed;
    out.detail =
        "installed " + item.name + (item.versionText.empty() ? "" : " " + item.versionText);
    return out;
}

bool CfwInstallService::rollBack(const MergeMarker& marker, const std::vector<std::string>& planned)
{
    bool complete = true;

    // Captured BEFORE restoring anything. The restore below moves each backup
    // back into place, which removes it from the backup directory - so asking
    // afterwards whether a file "had a backup" would answer no for every file
    // that was just successfully restored, and the loop after this one would
    // then delete it again.
    const std::vector<std::string> backedUp = m_files.listFilesRecursive(marker.backupDir);
    const std::set<std::string> displaced(backedUp.begin(), backedUp.end());

    for (const std::string& relative : backedUp) {
        const std::string from = join(marker.backupDir, relative);
        const std::string to = join(marker.destinationRoot, relative);

        const std::string parent = parentOf(to);
        if (!parent.empty()) {
            (void)m_files.makeDirectories(parent);
        }
        if (!m_files.rename(from, to)) {
            complete = false;
        }
    }

    // Anything the merge CREATED has no backup to restore over it, so it must
    // be removed by name.
    //
    // The hard part is telling those apart from planned files the merge never
    // reached - a cancellation or a power cut stops part way, and the files
    // after that point are still the user's own. Both cases look identical at
    // the destination: present, with no backup.
    //
    // The staging tree decides it. The merge MOVES a file out of staging, so a
    // staged copy that is still there means that file was never installed, and
    // whatever sits at the destination is the original. Deleting it would
    // destroy a file this rollback exists to protect.
    for (const std::string& relative : planned) {
        if (displaced.contains(relative)) {
            continue;  // restored above
        }
        if (m_files.exists(join(marker.stagingDir, relative))) {
            continue;  // never installed; the destination is untouched
        }
        (void)m_files.remove(join(marker.destinationRoot, relative));
    }

    (void)m_files.removeTree(marker.backupDir);
    (void)m_files.removeTree(marker.stagingDir);
    (void)m_files.remove(m_config.archiveFor(marker.itemId));
    (void)m_files.remove(m_config.mergeFileListPath());
    (void)m_files.remove(m_config.markerPath());

    return complete;
}

std::vector<std::string> CfwInstallService::readPlannedFiles() const
{
    std::vector<std::string> out;

    // A pack is thousands of files, so this can be large - but it is read once,
    // during a recovery, and never during a normal run.
    const std::optional<std::string> raw =
        m_files.readText(m_config.mergeFileListPath(), 8u * 1024u * 1024u);
    if (!raw.has_value()) {
        return out;
    }

    std::string::size_type start = 0;
    while (start <= raw->size()) {
        const std::string::size_type newline = raw->find('\n', start);
        const std::string::size_type stop = newline == std::string::npos ? raw->size() : newline;
        if (stop > start) {
            out.push_back(raw->substr(start, stop - start));
        }
        if (newline == std::string::npos) {
            break;
        }
        start = newline + 1;
    }
    return out;
}

void CfwInstallService::commit(const MergeMarker& marker)
{
    // The backup goes first: while it exists the installation is still
    // reversible, and the marker is what says so. Removing the marker before
    // the backup would leave a directory nothing would ever clean up.
    (void)m_files.removeTree(marker.backupDir);
    (void)m_files.removeTree(marker.stagingDir);
    (void)m_files.remove(m_config.archiveFor(marker.itemId));
    (void)m_files.remove(m_config.mergeFileListPath());
    (void)m_files.remove(m_config.markerPath());
}

// ---------------------------------------------------------------------------
// Recovery
// ---------------------------------------------------------------------------

InstallOutcome CfwInstallService::recoverInterruptedMerge()
{
    InstallOutcome out;

    core::MergePresence present;
    present.marker = m_files.exists(m_config.markerPath());

    std::optional<MergeMarker> marker;
    if (present.marker) {
        const std::optional<std::string> raw = m_files.readText(m_config.markerPath(), 64u * 1024u);
        if (raw.has_value()) {
            const core::Result<MergeMarker, core::MergeMarkerError> parsed =
                core::parseMergeMarker(*raw);
            if (parsed.hasValue()) {
                marker = parsed.value();
                present.staging = !m_files.listFilesRecursive(marker->stagingDir).empty();
                present.backup = !m_files.listFilesRecursive(marker->backupDir).empty();
                present.destination = true;
            }
        }
    }

    const core::MergeRecoveryDecision decision = core::decideMergeRecovery(marker, present);

    switch (decision.action) {
        case core::MergeRecovery::Nothing:
            out.result = InstallResult::Installed;  // nothing was wrong
            out.detail = decision.reason;
            return out;

        case core::MergeRecovery::Unrecoverable:
            out.result = InstallResult::MergeFailed;
            out.detail = decision.reason;
            return out;

        case core::MergeRecovery::CleanUpOnly:
            if (marker.has_value()) {
                (void)m_files.removeTree(marker->stagingDir);
                (void)m_files.removeTree(marker->backupDir);
                (void)m_files.remove(m_config.archiveFor(marker->itemId));
            }
            (void)m_files.remove(m_config.mergeFileListPath());
            (void)m_files.remove(m_config.markerPath());
            out.result = InstallResult::RolledBack;
            out.detail = decision.reason;
            return out;

        case core::MergeRecovery::RollBack:
            break;
    }

    const std::size_t toRestore = m_files.listFilesRecursive(marker->backupDir).size();
    const std::vector<std::string> planned = readPlannedFiles();
    const bool complete = rollBack(*marker, planned);

    out.filesReplaced = toRestore;
    out.result = complete ? InstallResult::RolledBack : InstallResult::MergeFailed;

    if (!complete) {
        out.detail = "some files could not be restored; reinstall the pack to be sure";
    }
    else if (planned.empty()) {
        // Everything displaced went back, but without the planned list there is
        // no way to name what the pack created fresh, so some of its files may
        // remain. Say so rather than reporting a clean undo.
        out.detail = decision.reason +
                     " - the list of installed files was lost, so some of the new "
                     "package may remain";
    }
    else {
        out.detail = decision.reason;
    }
    return out;
}

void CfwInstallService::discardStaging(const std::string& itemId)
{
    (void)m_files.removeTree(m_config.stagingFor(itemId));
    (void)m_files.remove(m_config.archiveFor(itemId) + ".part");
    (void)m_files.remove(m_config.archiveFor(itemId));
}

}  // namespace nsx::domain
