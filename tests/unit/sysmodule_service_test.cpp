// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/domain/sysmodule/sysmodule_service.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

#include <doctest.h>

using namespace nsx::domain;
namespace core = nsx::core;

namespace {

class FakeFileStore final : public FileStore
{
public:
    std::map<std::string, std::string> files;
    std::set<std::string> dirs{"/atmosphere", "/atmosphere/contents"};

    [[nodiscard]] bool exists(const std::string& path) const override
    {
        return files.count(path) != 0 || dirs.count(path) != 0;
    }

    [[nodiscard]] std::optional<std::string> readText(const std::string& path,
                                                      std::uint64_t maxBytes) const override
    {
        const auto it = files.find(path);
        if (it == files.end() || it->second.size() > maxBytes) {
            return std::nullopt;
        }
        return it->second;
    }

    [[nodiscard]] bool writeAtomic(const std::string& path, std::string_view data) override
    {
        files[path] = std::string(data);
        return true;
    }

    bool remove(const std::string& path) override
    {
        files.erase(path);
        return true;
    }

    [[nodiscard]] bool makeDirectories(const std::string& path) override
    {
        dirs.insert(path);
        return true;
    }

    [[nodiscard]] bool copyFile(const std::string&, const std::string&) override { return false; }

    [[nodiscard]] std::optional<core::Sha256::Digest> digestOf(const std::string&) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::uint64_t> freeSpaceBytes(const std::string&) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] bool rename(const std::string&, const std::string&) override { return false; }

    [[nodiscard]] std::vector<std::string> listFilesRecursive(const std::string&) const override
    {
        return {};
    }

    bool removeTree(const std::string&) override { return false; }

    [[nodiscard]] std::vector<std::string> listDirectories(const std::string& dir) const override
    {
        const std::string prefix = dir.empty() || dir.back() == '/' ? dir : dir + "/";
        std::vector<std::string> out;
        for (const auto& d : dirs) {
            if (d.size() > prefix.size() && d.compare(0, prefix.size(), prefix) == 0) {
                const std::string sub = d.substr(prefix.size());
                if (sub.find('/') == std::string::npos) {
                    out.push_back(sub);
                }
            }
        }
        return out;
    }
};

}  // namespace

TEST_CASE("SysmoduleService discovers sysmodules and parses metadata")
{
    FakeFileStore files;
    files.dirs.insert("/atmosphere/contents/010000000000BD00");
    files.dirs.insert("/atmosphere/contents/420000000007E51A");
    files.dirs.insert("/atmosphere/contents/0100000000009999");

    // Module 1: Has toolbox.json and boot2.flag
    files.files["/atmosphere/contents/010000000000BD00/toolbox.json"] =
        R"({"name":"Mission Control Custom","author":"ndeadly","description":"Bluetooth controllers"})";
    files.files["/atmosphere/contents/010000000000BD00/flags/boot2.flag"] = "";

    // Module 2: No toolbox.json, but is known title ID emuiibo
    files.files["/atmosphere/contents/420000000007E51A/flags/boot2.flag"] = "";

    // Module 3: Unknown title ID without toolbox.json, disabled
    // Not enabled

    SysmoduleService svc(files);
    const auto list = svc.listSysmodules();

    REQUIRE(list.size() == 3);

    // Sorted alphabetically by name:
    // "0100000000009999", "Mission Control Custom", "emuiibo"
    const auto mc = std::find_if(list.begin(), list.end(), [](const SysmoduleInfo& m) {
        return m.titleId == "010000000000BD00";
    });
    REQUIRE(mc != list.end());
    CHECK(mc->name == "Mission Control Custom");
    CHECK(mc->author == "ndeadly");
    CHECK(mc->description == "Bluetooth controllers");
    CHECK(mc->enabled == true);

    const auto emu = std::find_if(list.begin(), list.end(), [](const SysmoduleInfo& m) {
        return m.titleId == "420000000007E51A";
    });
    REQUIRE(emu != list.end());
    CHECK(emu->name == "emuiibo");
    CHECK(emu->enabled == true);

    const auto unk = std::find_if(list.begin(), list.end(), [](const SysmoduleInfo& m) {
        return m.titleId == "0100000000009999";
    });
    REQUIRE(unk != list.end());
    CHECK(unk->name == "0100000000009999");
    CHECK(unk->enabled == false);
}

TEST_CASE("SysmoduleService toggles boot2.flag enable state")
{
    FakeFileStore files;
    files.dirs.insert("/atmosphere/contents/0100000000001000");

    SysmoduleService svc(files);

    CHECK_FALSE(svc.isEnabled("0100000000001000"));

    // Enable it
    CHECK(svc.setEnabled("0100000000001000", true));
    CHECK(svc.isEnabled("0100000000001000"));
    CHECK(files.exists("/atmosphere/contents/0100000000001000/flags/boot2.flag"));

    // Disable it
    CHECK(svc.setEnabled("0100000000001000", false));
    CHECK_FALSE(svc.isEnabled("0100000000001000"));
    CHECK_FALSE(files.exists("/atmosphere/contents/0100000000001000/flags/boot2.flag"));
}
