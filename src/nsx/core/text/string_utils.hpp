// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nsx::core::text {

/// @brief Format a raw byte count into human-readable string (B, KB, MB, GB).
/// @param bytes Byte count.
/// @return Formatted string, e.g. "1.23 MB" or "512 B".
[[nodiscard]] std::string formatBytes(std::uint64_t bytes);

/// @brief Trim whitespace from the left side of a string view.
[[nodiscard]] std::string_view trimLeft(std::string_view str) noexcept;

/// @brief Trim whitespace from the right side of a string view.
[[nodiscard]] std::string_view trimRight(std::string_view str) noexcept;

/// @brief Trim whitespace from both ends of a string view.
[[nodiscard]] std::string_view trim(std::string_view str) noexcept;

/// @brief Sanitize a string by stripping non-printable ASCII characters.
[[nodiscard]] std::string sanitize(std::string_view str);

/// @brief Split a string view by a delimiter character.
[[nodiscard]] std::vector<std::string> split(std::string_view str, char delimiter);

/// @brief Join a list of strings with a delimiter.
[[nodiscard]] std::string join(const std::vector<std::string>& elements,
                               std::string_view delimiter);

}  // namespace nsx::core::text
