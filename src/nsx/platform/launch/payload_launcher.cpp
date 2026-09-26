// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/platform/launch/payload_launcher.hpp"

#include <filesystem>
#include <string>

#include "nsx/platform/power/reboot.hpp"

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace nsx::platform::launch {

std::vector<PayloadEntry> PayloadLauncher::listAvailablePayloads(
    const std::vector<std::string>& searchDirs)
{
    namespace fs = std::filesystem;
    std::vector<PayloadEntry> entries;
    std::error_code ec;

    for (const auto& dir : searchDirs) {
        if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
            continue;
        }

        for (const auto& it : fs::directory_iterator(dir, ec)) {
            if (it.is_regular_file(ec) && it.path().extension() == ".bin") {
                entries.push_back(PayloadEntry{.path = it.path().string(),
                                               .filename = it.path().filename().string()});
            }
        }
    }

    return entries;
}

using ResultType = core::Result<std::monostate, LaunchError>;

core::Result<std::monostate, LaunchError> PayloadLauncher::chainloadNro(std::string_view nroPath,
                                                                        std::string_view argv)
{
    if (nroPath.empty()) {
        return ResultType::err(LaunchError::InvalidPath);
    }

#ifdef __SWITCH__
    const std::string pathStr(nroPath);
    const std::string argsStr(argv);
    if (R_FAILED(envSetNextLoad(pathStr.c_str(), argsStr.c_str()))) {
        return ResultType::err(LaunchError::ServiceFailed);
    }
#endif

    return ResultType::ok(std::monostate{});
}

core::Result<std::monostate, LaunchError> PayloadLauncher::rebootToPayload(
    std::string_view payloadPath)
{
    if (payloadPath.empty()) {
        return ResultType::err(LaunchError::InvalidPath);
    }

    const auto res = nsx::platform::rebootToPayload(payloadPath);
    if (!res.hasValue()) {
        return ResultType::err(LaunchError::ConfigFailed);
    }

    return ResultType::ok(std::monostate{});
}

}  // namespace nsx::platform::launch
