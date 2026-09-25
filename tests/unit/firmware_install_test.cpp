// SPDX-License-Identifier: GPL-3.0-only
//
// Staging official firmware for Daybreak.
//
// The dangerous step here is not the download, it is clearing /firmware/. That
// directory is a well-known path this application does not own, a user may have
// put things in it, and there is no backup - a firmware set is replaced whole
// because NCAs from two versions is how Daybreak installs a system that will not
// boot. So most of what follows is about what happens BEFORE anything is
// deleted.

#include <iterator>
#include <map>
#include <set>
#include <string>

#include <doctest.h>

#include "nsx/domain/firmware/firmware_install_service.hpp"

using namespace nsx::domain;
namespace core = nsx::core;
namespace infra = nsx::infra;

namespace {

class FakeFileStore final : public FileStore
{
public:
    std::map<std::string, std::string> files;
    std::optional<std::uint64_t> freeSpace{64ULL * 1024 * 1024 * 1024};
    std::set<std::string> removeFailures;

    [[nodiscard]] bool exists(const std::string& path) const override
    {
        return files.count(path) != 0;
    }

    [[nodiscard]] std::optional<std::string> readText(const std::string& path,
                                                      std::uint64_t) const override
    {
        const auto it = files.find(path);
        return it == files.end() ? std::nullopt : std::optional<std::string>(it->second);
    }

    [[nodiscard]] bool writeAtomic(const std::string& path, std::string_view data) override
    {
        files[path] = std::string(data);
        return true;
    }

    bool remove(const std::string& path) override
    {
        if (removeFailures.count(path) != 0) {
            return false;
        }
        files.erase(path);
        return true;
    }

    [[nodiscard]] bool makeDirectories(const std::string&) override { return true; }

    [[nodiscard]] bool copyFile(const std::string& from, const std::string& to) override
    {
        const auto it = files.find(from);
        return it != files.end() && writeAtomic(to, it->second);
    }

    [[nodiscard]] std::optional<core::Sha256::Digest> digestOf(
        const std::string& path) const override
    {
        const auto it = files.find(path);
        return it == files.end()
                   ? std::nullopt
                   : std::optional<core::Sha256::Digest>(core::Sha256::of(it->second));
    }

    [[nodiscard]] std::optional<std::uint64_t> freeSpaceBytes(const std::string&) const override
    {
        return freeSpace;
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
            it = inside ? files.erase(it) : std::next(it);
        }
        return true;
    }
};

class FakeHttp final : public HttpGateway
{
public:
    explicit FakeHttp(FakeFileStore& files) : m_files(files) {}

    std::string body{"firmware-archive"};
    std::optional<infra::HttpError> error;
    int downloads{};

    [[nodiscard]] core::Result<infra::Response, infra::HttpError> fetch(const std::string&,
                                                                        std::string_view) override
    {
        return core::Result<infra::Response, infra::HttpError>::err(infra::HttpError::Internal);
    }

    [[nodiscard]] core::Result<std::monostate, infra::HttpError> download(
        const std::string&, const std::string& destPath, const infra::ExpectedArtifact& expected,
        const infra::ProgressCallback&) override
    {
        using R = core::Result<std::monostate, infra::HttpError>;
        ++downloads;
        if (error.has_value()) {
            return R::err(*error);
        }
        if (body.size() != expected.size) {
            return R::err(infra::HttpError::SizeMismatch);
        }
        if (!core::digestsEqual(core::Sha256::of(body), expected.sha256)) {
            return R::err(infra::HttpError::DigestMismatch);
        }
        (void)m_files.writeAtomic(destPath, body);
        return R::ok(std::monostate{});
    }

private:
    FakeFileStore& m_files;
};

class FakeArchives final : public ArchiveGateway
{
public:
    explicit FakeArchives(FakeFileStore& files) : m_files(files) {}

    std::map<std::string, std::string> contents{
        {"0100000000000809.nca", "nca-a"},
        {"0100000000000816.nca", "nca-b"},
        {"0100000000000819.cnmt.nca", "meta"},
    };
    std::optional<infra::ArchiveError> error;
    int extractions{};

