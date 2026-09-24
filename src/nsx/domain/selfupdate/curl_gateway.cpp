// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/domain/selfupdate/curl_gateway.hpp"

namespace nsx::domain {

core::Result<infra::Response, infra::HttpError> CurlGateway::fetch(const std::string& url,
                                                                   std::string_view ifNoneMatch)
{
    return m_client.get(url, 1u << 20u, ifNoneMatch);
}

core::Result<std::monostate, infra::HttpError> CurlGateway::download(
    const std::string& url, const std::string& destPath, const infra::ExpectedArtifact& expected,
    const infra::ProgressCallback& onProgress)
{
    return m_client.download(url, destPath, expected, onProgress);
}

}  // namespace nsx::domain
