// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "nsx/domain/selfupdate/ports.hpp"

namespace nsx::domain {

/// @brief @ref HttpGateway over @ref nsx::infra::CurlClient.
///
/// @details Deliberately a forwarding shim with no logic of its own. Every
///          security decision - peer and host verification, TLS 1.2, https-only
///          through redirects, digest-before-rename - belongs to `CurlClient`
///          and stays auditable in that one file. An adapter that made
///          decisions here would be a second place to check.
///
/// @see ADR-0016
/// @since 0.2.0
class CurlGateway final : public HttpGateway
{
public:
    /// @brief Fetch a small document.
    /// @param url Must be https.
    /// @param ifNoneMatch ETag for a conditional request; empty for none.
    /// @return The response, or why it failed.
    [[nodiscard]] core::Result<infra::Response, infra::HttpError> fetch(
        const std::string& url, std::string_view ifNoneMatch) override;

    /// @brief Download and verify a file.
    /// @param url Must be https.
    /// @param destPath Where the verified file should end up.
    /// @param expected Size and digest the content must match.
    /// @param onProgress Optional; returning false aborts.
    /// @return Nothing on success, or why it failed.
    [[nodiscard]] core::Result<std::monostate, infra::HttpError> download(
        const std::string& url, const std::string& destPath,
        const infra::ExpectedArtifact& expected,
        const infra::ProgressCallback& onProgress) override;

private:
    infra::CurlClient m_client;
};

}  // namespace nsx::domain