    [[nodiscard]] core::Result<infra::ExtractionReport, infra::ArchiveError> extract(
        const std::string&, const core::ExtractionPolicy& policy, std::uint64_t,
        const infra::ExtractionCallback&) override
    {
        using R = core::Result<infra::ExtractionReport, infra::ArchiveError>;
        ++extractions;
        if (error.has_value()) {
            return R::err(*error);
        }
        infra::ExtractionReport report;
        for (const auto& [name, data] : contents) {
            const core::EntryDecision d = policy.decide(name);
            if (d.action == core::EntryAction::Reject) {
                return R::err(infra::ArchiveError::UnsafeEntry);
            }
            if (d.action == core::EntryAction::Write) {
                (void)m_files.writeAtomic(d.destination, data);
                ++report.filesWritten;
            }
        }
        return R::ok(std::move(report));
    }

private:
    FakeFileStore& m_files;
};

struct Rig
{
    FakeFileStore files;
    FakeHttp http{files};
    FakeArchives archives{files};
    FirmwareConfig config;
    core::CatalogItem item;

    Rig()
    {
        item.id = "firmware-19-0-1";
        item.name = "Firmware 19.0.1";
        item.versionText = "19.0.1";
        item.kind = core::ContentKind::Firmware;
        item.target = core::InstallTarget::FirmwareStaging;
        item.url = "https://example.invalid/fw.zip";
        item.size = http.body.size();
        item.sha256 = core::Sha256::of(http.body);

        files.files[config.daybreakNro] = "daybreak";
    }

    FirmwareInstallService service()
    {
        return FirmwareInstallService(http, archives, files, config);
    }

    void withPreviousFirmware()
    {
        files.files["/firmware/0100000000000800.nca"] = "old-a";
        files.files["/firmware/0100000000000801.nca"] = "old-b";
    }

    [[nodiscard]] std::string at(const std::string& path) const
    {
        const auto it = files.files.find(path);
        return it == files.files.end() ? std::string("<absent>") : it->second;
    }
};

bool isResult(FirmwareResult a, FirmwareResult b)
{
    return a == b;
}

}  // namespace

// ---------------------------------------------------------------------------
// Classifying what is in the directory
// ---------------------------------------------------------------------------

TEST_CASE("firmware content is recognised by extension, case-insensitively")
{
    CHECK(core::isFirmwareContent("0100000000000809.nca"));
    CHECK(core::isFirmwareContent("0100000000000819.cnmt.nca"));
    CHECK(core::isFirmwareContent("SOMETHING.NCA"));  // unpacked on a PC
    CHECK_FALSE(core::isFirmwareContent("readme.txt"));
    CHECK_FALSE(core::isFirmwareContent("notes.nca.bak"));
    CHECK_FALSE(core::isFirmwareContent(""));
    CHECK_FALSE(core::isFirmwareContent(".nca"));
}

TEST_CASE("a scan separates a previous set from anything else")
{
    const core::FirmwareDirectoryScan scan =
        core::scanFirmwareDirectory({"a.nca", "b.nca", "my-notes.txt", "backup/c.nca"});

    CHECK(scan.firmwareFiles == 3);  // backup/c.nca is still an nca
    REQUIRE(scan.unexpected.size() == 1);
    CHECK(scan.unexpected[0] == "my-notes.txt");
    CHECK_FALSE(scan.onlyFirmware());
    CHECK_FALSE(scan.empty());
}

TEST_CASE("an empty directory is clearable and reports itself as empty")
{
    const core::FirmwareDirectoryScan scan = core::scanFirmwareDirectory({});
    CHECK(scan.empty());
    CHECK(scan.onlyFirmware());
}

TEST_CASE("an archive with no nca is not a firmware set")
{
    CHECK(core::looksLikeFirmware({"a.nca"}));
    CHECK_FALSE(core::looksLikeFirmware({}));
    CHECK_FALSE(core::looksLikeFirmware({"readme.txt"}));
    CHECK_FALSE(core::looksLikeFirmware({"a.nca", "readme.txt"}));
}

// ---------------------------------------------------------------------------
// The happy path
// ---------------------------------------------------------------------------

TEST_CASE("firmware downloads, replaces the previous set and hands off to Daybreak")
{
    Rig rig;
    rig.withPreviousFirmware();

    FirmwareInstallService service = rig.service();
    const FirmwareOutcome out = service.stage(rig.item);

    CHECK(isResult(out.result, FirmwareResult::Ready));
    CHECK(out.readyForDaybreak());
    CHECK(out.filesStaged == 3);
    CHECK(out.previousFilesRemoved == 2);

    CHECK(rig.at("/firmware/0100000000000809.nca") == "nca-a");
    CHECK(rig.at("/firmware/0100000000000800.nca") == "<absent>");  // the old set is gone

    CHECK(out.firmwareDir == "/firmware");
    CHECK(out.daybreakArgs == "\"/switch/daybreak.nro\" \"/firmware\"");
}

