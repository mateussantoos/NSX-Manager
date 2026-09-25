// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/domain/sysmodule/sysmodule_service.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace nsx::domain {

namespace {

using Json = nlohmann::json;

bool isHex16(std::string_view s)
{
    if (s.size() != 16) {
        return false;
    }
    return std::all_of(s.begin(), s.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    });
}

std::string toLower(std::string_view s)
{
    std::string res;
    res.reserve(s.size());
    for (char c : s) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

std::string knownSysmoduleName(std::string_view titleId)
{
    const std::string id = toLower(titleId);
    static const std::unordered_map<std::string, std::string> kKnown = {
        {"010000000000bd00", "MissionControl"}, {"420000000007e51a", "emuiibo"},
        {"010000000007e51a", "emuiibo"},        {"0100000000001000", "sys-clk"},
        {"4200000000000010", "ldn_mitm"},       {"0100000000000352", "SaltyNX"},
        {"420000000000000b", "sys-ftpd-ovl"},   {"420000000000000e", "sys-con"},
        {"0100000000000d15", "sys-patch"},      {"00ff0000a2000d66", "ovlloader (Tesla)"},
    };

    const auto it = kKnown.find(id);
    if (it != kKnown.end()) {
        return it->second;
    }
    return std::string(titleId);
}

}  // namespace

SysmoduleService::SysmoduleService(FileStore& files, std::string contentsRoot)
    : m_files(files), m_contentsRoot(std::move(contentsRoot))
{
}

bool SysmoduleService::isEnabled(std::string_view titleId) const
{
    const std::string base = m_contentsRoot + "/" + std::string(titleId);
    return m_files.exists(base + "/flags/boot2.flag") || m_files.exists(base + "/boot2.flag");
}

bool SysmoduleService::setEnabled(std::string_view titleId, bool enable)
{
    const std::string base = m_contentsRoot + "/" + std::string(titleId);

    if (enable) {
        if (!m_files.makeDirectories(base + "/flags")) {
            return false;
        }
        return m_files.writeAtomic(base + "/flags/boot2.flag", "");
    }

    const bool r1 = m_files.remove(base + "/flags/boot2.flag");
    const bool r2 = m_files.remove(base + "/boot2.flag");
    return r1 || r2 || !isEnabled(titleId);
}

std::vector<SysmoduleInfo> SysmoduleService::listSysmodules() const
{
    std::vector<SysmoduleInfo> modules;
    const std::vector<std::string> dirs = m_files.listDirectories(m_contentsRoot);

    for (const std::string& dirName : dirs) {
        const std::string base = m_contentsRoot + "/" + dirName;
        const std::string toolboxPath = base + "/toolbox.json";

        const bool hasToolbox = m_files.exists(toolboxPath);
        const bool hasBoot2 = isEnabled(dirName);
        const bool isHex = isHex16(dirName);

        if (!hasToolbox && !hasBoot2 && !isHex) {
            continue;
        }

        SysmoduleInfo info;
        info.titleId = dirName;
        info.enabled = hasBoot2;

        if (hasToolbox) {
            constexpr std::uint64_t kMaxJsonBytes = 32 * 1024;
            const auto text = m_files.readText(toolboxPath, kMaxJsonBytes);
            if (text.has_value()) {
                const Json doc = Json::parse(*text, nullptr, false);
                if (doc.is_object()) {
                    if (doc.contains("name") && doc["name"].is_string()) {
                        info.name = doc["name"].get<std::string>();
                    }
                    if (doc.contains("author") && doc["author"].is_string()) {
                        info.author = doc["author"].get<std::string>();
                    }
                    if (doc.contains("description") && doc["description"].is_string()) {
                        info.description = doc["description"].get<std::string>();
                    }
                }
            }
        }

        if (info.name.empty()) {
            info.name = knownSysmoduleName(dirName);
        }

        modules.push_back(std::move(info));
    }

    std::sort(modules.begin(), modules.end(),
              [](const SysmoduleInfo& a, const SysmoduleInfo& b) { return a.name < b.name; });

    return modules;
}

}  // namespace nsx::domain
