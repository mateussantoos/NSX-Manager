// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/domain/maintenance/cleanup_service.hpp"

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
    std::set<std::string> dirs{"/", "/config", "/config/nsx-manager", "/config/nsx-manager/cache",
                               "/config/nsx-manager/staging"};

    [[nodiscard]] bool exists(const std::string& path) const override
    {
        return files.count(path) != 0;
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

    [[nodiscard]] bool copyFile(const std::string& from, const std::string& to) override
    {
        const auto it = files.find(from);
        if (it == files.end()) {
            return false;
        }
        return writeAtomic(to, it->second);
    }

    [[nodiscard]] std::optional<core::Sha256::Digest> digestOf(
        const std::string& path) const override
    {
        const auto it = files.find(path);
        if (it == files.end()) {
            return std::nullopt;
        }
        return core::Sha256::of(it->second);
    }

    [[nodiscard]] std::optional<std::uint64_t> freeSpaceBytes(const std::string&) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] bool rename(const std::string& from, const std::string& to) override
    {
        const auto it = files.find(from);
        if (it == files.end()) {
            return false;
        }
        const std::string data = it->second;
        files.erase(it);
        files[to] = data;
        return true;
    }

    [[nodiscard]] std::vector<std::string> listFilesRecursive(const std::string& dir) const override
    {
        const std::string prefix = dir.empty() || dir.back() == '/' ? dir : dir + "/";
        std::vector<std::string> out;
        for (const auto& [path, unused] : files) {
            if (path.size() > prefix.size() && path.compare(0, prefix.size(), prefix) == 0) {
                out.push_back(path.substr(prefix.size()));
            }
        }
        return out;
    }

    bool removeTree(const std::string& dir) override
    {
        const std::string prefix = dir.empty() || dir.back() == '/' ? dir : dir + "/";
        for (auto it = files.begin(); it != files.end();) {
            const bool inside = it->first.size() > prefix.size() &&
                                it->first.compare(0, prefix.size(), prefix) == 0;
            it = (inside || it->first == dir) ? files.erase(it) : std::next(it);
        }
        return true;
    }
};

}  // namespace

TEST_CASE("cleanStaging removes all staged downloads and partials")
{
    FakeFileStore files;
    CleanupConfig cfg;
    cfg.stagingDir = "/config/nsx-manager/staging";
    cfg.cacheDir = "/config/nsx-manager/cache";

    files.files["/config/nsx-manager/staging/firmware.zip"] = "firmware data";
    files.files["/config/nsx-manager/staging/cfw/pack.zip"] = "cfw pack";
    files.files["/config/nsx-manager/staging/extracted/atmosphere/bin"] = "binary";

    // Cache files that must NOT be removed
    files.files["/config/nsx-manager/cache/catalog.json"] = "catalog json";
    files.files["/config/nsx-manager/cache/catalog.meta.json"] = "meta";
    files.files["/config/nsx-manager/cache/backoff.json"] = "backoff";

    CleanupService svc(files, cfg);
    CHECK(svc.countStagedFiles() == 3);

    const CleanupReport report = svc.cleanStaging();
    CHECK(report.success);
    CHECK(report.filesRemoved == 3);
    CHECK(svc.countStagedFiles() == 0);

    // Staging is empty
    CHECK_FALSE(files.exists("/config/nsx-manager/staging/firmware.zip"));
    CHECK_FALSE(files.exists("/config/nsx-manager/staging/cfw/pack.zip"));
    CHECK_FALSE(files.exists("/config/nsx-manager/staging/extracted/atmosphere/bin"));

    // Cache files are preserved
    CHECK(files.exists("/config/nsx-manager/cache/catalog.json"));
    CHECK(files.exists("/config/nsx-manager/cache/catalog.meta.json"));
    CHECK(files.exists("/config/nsx-manager/cache/backoff.json"));
}

TEST_CASE("cleanStaging refuses unsafe or conflicting paths")
{
    FakeFileStore files;

    SUBCASE("root is refused")
    {
        CleanupConfig cfg;
        cfg.stagingDir = "/";
        CleanupService svc(files, cfg);
        const CleanupReport rep = svc.cleanStaging();
        CHECK_FALSE(rep.success);
    }

    SUBCASE("config root is refused")
    {
        CleanupConfig cfg;
        cfg.stagingDir = "/config";
        CleanupService svc(files, cfg);
        const CleanupReport rep = svc.cleanStaging();
        CHECK_FALSE(rep.success);
    }

    SUBCASE("conflicting cache path is refused")
    {
        CleanupConfig cfg;
        cfg.stagingDir = "/config/nsx-manager/cache";
        cfg.cacheDir = "/config/nsx-manager/cache";
        CleanupService svc(files, cfg);
        const CleanupReport rep = svc.cleanStaging();
        CHECK_FALSE(rep.success);
    }
}
