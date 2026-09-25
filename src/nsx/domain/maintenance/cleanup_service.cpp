// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/domain/maintenance/cleanup_service.hpp"

#include <utility>
#include <vector>

namespace nsx::domain {

CleanupService::CleanupService(FileStore& files, CleanupConfig config)
    : m_files(files), m_config(std::move(config))
{
}

std::size_t CleanupService::countStagedFiles() const
{
    const std::vector<std::string> files = m_files.listFilesRecursive(m_config.stagingDir);
    return files.size();
}

CleanupReport CleanupService::cleanStaging()
{
    CleanupReport report;

    // Safety guard: staging must not be empty or root, and must not contain the cache directory.
    if (m_config.stagingDir.empty() || m_config.stagingDir == "/" ||
        m_config.stagingDir == "/config") {
        report.success = false;
        report.detail = "refusing to clean an unsafe staging root path";
        return report;
    }

    if (m_config.cacheDir.starts_with(m_config.stagingDir) ||
        m_config.stagingDir == m_config.cacheDir) {
        report.success = false;
        report.detail = "staging path conflicts with cache directory";
        return report;
    }

    // List all files inside staging to report accurate count.
    const std::vector<std::string> files = m_files.listFilesRecursive(m_config.stagingDir);
    report.filesRemoved = files.size();

    // Remove the entire staging directory tree.
    if (!m_files.removeTree(m_config.stagingDir)) {
        report.success = false;
        report.detail = "failed to remove staging tree";
        return report;
    }

    report.success = true;
    report.detail = "cleaned " + std::to_string(report.filesRemoved) + " file(s) from staging";
    return report;
}

}  // namespace nsx::domain
