// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/platform/system/system_info.hpp"

#include <cstdio>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace nsx::platform {

SystemVersions querySystemVersions()
{
    SystemVersions versions{"Unknown", "Not detected"};

#ifdef __SWITCH__
    // 1. Current Horizon OS Firmware version
    SetSysFirmwareVersion fw{};
    if (R_SUCCEEDED(setsysGetFirmwareVersion(&fw))) {
        fw.display_version[sizeof(fw.display_version) - 1] = '\0';
        versions.hosVersion = fw.display_version;
    }
    else {
        const u32 hos = hosversionGet();
        if (hos != 0) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%u.%u.%u", HOSVER_MAJOR(hos), HOSVER_MINOR(hos),
                          HOSVER_MICRO(hos));
            versions.hosVersion = buf;
        }
    }

    // 2. Detected Atmosphere CFW version
    if (hosversionIsAtmosphere()) {
        if (R_SUCCEEDED(splInitialize())) {
            u64 amsVer = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            if (R_SUCCEEDED(splGetConfig(static_cast<SplConfigItem>(65000), &amsVer))) {
                const auto major = static_cast<unsigned>((amsVer >> 56) & 0xFF);
                const auto minor = static_cast<unsigned>((amsVer >> 48) & 0xFF);
                const auto micro = static_cast<unsigned>((amsVer >> 40) & 0xFF);
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%u.%u.%u", major, minor, micro);
                versions.amsVersion = buf;
            }
#pragma GCC diagnostic pop
            splExit();
        }

        if (versions.amsVersion == "Not detected") {
            versions.amsVersion = "Atmosphere (Active)";
        }
    }
    else {
        // Check for presence of Atmosphere files on SD card
        std::FILE* f = std::fopen("/atmosphere/stratosphere.romfs", "rb");
        if (!f) {
            f = std::fopen("/atmosphere/package3", "rb");
        }
        if (f) {
            std::fclose(f);
            versions.amsVersion = "Atmosphere (Active)";
        }
    }
#else
    versions.hosVersion = "Host Environment";
    versions.amsVersion = "Atmosphere (Emulated)";
#endif

    return versions;
}

}  // namespace nsx::platform
