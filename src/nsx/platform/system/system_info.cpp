// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/platform/system/system_info.hpp"

#include <cstdio>

#ifdef __SWITCH__
#include <switch.h>
#include <unistd.h>

extern "C" {
int fileno(FILE* stream);
int ftruncate(int fd, off_t length);
}
#endif

namespace nsx::platform {

SystemInfo querySystemInfo()
{
    SystemInfo info;
    info.model = "Nintendo Switch";
    info.socStepping = "T210 (Erista)";
    info.hosVersion = "Unknown";
    info.amsVersion = "Not detected";
    info.nandType = "SysNAND";
    info.fsType = "FAT32";
    info.isExFAT = false;

#ifdef __SWITCH__
    // 1. Detect Switch Model
    SetSysProductModel model = SetSysProductModel_Invalid;
    if (R_SUCCEEDED(setsysGetProductModel(&model))) {
        switch (model) {
            case SetSysProductModel_Aula:
                info.model = "Switch OLED";
                info.socStepping = "T210B01 (Mariko)";
                break;
            case SetSysProductModel_Hoag:
                info.model = "Switch Lite";
                info.socStepping = "T210B01 (Mariko)";
                break;
            case SetSysProductModel_Iowa:
                info.model = "Switch V2";
                info.socStepping = "T210B01 (Mariko)";
                break;
            case SetSysProductModel_Nx:
                info.model = "Switch V1";
                info.socStepping = "T210 (Erista)";
                break;
            case SetSysProductModel_Copper:
            case SetSysProductModel_Calcio:
                info.model = "Switch DevKit";
                info.socStepping = "T210 (Dev)";
                break;
            default:
                info.model = "Nintendo Switch";
                info.socStepping = "T210";
                break;
        }
    }

    // 2. Horizon OS Firmware version
    SetSysFirmwareVersion fw{};
    if (R_SUCCEEDED(setsysGetFirmwareVersion(&fw))) {
        fw.display_version[sizeof(fw.display_version) - 1] = '\0';
        info.hosVersion = fw.display_version;
    }
    else {
        const u32 hos = hosversionGet();
        if (hos != 0) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%u.%u.%u", HOSVER_MAJOR(hos), HOSVER_MINOR(hos),
                          HOSVER_MICRO(hos));
            info.hosVersion = buf;
        }
    }

    // 3. Atmosphere CFW version & NAND detection
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
                info.amsVersion = buf;
            }

            u64 emummc = 0;
            if (R_SUCCEEDED(splGetConfig(static_cast<SplConfigItem>(65007), &emummc))) {
                if (emummc == 1) {
                    info.nandType = "EmuNAND (Partition)";
                }
                else if (emummc == 2) {
                    info.nandType = "EmuNAND (File)";
                }
                else if (emummc > 0) {
                    info.nandType = "EmuNAND";
                }
                else {
                    info.nandType = "SysNAND";
                }
            }
#pragma GCC diagnostic pop
            splExit();
        }

        if (info.amsVersion == "Not detected") {
            info.amsVersion = "Atmosphere (Active)";
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
            info.amsVersion = "Atmosphere (Active)";
        }
    }

    // 4. SD Card Filesystem Detection (FAT32 vs exFAT)
    const char* testPath = "/.__fstest_nsx.tmp";
    std::FILE* fp = std::fopen(testPath, "wb");
    if (fp) {
        int fd = fileno(fp);
        int res = ftruncate(fd, 0x100000001ULL);
        std::fclose(fp);
        std::remove(testPath);
        if (res == 0) {
            info.isExFAT = true;
            info.fsType = "exFAT";
        }
        else {
            info.isExFAT = false;
            info.fsType = "FAT32";
        }
    }
#else
    info.model = "Switch OLED (Emulated)";
    info.hosVersion = "Emulated";
    info.amsVersion = "Atmosphere (Emulated)";
    info.nandType = "EmuNAND (Partition)";
    info.fsType = "FAT32";
    info.isExFAT = false;
#endif

    return info;
}

}  // namespace nsx::platform
