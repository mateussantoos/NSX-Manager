// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "nsx/core/result/result.hpp"

namespace nsx::platform::launch {

/// @brief Reasons a chainload or payload launch attempt failed.
enum class LaunchError
{
    FileNotFound,
    InvalidPath,
    ServiceFailed,
    ConfigFailed
};

/// @brief Information about an available .bin payload found on the SD card.
struct PayloadEntry
{
    std::string path;
    std::string filename;
};

/// @brief Target chainloader and RCM payload launcher.
class PayloadLauncher
{
public:
    /// @brief Discover available .bin payloads across common payload directories.
    /// @param searchDirs List of directories to search, defaults to standard Switch payload
    /// locations.
    /// @return List of discovered payload entries.
    [[nodiscard]] static std::vector<PayloadEntry> listAvailablePayloads(
        const std::vector<std::string>& searchDirs = {"sdmc:/bootloader/payloads",
                                                      "sdmc:/switch/nsx-manager/payloads"});

    /// @brief Transition execution to a target NRO via envSetNextLoad.
    /// @param nroPath Target application or forwarder NRO path.
    /// @param argv Command-line arguments.
    /// @return Result with success or LaunchError.
    static core::Result<std::monostate, LaunchError> chainloadNro(std::string_view nroPath,
                                                                  std::string_view argv = "");

    /// @brief Reboot to a target RCM payload .bin (e.g. Hekate/Lockpick).
    /// @param payloadPath Path to payload binary.
    /// @return Result with success or LaunchError.
    static core::Result<std::monostate, LaunchError> rebootToPayload(std::string_view payloadPath);
};

}  // namespace nsx::platform::launch
