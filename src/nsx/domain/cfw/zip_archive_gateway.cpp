// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/domain/cfw/zip_archive_gateway.hpp"

namespace nsx::domain {

core::Result<infra::ExtractionReport, infra::ArchiveError> ZipArchiveGateway::extract(
    const std::string& archivePath, const core::ExtractionPolicy& policy,
    std::uint64_t maxUncompressedBytes, const infra::ExtractionCallback& onProgress)
{
    return infra::extractZip(archivePath, policy, maxUncompressedBytes, onProgress);
}

}  // namespace nsx::domain
