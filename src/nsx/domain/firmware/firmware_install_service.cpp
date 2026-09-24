// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/domain/firmware/firmware_install_service.hpp"

#include <utility>

#include "nsx/core/paths/preserve_rules.hpp"

namespace nsx::domain {

namespace {

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

}  // namespace

std::string_view describe(FirmwareResult result)
{
    switch (result) {
        case FirmwareResult::Ready:
            return "firmware ready to install";
        case FirmwareResult::NotFirmwareItem:
            return "that catalogue item is not a firmware set";
        case FirmwareResult::InsufficientSpace:
            return "not enough free space";
        case FirmwareResult::DownloadFailed:
            return "the download failed";
        case FirmwareResult::VerifyFailed:
            return "the download did not verify";
        case FirmwareResult::UnsafeArchive:
            return "the firmware archive contains a file that would be written outside /firmware";
        case FirmwareResult::ExtractFailed:
            return "the firmware archive could not be expanded";
        case FirmwareResult::DirectoryNotOurs:
            return "/firmware contains files this application will not delete";
        case FirmwareResult::ClearFailed:
            return "the previous firmware set could not be removed";
        case FirmwareResult::NotFirmwareContent:
            return "that archive does not contain a firmware set";
        case FirmwareResult::DaybreakMissing:
            return "Daybreak is not installed";
        case FirmwareResult::Cancelled:
            return "cancelled";
    }
    return "unknown";
}

FirmwareInstallService::FirmwareInstallService(HttpGateway& http, ArchiveGateway& archives,
                                               FileStore& files, FirmwareConfig config)
    : m_http(http), m_archives(archives), m_files(files), m_config(std::move(config))
{
}

core::FirmwareDirectoryScan FirmwareInstallService::inspect() const
{
    return core::scanFirmwareDirectory(m_files.listFilesRecursive(m_config.firmwareDir));
}

void FirmwareInstallService::discardStaging(const std::string& itemId)
{
    (void)m_files.remove(m_config.archiveFor(itemId) + ".part");
    (void)m_files.remove(m_config.archiveFor(itemId));
}

FirmwareOutcome FirmwareInstallService::stage(const core::CatalogItem& item,
                                              const FirmwareCallback& onProgress)
{
    FirmwareOutcome out;

    auto report = [&onProgress](FirmwareStage stage, std::uint64_t done, std::uint64_t total,
                                std::string detail) {
        if (!onProgress) {
            return true;
        }
        FirmwareProgress p;
        p.stage = stage;
        p.done = done;
        p.total = total;
        p.detail = std::move(detail);
        return onProgress(p);
    };

    if (item.kind != core::ContentKind::Firmware) {
        out.result = FirmwareResult::NotFirmwareItem;
        out.detail = "'" + item.name + "' is not a firmware set";
        return out;
    }

    if (!report(FirmwareStage::Preflight, 0, 0, item.name)) {
        out.result = FirmwareResult::Cancelled;
        out.detail = "cancelled";
        return out;
    }

    // 1. Look at what is already there BEFORE downloading anything. A user with
    //    an unexpected file in /firmware/ should find that out now, not after
    //    waiting for 400 MB.
    const core::FirmwareDirectoryScan existing = inspect();
    if (!existing.onlyFirmware()) {
        out.result = FirmwareResult::DirectoryNotOurs;
        out.unexpectedFiles = existing.unexpected;
        out.detail = "/firmware contains " + std::to_string(existing.unexpected.size()) +
                     " file(s) this application did not put there, starting with '" +
                     existing.unexpected.front() + "'. Move them out and try again.";
        return out;
    }

    // Daybreak is what actually installs this. Finding it missing after the
    // download would mean a set staged with nothing able to use it.
    if (!m_files.exists(m_config.daybreakNro)) {
        out.result = FirmwareResult::DaybreakMissing;
        out.detail = "install Daybreak at " + m_config.daybreakNro + " first";
        return out;
    }

    discardStaging(item.id);
    if (!m_files.makeDirectories(m_config.stagingRoot)) {
        out.result = FirmwareResult::ExtractFailed;
        out.detail = "could not create " + m_config.stagingRoot;
        return out;
    }

    const std::optional<std::uint64_t> free = m_files.freeSpaceBytes(m_config.stagingRoot);
    const std::uint64_t needed =
        item.size +
        (item.size * static_cast<std::uint64_t>(m_config.freeSpaceHeadroomPercent) / 100u);
    if (free.has_value() && *free < needed) {
        out.result = FirmwareResult::InsufficientSpace;
        out.detail = "need about " + std::to_string(needed / 1024u / 1024u) + " MB free, have " +
                     std::to_string(*free / 1024u / 1024u) + " MB";
        return out;
    }

    // 2. Download and verify. Still nothing has been removed.
    if (!report(FirmwareStage::Downloading, 0, item.size, item.name)) {
        out.result = FirmwareResult::Cancelled;
        out.detail = "cancelled";
        return out;
    }

    infra::ExpectedArtifact expected;
    expected.size = item.size;
    expected.sha256 = item.sha256;

    infra::ProgressCallback downloadProgress;
    if (onProgress) {
        downloadProgress = [&report, &item](const infra::Progress& p) {
            return report(FirmwareStage::Downloading, p.received, p.total, item.name);
        };
    }

    const std::string archivePath = m_config.archiveFor(item.id);
    const core::Result<std::monostate, infra::HttpError> downloaded =
        m_http.download(item.url, archivePath, expected, downloadProgress);

    if (!downloaded.hasValue()) {
        const infra::HttpError error = downloaded.error();
        out.transport = error;
        out.result =
            (error == infra::HttpError::DigestMismatch || error == infra::HttpError::SizeMismatch)
                ? FirmwareResult::VerifyFailed
                : (error == infra::HttpError::Aborted ? FirmwareResult::Cancelled
                                                      : FirmwareResult::DownloadFailed);
        out.detail = std::string(infra::describe(error));
        discardStaging(item.id);
        return out;
    }

    // 3. ONLY NOW is the previous set removed.
    //
    // A firmware set is replaced whole rather than merged: NCAs from two
    // versions in one directory is how Daybreak installs a system that does not
    // boot. There is no backup, which is why this waits until the replacement
    // is downloaded and verified - a failed download must never cost the user
    // the firmware they already had.
    if (!report(FirmwareStage::Clearing, 0, existing.firmwareFiles, m_config.firmwareDir)) {
        discardStaging(item.id);
        out.result = FirmwareResult::Cancelled;
        out.detail = "cancelled";
        return out;
    }

    for (const std::string& relative : m_files.listFilesRecursive(m_config.firmwareDir)) {
        if (!m_files.remove(join(m_config.firmwareDir, relative))) {
            out.result = FirmwareResult::ClearFailed;
            out.detail = "could not remove " + relative + " from " + m_config.firmwareDir;
            discardStaging(item.id);
            return out;
        }
        ++out.previousFilesRemoved;
    }

    if (!m_files.makeDirectories(m_config.firmwareDir)) {
        out.result = FirmwareResult::ClearFailed;
        out.detail = "could not create " + m_config.firmwareDir;
        discardStaging(item.id);
        return out;
    }

    // 4. Extract straight into the firmware directory. Every entry still goes
    //    through the extraction policy, so an archive claiming to be firmware
    //    cannot use this path to write somewhere else. No preserve rules: the
    //    directory was just emptied, and there is nothing of the user's in it.
    if (!report(FirmwareStage::Extracting, 0, 0, item.name)) {
        discardStaging(item.id);
        out.result = FirmwareResult::Cancelled;
        out.detail = "cancelled";
        return out;
    }

    const core::ExtractionPolicy policy(m_config.firmwareDir, core::PreserveRules::parse(""));

    infra::ExtractionCallback extractProgress;
    if (onProgress) {
        extractProgress = [&report, &item](const infra::ExtractionProgress& p) {
            return report(FirmwareStage::Extracting, p.entriesDone, p.entriesTotal,
                          p.currentEntry.empty() ? item.name : p.currentEntry);
        };
    }

    const core::Result<infra::ExtractionReport, infra::ArchiveError> extracted =
        m_archives.extract(archivePath, policy, m_config.maxUncompressedBytes, extractProgress);

    if (!extracted.hasValue()) {
        const infra::ArchiveError error = extracted.error();
        out.result = error == infra::ArchiveError::UnsafeEntry ? FirmwareResult::UnsafeArchive
                     : error == infra::ArchiveError::Aborted   ? FirmwareResult::Cancelled
                                                               : FirmwareResult::ExtractFailed;
        out.detail = std::string(infra::describe(error));
        discardStaging(item.id);
        return out;
    }

    // 5. Confirm what landed is actually a firmware set. An archive that
    //    expands to no NCA is not one, whatever the catalogue called it, and
    //    handing Daybreak an empty directory would waste a reboot to discover.
    const std::vector<std::string> staged = m_files.listFilesRecursive(m_config.firmwareDir);
    if (!core::looksLikeFirmware(staged)) {
        out.result = FirmwareResult::NotFirmwareContent;
        out.detail = staged.empty() ? "the archive expanded to nothing"
                                    : "the archive contains no firmware content";
        discardStaging(item.id);
        return out;
    }

    discardStaging(item.id);

    out.result = FirmwareResult::Ready;
    out.filesStaged = staged.size();
    out.firmwareDir = m_config.firmwareDir;

    // The argv Daybreak expects: its own path, then the directory to install
    // from. Taken from the predecessor, which shipped this working
    // (dialogue_page.cpp:133) - not guessed at.
    out.daybreakArgs = "\"" + m_config.daybreakNro + "\" \"" + m_config.firmwareDir + "\"";
    out.detail = "firmware " + (item.versionText.empty() ? item.name : item.versionText) +
                 " is ready; Daybreak will install it";
    return out;
}

}  // namespace nsx::domain