TEST_CASE("the download is cleaned up once the set is staged")
{
    Rig rig;
    FirmwareInstallService service = rig.service();
    REQUIRE(service.stage(rig.item).readyForDaybreak());
    CHECK_FALSE(rig.files.exists(rig.config.archiveFor(rig.item.id)));
}

// ---------------------------------------------------------------------------
// Refusals that happen BEFORE anything is deleted
// ---------------------------------------------------------------------------

TEST_CASE("an unexpected file stops the install and is named")
{
    // /firmware/ is a well-known path we do not own. A recursive delete to save
    // the user one manual step is not a trade worth making.
    Rig rig;
    rig.withPreviousFirmware();
    rig.files.files["/firmware/my-important-notes.txt"] = "do not delete";

    FirmwareInstallService service = rig.service();
    const FirmwareOutcome out = service.stage(rig.item);

    CHECK(isResult(out.result, FirmwareResult::DirectoryNotOurs));
    REQUIRE(out.unexpectedFiles.size() == 1);
    CHECK(out.unexpectedFiles[0] == "my-important-notes.txt");
    CHECK(out.detail.find("my-important-notes.txt") != std::string::npos);

    // Nothing downloaded, nothing deleted.
    CHECK(rig.http.downloads == 0);
    CHECK(rig.at("/firmware/my-important-notes.txt") == "do not delete");
    CHECK(rig.at("/firmware/0100000000000800.nca") == "old-a");
}

TEST_CASE("a missing Daybreak stops the install before the download")
{
    // Staging a set with nothing able to install it would waste the download
    // and leave the user with no firmware where they had one.
    Rig rig;
    rig.withPreviousFirmware();
    rig.files.files.erase(rig.config.daybreakNro);

    FirmwareInstallService service = rig.service();
    const FirmwareOutcome out = service.stage(rig.item);

    CHECK(isResult(out.result, FirmwareResult::DaybreakMissing));
    CHECK(rig.http.downloads == 0);
    CHECK(rig.at("/firmware/0100000000000800.nca") == "old-a");
}

TEST_CASE("a catalogue item of the wrong kind is refused")
{
    Rig rig;
    rig.item.kind = core::ContentKind::CfwPack;

    FirmwareInstallService service = rig.service();
    CHECK(isResult(service.stage(rig.item).result, FirmwareResult::NotFirmwareItem));
    CHECK(rig.http.downloads == 0);
}

TEST_CASE("insufficient space is refused before the first byte")
{
    Rig rig;
    rig.withPreviousFirmware();
    rig.files.freeSpace = 1;

    FirmwareInstallService service = rig.service();
    CHECK(isResult(service.stage(rig.item).result, FirmwareResult::InsufficientSpace));
    CHECK(rig.http.downloads == 0);
    CHECK(rig.at("/firmware/0100000000000800.nca") == "old-a");
}

// ---------------------------------------------------------------------------
// THE property that matters: a failure must not cost the user their firmware
// ---------------------------------------------------------------------------

TEST_CASE("a failed download leaves the previous firmware in place")
{
    // There is no backup here - the set is replaced whole. So clearing must
    // happen after the replacement is downloaded AND verified, never before.
    Rig rig;
    rig.withPreviousFirmware();
    rig.http.error = infra::HttpError::ConnectFailed;

    FirmwareInstallService service = rig.service();
    const FirmwareOutcome out = service.stage(rig.item);

    CHECK(isResult(out.result, FirmwareResult::DownloadFailed));
    CHECK(rig.at("/firmware/0100000000000800.nca") == "old-a");
    CHECK(rig.at("/firmware/0100000000000801.nca") == "old-b");
    CHECK(out.previousFilesRemoved == 0);
    CHECK(rig.archives.extractions == 0);
}

