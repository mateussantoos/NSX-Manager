// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nsx::core::net {

/// @brief Parsed URL components.
struct ParsedUrl
{
    std::string scheme;
    std::string host;
    std::uint16_t port{0};
    std::string path;
    std::string query;
};

/// @brief Parse a URL into its constituent components.
/// @param url Raw URL string.
/// @return ParsedUrl if scheme and host are present and valid, nullopt otherwise.
[[nodiscard]] std::optional<ParsedUrl> parseUrl(std::string_view url);

/// @brief Build a URL query string from key-value parameter pairs.
/// @param params List of key-value pairs.
/// @return Query string (starting with '?' if params not empty).
[[nodiscard]] std::string buildQuery(
    const std::vector<std::pair<std::string, std::string>>& params);

/// @brief Format an ETag value for If-None-Match header.
/// @param etag Raw or quoted ETag string.
/// @return Quoted ETag.
[[nodiscard]] std::string formatIfNoneMatch(std::string_view etag);

}  // namespace nsx::core::net
