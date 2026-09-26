// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/net/url.hpp"

#include <charconv>

namespace nsx::core::net {

std::optional<ParsedUrl> parseUrl(std::string_view url)
{
    if (url.empty()) {
        return std::nullopt;
    }

    const std::size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string_view::npos || schemeEnd == 0) {
        return std::nullopt;
    }

    ParsedUrl result;
    result.scheme = std::string(url.substr(0, schemeEnd));

    const std::string_view remaining = url.substr(schemeEnd + 3);
    if (remaining.empty()) {
        return std::nullopt;
    }

    const std::size_t pathStart = remaining.find('/');
    const std::size_t queryStart = remaining.find('?');

    std::size_t hostEnd = remaining.size();
    if (pathStart != std::string_view::npos) {
        hostEnd = pathStart;
    }
    else if (queryStart != std::string_view::npos) {
        hostEnd = queryStart;
    }

    const std::string_view hostAndPort = remaining.substr(0, hostEnd);
    if (hostAndPort.empty()) {
        return std::nullopt;
    }

    const std::size_t portPos = hostAndPort.find(':');
    if (portPos != std::string_view::npos) {
        result.host = std::string(hostAndPort.substr(0, portPos));
        const std::string_view portStr = hostAndPort.substr(portPos + 1);
        std::uint16_t parsedPort = 0;
        const auto [ptr, ec] =
            std::from_chars(portStr.data(), portStr.data() + portStr.size(), parsedPort);
        if (ec != std::errc() || ptr != portStr.data() + portStr.size()) {
            return std::nullopt;
        }
        result.port = parsedPort;
    }
    else {
        result.host = std::string(hostAndPort);
        if (result.scheme == "http") {
            result.port = 80;
        }
        else if (result.scheme == "https") {
            result.port = 443;
        }
    }

    if (pathStart != std::string_view::npos) {
        if (queryStart != std::string_view::npos && queryStart > pathStart) {
            result.path = std::string(remaining.substr(pathStart, queryStart - pathStart));
            result.query = std::string(remaining.substr(queryStart + 1));
        }
        else {
            result.path = std::string(remaining.substr(pathStart));
        }
    }
    else if (queryStart != std::string_view::npos) {
        result.path = "/";
        result.query = std::string(remaining.substr(queryStart + 1));
    }
    else {
        result.path = "/";
    }

    return result;
}

std::string buildQuery(const std::vector<std::pair<std::string, std::string>>& params)
{
    if (params.empty()) {
        return {};
    }

    std::string out = "?";
    for (std::size_t i = 0; i < params.size(); ++i) {
        if (i > 0) {
            out.push_back('&');
        }
        out.append(params[i].first);
        out.push_back('=');
        out.append(params[i].second);
    }
    return out;
}

std::string formatIfNoneMatch(std::string_view etag)
{
    if (etag.empty()) {
        return {};
    }
    if (etag.front() == '"' && etag.back() == '"') {
        return std::string(etag);
    }
    std::string out;
    out.reserve(etag.size() + 2);
    out.push_back('"');
    out.append(etag);
    out.push_back('"');
    return out;
}

}  // namespace nsx::core::net
