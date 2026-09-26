// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>
#include <string_view>

namespace nsx::core::paths {

/// @brief Normalize a filesystem path (replaces backslashes, removes redundant slashes and ./).
/// @param path Raw path.
/// @return Normalized path string.
[[nodiscard]] std::string normalize(std::string_view path);

/// @brief Check if a path ends with a specific extension (case-insensitive).
/// @param path Path to check.
/// @param extension Extension including or omitting leading dot (e.g. ".nro" or "nro").
/// @return True if path ends with extension.
[[nodiscard]] bool hasExtension(std::string_view path, std::string_view extension) noexcept;

/// @brief Safely join two path segments.
/// @param base Directory or base path.
/// @param relative Relative subpath.
/// @return Joined path.
[[nodiscard]] std::string join(std::string_view base, std::string_view relative);

}  // namespace nsx::core::paths
