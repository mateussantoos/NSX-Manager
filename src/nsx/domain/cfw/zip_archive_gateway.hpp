// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "nsx/domain/ports/ports.hpp"

namespace nsx::domain {

/// @brief @ref ArchiveGateway over `nsx::infra::extractZip`.
///
/// @details A forwarding shim with no logic, for the same reason
///          @ref CurlGateway is one: every decision about where an entry may
///          land belongs to `ExtractionPolicy`, and an adapter that added any
///          of its own would be a second place to audit.
/// @since 0.3.0
class ZipArchiveGateway final : public ArchiveGateway
{
public:
    /// @brief Extract an archive under a destination, applying a policy.
    /// @param archivePath The archive on disk.
    /// @param policy Decides, per entry, where it goes or why it does not.
    /// @param maxUncompressedBytes Ceiling on the declared expanded size.
    /// @param onProgress Optional; returning false aborts.
    /// @return What was extracted, or why it stopped.
    [[nodiscard]] core::Result<infra::ExtractionReport, infra::ArchiveError> extract(
        const std::string& archivePath, const core::ExtractionPolicy& policy,
        std::uint64_t maxUncompressedBytes, const infra::ExtractionCallback& onProgress) override;
};

}  // namespace nsx::domain
