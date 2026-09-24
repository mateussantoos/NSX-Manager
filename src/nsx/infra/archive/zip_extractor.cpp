// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/infra/archive/zip_extractor.hpp"

#include <fstream>

#include <sys/stat.h>
#include <sys/types.h>
#include <unzipper.h>

namespace nsx::infra {

namespace {

using core::EntryAction;
using core::EntryDecision;

/// Create a directory and its parents. mkdir on an existing directory fails
/// with EEXIST, which is success here, so the outcome is judged by stat rather
/// than by any return code.
bool makeDirectories(const std::string& path)
{
    if (path.empty()) {
        return false;
    }

    std::string partial;
    partial.reserve(path.size());

    for (std::string::size_type i = 0; i < path.size(); ++i) {
        partial.push_back(path[i]);
        const bool atSeparator = path[i] == '/' && i > 0;
        const bool atEnd = i + 1 == path.size();
        if (!atSeparator && !atEnd) {
            continue;
        }

        std::string component = partial;
        if (atSeparator) {
            component.pop_back();
        }
        if (component.empty() || component.back() == ':') {
            continue;
        }
        (void)::mkdir(component.c_str(), 0777);
    }

    struct stat st
    {
    };

    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

/// The directory part of a file path, or empty when there is none.
std::string parentOf(const std::string& path)
{
    const std::string::size_type slash = path.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
        return {};
    }
    return path.substr(0, slash);
}

}  // namespace

std::string_view describe(ArchiveError error)
{
    switch (error) {
        case ArchiveError::CannotOpen:
            return "the archive could not be opened";
        case ArchiveError::Empty:
            return "the archive is empty";
        case ArchiveError::UnsafeEntry:
            return "the archive contains an entry that would write outside the destination";
        case ArchiveError::TooLarge:
            return "the archive expands to more than the allowed size";
        case ArchiveError::DirectoryFailed:
            return "a destination directory could not be created";
        case ArchiveError::WriteFailed:
            return "a file could not be written";
        case ArchiveError::ReadFailed:
            return "an entry could not be decompressed";
        case ArchiveError::Aborted:
            return "cancelled";
    }
    return "unknown archive error";
}

core::Result<ExtractionReport, ArchiveError> extractZip(const std::string& archivePath,
                                                        const core::ExtractionPolicy& policy,
                                                        std::uint64_t maxUncompressedBytes,
                                                        const ExtractionCallback& onProgress)
{
    using R = core::Result<ExtractionReport, ArchiveError>;

    ExtractionReport report;

    std::vector<zipper::ZipEntry> entries;
    {
        // Scoped so the archive is closed before anything else happens - the
        // console has a modest file handle budget and a failure path that
        // leaves one open is hard to notice.
        zipper::Unzipper archive(archivePath);
        entries = archive.entries();
        if (entries.empty()) {
            archive.close();
            return R::err(ArchiveError::Empty);
        }
        archive.close();
    }

    // PASS ONE: decide everything before writing anything.
    //
    // The whole archive is validated up front so a hostile entry is found
    // before a single byte has been written, rather than half way through when
    // the destination already holds files from an archive we have just decided
    // not to trust.
    std::vector<EntryDecision> decisions;
    decisions.reserve(entries.size());

    std::uint64_t declaredTotal = 0;
    bool refused = false;

    for (const zipper::ZipEntry& entry : entries) {
        const EntryDecision decision = policy.decide(entry.name);
        if (decision.action == EntryAction::Reject) {
            report.rejected.push_back(decision.reason);
            refused = true;
        }
        declaredTotal += entry.uncompressedSize;
        decisions.push_back(decision);
    }

    if (refused) {
        // One traversal entry condemns the archive. See the header.
        return R::err(ArchiveError::UnsafeEntry);
    }

    // Read from the directory, so a decompression bomb is refused before it is
    // expanded rather than after it has filled the card.
    if (maxUncompressedBytes > 0 && declaredTotal > maxUncompressedBytes) {
        return R::err(ArchiveError::TooLarge);
    }

    // PASS TWO: carry it out.
    zipper::Unzipper archive(archivePath);

    ExtractionProgress progress;
    progress.entriesTotal = entries.size();

    auto fail = [&archive](ArchiveError e) {
        archive.close();
        return R::err(e);
    };

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const zipper::ZipEntry& entry = entries[i];
        const EntryDecision& decision = decisions[i];

        progress.entriesDone = i;
        progress.currentEntry = entry.name;
        progress.bytesWritten = report.bytesWritten;

        if (onProgress && !onProgress(progress)) {
            return fail(ArchiveError::Aborted);
        }

        if (decision.action == EntryAction::SkipPreserved) {
            ++report.skippedPreserved;
            continue;
        }

        if (decision.action == EntryAction::CreateDirectory) {
            if (!makeDirectories(decision.destination)) {
                return fail(ArchiveError::DirectoryFailed);
            }
            ++report.directoriesCreated;
            continue;
        }

        // A file whose parent directory the archive never declared.
        const std::string parent = parentOf(decision.destination);
        if (!parent.empty() && !makeDirectories(parent)) {
            return fail(ArchiveError::DirectoryFailed);
        }

        std::ofstream out(decision.destination, std::ios::binary | std::ios::trunc);
        if (!out.good()) {
            return fail(ArchiveError::WriteFailed);
        }

        // Streamed, not extractEntryToMemory: a CFW pack contains files far
        // larger than a console's homebrew heap wants to hold at once.
        if (!archive.extractEntryToStream(entry.name, out)) {
            out.close();
            std::remove(decision.destination.c_str());
            return fail(ArchiveError::ReadFailed);
        }

        out.flush();
        const bool written = out.good();
        out.close();

        if (!written) {
            std::remove(decision.destination.c_str());
            return fail(ArchiveError::WriteFailed);
        }

        ++report.filesWritten;
        report.bytesWritten += entry.uncompressedSize;
    }

    archive.close();

    progress.entriesDone = entries.size();
    progress.bytesWritten = report.bytesWritten;
    progress.currentEntry.clear();
    if (onProgress) {
        (void)onProgress(progress);
    }

    return R::ok(std::move(report));
}

}  // namespace nsx::infra
