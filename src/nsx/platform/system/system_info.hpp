// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>

namespace nsx::platform {

/// @brief Detected system versions for Horizon OS and Atmosphere CFW.
struct SystemVersions
{
    std::string hosVersion;  ///< Current Horizon OS firmware version string (e.g. "18.1.0").
    std::string amsVersion;  ///< Detected Atmosphere version (e.g. "1.7.1" or "Active").
};

/// @brief Query the console system for Horizon OS and Atmosphere versions.
[[nodiscard]] SystemVersions querySystemVersions();

}  // namespace nsx::platform