TEST_CASE("a download that does not verify leaves the previous firmware in place")
{
    Rig rig;
    rig.withPreviousFirmware();
    rig.http.error = infra::HttpError::DigestMismatch;

    FirmwareInstallService service = rig.service();
    const FirmwareOutcome out = service.stage(rig.item);

    CHECK(isResult(out.result, FirmwareResult::VerifyFailed));
    CHECK(rig.at("/firmware/0100000000000800.nca") == "old-a");
    CHECK(rig.archives.extractions == 0);
}

TEST_CASE("cancelling before the clear leaves the previous firmware in place")
{
    Rig rig;
    rig.withPreviousFirmware();

    FirmwareInstallService service = rig.service();
    const FirmwareOutcome out = service.stage(
        rig.item, [](const FirmwareProgress& p) { return p.stage != FirmwareStage::Clearing; });

    CHECK(isResult(out.result, FirmwareResult::Cancelled));
    CHECK(rig.at("/firmware/0100000000000800.nca") == "old-a");
}

// ---------------------------------------------------------------------------
// Failures after the clear - honest about the state they leave
// ---------------------------------------------------------------------------

TEST_CASE("a hostile archive is refused, and the policy is what refuses it")
{
    Rig rig;
    rig.archives.contents["../../atmosphere/contents/evil.nsp"] = "PWNED";

    FirmwareInstallService service = rig.service();
    const FirmwareOutcome out = service.stage(rig.item);

    CHECK(isResult(out.result, FirmwareResult::UnsafeArchive));
    CHECK(rig.at("/atmosphere/contents/evil.nsp") == "<absent>");
}

TEST_CASE("an archive that is not firmware is reported rather than handed to Daybreak")
{
    // Daybreak given a directory with no NCA wastes a reboot discovering that.
    Rig rig;
    rig.archives.contents = {{"readme.txt", "not firmware"}};

    FirmwareInstallService service = rig.service();
    const FirmwareOutcome out = service.stage(rig.item);

    CHECK(isResult(out.result, FirmwareResult::NotFirmwareContent));
    CHECK_FALSE(out.readyForDaybreak());
    CHECK(out.daybreakArgs.empty());
}

TEST_CASE("an archive that expands to nothing is reported")
{
    Rig rig;
    rig.archives.contents.clear();

    FirmwareInstallService service = rig.service();
    const FirmwareOutcome out = service.stage(rig.item);

    CHECK(isResult(out.result, FirmwareResult::NotFirmwareContent));
    CHECK(out.detail.find("nothing") != std::string::npos);
}

TEST_CASE("a file that cannot be removed stops the clear rather than continuing")
{
    // Continuing would leave NCAs from two versions in one directory, which is
    // precisely the state that makes Daybreak install a system that will not
    // boot.
    Rig rig;
    rig.withPreviousFirmware();
    rig.files.removeFailures.insert("/firmware/0100000000000801.nca");

    FirmwareInstallService service = rig.service();
    const FirmwareOutcome out = service.stage(rig.item);

    CHECK(isResult(out.result, FirmwareResult::ClearFailed));
    CHECK_FALSE(out.readyForDaybreak());
    CHECK(out.detail.find("0100000000000801.nca") != std::string::npos);
}

// ---------------------------------------------------------------------------
// inspect()
// ---------------------------------------------------------------------------

TEST_CASE("inspect reports without changing anything")
{
    Rig rig;
    rig.withPreviousFirmware();
    rig.files.files["/firmware/stray.txt"] = "x";

    FirmwareInstallService service = rig.service();
    const core::FirmwareDirectoryScan scan = service.inspect();

    CHECK(scan.firmwareFiles == 2);
    CHECK(scan.unexpected.size() == 1);
    CHECK(rig.at("/firmware/stray.txt") == "x");
    CHECK(rig.at("/firmware/0100000000000800.nca") == "old-a");
}

TEST_CASE("every firmware result has a description")
{
    for (const FirmwareResult r :
         {FirmwareResult::Ready, FirmwareResult::NotFirmwareItem, FirmwareResult::InsufficientSpace,
          FirmwareResult::DownloadFailed, FirmwareResult::VerifyFailed,
          FirmwareResult::UnsafeArchive, FirmwareResult::ExtractFailed,
          FirmwareResult::DirectoryNotOurs, FirmwareResult::ClearFailed,
          FirmwareResult::NotFirmwareContent, FirmwareResult::DaybreakMissing,
          FirmwareResult::Cancelled}) {
        CHECK_FALSE(describe(r).empty());
        CHECK(describe(r) != "unknown");
    }
}
