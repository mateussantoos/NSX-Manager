// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>

namespace nsx::platform {

/// @brief Detected system information for the hardware dashboard.
struct SystemInfo
{
    std::string model;  ///< Model name ("Switch OLED", "Switch Lite", "Switch V1", "Switch V2").
    std::string hosVersion;  ///< Current Horizon OS firmware version string (e.g. "18.1.0").
    std::string amsVersion;  ///< Detected Atmosphere version (e.g. "1.7.1" or "Active").
    std::string nandType;    ///< NAND status ("SysNAND", "EmuNAND (Partition)", "EmuNAND (File)").
    std::string fsType;      ///< SD card filesystem type ("FAT32" or "exFAT").
    bool isExFAT{false};     ///< True if SD card is formatted with exFAT.
};

/// @brief Query the console system for complete system info.
[[nodiscard]] SystemInfo querySystemInfo();

}  // namespace nsx::platform
