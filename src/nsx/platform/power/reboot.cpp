// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/platform/power/reboot.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace nsx::platform {

std::string_view describe(RebootError error)
{
    switch (error) {
        case RebootError::PayloadNotFound:
            return "the payload binary was not found in romfs";
        case RebootError::PayloadWriteFailed:
            return "could not write the payload to the SD card";
        case RebootError::ConfigFailed:
            return "could not configure Atmosphere reboot-to-payload";
        case RebootError::ServiceFailed:
            return "the power service refused the command";
    }
    return "unknown reboot error";
}

core::Result<std::monostate, RebootError> rebootToPayload(std::string_view sourcePayloadPath)
{
#ifdef __SWITCH__
    // 1. Read source payload from romfs.
    const std::string sourceStr(sourcePayloadPath);
    std::FILE* in = std::fopen(sourceStr.c_str(), "rb");
    if (!in) {
        return core::Result<std::monostate, RebootError>::err(RebootError::PayloadNotFound);
    }

    std::fseek(in, 0, SEEK_END);
    const long size = std::ftell(in);
    std::fseek(in, 0, SEEK_SET);

    if (size <= 0) {
        std::fclose(in);
        return core::Result<std::monostate, RebootError>::err(RebootError::PayloadNotFound);
    }

    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(size));
    const std::size_t readBytes = std::fread(buffer.data(), 1, buffer.size(), in);
    std::fclose(in);

    if (readBytes != buffer.size()) {
        return core::Result<std::monostate, RebootError>::err(RebootError::PayloadNotFound);
    }

    // 2. Write to both /payload.bin and /atmosphere/reboot_payload.bin.
    const char* targets[] = {"/payload.bin", "/atmosphere/reboot_payload.bin"};
    bool wroteAny = false;
    for (const char* target : targets) {
        std::FILE* out = std::fopen(target, "wb");
        if (out) {
            const std::size_t written = std::fwrite(buffer.data(), 1, buffer.size(), out);
            std::fclose(out);
            if (written == buffer.size()) {
                wroteAny = true;
            }
        }
    }

    if (!wroteAny) {
        return core::Result<std::monostate, RebootError>::err(RebootError::PayloadWriteFailed);
    }

    // 3. Configure Atmosphere reboot-to-payload via SPL extension (item 65001).
    if (R_SUCCEEDED(splInitialize())) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        (void)splSetConfig(static_cast<SplConfigItem>(65001), 1);
#pragma GCC diagnostic pop
        splExit();
    }

    // 4. Trigger reboot via SPSM or BPC.
    if (R_SUCCEEDED(spsmInitialize())) {
        (void)spsmShutdown(true);
        spsmExit();
        return core::Result<std::monostate, RebootError>::ok({});
    }

    if (R_SUCCEEDED(bpcInitialize())) {
        (void)bpcRebootSystem();
        bpcExit();
        return core::Result<std::monostate, RebootError>::ok({});
    }

    return core::Result<std::monostate, RebootError>::err(RebootError::ServiceFailed);
#else
    (void)sourcePayloadPath;
    return core::Result<std::monostate, RebootError>::err(RebootError::ServiceFailed);
#endif
}

core::Result<std::monostate, RebootError> reboot()
{
#ifdef __SWITCH__
    if (R_SUCCEEDED(spsmInitialize())) {
        (void)spsmShutdown(true);
        spsmExit();
        return core::Result<std::monostate, RebootError>::ok({});
    }
    if (R_SUCCEEDED(bpcInitialize())) {
        (void)bpcRebootSystem();
        bpcExit();
        return core::Result<std::monostate, RebootError>::ok({});
    }
    return core::Result<std::monostate, RebootError>::err(RebootError::ServiceFailed);
#else
    return core::Result<std::monostate, RebootError>::err(RebootError::ServiceFailed);
#endif
}

core::Result<std::monostate, RebootError> shutdown()
{
#ifdef __SWITCH__
    if (R_SUCCEEDED(spsmInitialize())) {
        (void)spsmShutdown(false);
        spsmExit();
        return core::Result<std::monostate, RebootError>::ok({});
    }
    if (R_SUCCEEDED(bpcInitialize())) {
        (void)bpcShutdownSystem();
        bpcExit();
        return core::Result<std::monostate, RebootError>::ok({});
    }
    return core::Result<std::monostate, RebootError>::err(RebootError::ServiceFailed);
#else
    return core::Result<std::monostate, RebootError>::err(RebootError::ServiceFailed);
#endif
}

}  // namespace nsx::platform
