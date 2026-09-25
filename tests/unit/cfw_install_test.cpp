// SPDX-License-Identifier: GPL-3.0-only
//
// The CFW pack install pipeline.
//
// A pack is thousands of files written over a user's working installation, and
// the merge takes minutes. The interesting tests are therefore not the happy
// path but every way it can stop half done: a failed rename, a cancellation, a
// power cut. Each one must leave the card either fully updated or exactly as it
// was, and never in between.

#include <deque>
#include <iterator>
#include <map>
#include <set>
#include <string>

#include <doctest.h>

#include "nsx/domain/cfw/cfw_install_service.hpp"

using namespace nsx::domain;
namespace core = nsx::core;
namespace infra = nsx::infra;

namespace {

constexpr std::int64_t kNow = 1'760'000'000;

class FakeClock final : public Clock
{
public:
    std::int64_t now{kNow};

    [[nodiscard]] std::int64_t nowUnix() const override { return now; }
};

/// An in-memory card. Paths are keys; a directory exists when something is
/// under it, which is close enough to FAT for this flow.
class FakeFileStore final : public FileStore
{
public:
    std::map<std::string, std::string> files;
    std::set<std::string> dirs{"/", "/config", "/switch", "/bootloader", "/atmosphere"};
    std::optional<std::uint64_t> freeSpace{64ULL * 1024 * 1024 * 1024};

    /// Paths that refuse to be written - how an I/O failure part way through a
    /// merge is produced.
    std::set<std::string> writeFailures;

    /// A file that cannot be moved OUT of staging: the package's copy is bad.
    /// The merge fails; putting the original back still works.
    std::set<std::string> renameSourceFailures;

    /// A destination that cannot be written at all. The merge fails AND the
    /// restore fails, which is a different outcome and must be reported as one.
    std::set<std::string> renameDestFailures;

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
        if (writeFailures.count(path) != 0) {
            return false;
        }
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
        return freeSpace;
    }

    [[nodiscard]] bool rename(const std::string& from, const std::string& to) override
    {
        if (renameSourceFailures.count(from) != 0 || renameDestFailures.count(to) != 0) {
            return false;
        }
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

class FakeHttp final : public HttpGateway
{
public:
    explicit FakeHttp(FakeFileStore& files) : m_files(files) {}

    std::string body{"archive-bytes"};
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

/// Writes a canned set of files into the staging tree, the way a real
/// extraction would, and runs each name through the real policy so a hostile
/// entry is refused here exactly as it would be on device.
class FakeArchives final : public ArchiveGateway
{
public:
    explicit FakeArchives(FakeFileStore& files) : m_files(files) {}

    std::map<std::string, std::string> contents{
        {"atmosphere/package3", "new-package3"},
        {"bootloader/hekate_ipl.ini", "PACK BOOT ENTRIES"},
        {"switch/tool/tool.nro", "new-tool"},
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

core::CatalogItem packItem()
{
    core::CatalogItem item;
    item.id = "atmosphere-pack";
    item.name = "Atmosphere Pack";
    item.versionText = "1.7.1";
    item.kind = core::ContentKind::CfwPack;
    item.target = core::InstallTarget::SdRoot;
    item.url = "https://example.invalid/pack.zip";
    item.honourPreserveRules = true;
    return item;
}

struct Rig
{
    FakeClock clock;
    FakeFileStore files;
    FakeHttp http{files};
    FakeArchives archives{files};
    CfwInstallConfig config;
    core::CatalogItem item{packItem()};

    Rig()
    {
        // The catalogue's declared size and digest must match what the fake
        // server will actually send, or every test fails at the download.
        item.size = http.body.size();
        item.sha256 = core::Sha256::of(http.body);
    }

    CfwInstallService service() { return CfwInstallService(http, archives, files, clock, config); }

    /// Put a working installation on the card for the pack to land on.
    void withExistingInstall()
    {
        files.files["/atmosphere/package3"] = "old-package3";
        files.files["/bootloader/hekate_ipl.ini"] = "MY OWN BOOT ENTRIES";
        files.files["/switch/tool/tool.nro"] = "old-tool";
    }

    [[nodiscard]] std::string at(const std::string& path) const
    {
        const auto it = files.files.find(path);
        return it == files.files.end() ? std::string("<absent>") : it->second;
    }

    [[nodiscard]] bool stagingClean() const
    {
        return files.listFilesRecursive(config.stagingRoot).empty();
    }
};

bool isResult(InstallResult a, InstallResult b)
{
    return a == b;
}

}  // namespace

// ---------------------------------------------------------------------------
// The happy path
// ---------------------------------------------------------------------------

TEST_CASE("a pack downloads, extracts and merges")
{
    Rig rig;
    rig.withExistingInstall();

    CfwInstallService service = rig.service();
    const InstallOutcome out = service.install(rig.item, core::PreserveRules::defaults());

    CHECK(isResult(out.result, InstallResult::Installed));
    CHECK(out.installed());
    CHECK(rig.at("/atmosphere/package3") == "new-package3");
    CHECK(rig.at("/switch/tool/tool.nro") == "new-tool");
    CHECK(out.filesMerged == 2);
}

TEST_CASE("preserve.txt keeps the user's file and the package's copy is dropped")
{
    Rig rig;
    rig.withExistingInstall();

    CfwInstallService service = rig.service();
    const InstallOutcome out = service.install(rig.item, core::PreserveRules::defaults());

    CHECK(rig.at("/bootloader/hekate_ipl.ini") == "MY OWN BOOT ENTRIES");
    CHECK(out.filesPreserved == 1);
}

TEST_CASE("a preserved file that does not exist yet is still installed")
{
    // Preserving means "do not replace what is there". With nothing there, the
    // package's copy must land, or a fresh card never gets a hekate_ipl.ini.
    Rig rig;  // no existing install
    CfwInstallService service = rig.service();
    const InstallOutcome out = service.install(rig.item, core::PreserveRules::defaults());

    CHECK(isResult(out.result, InstallResult::Installed));
    CHECK(rig.at("/bootloader/hekate_ipl.ini") == "PACK BOOT ENTRIES");
    CHECK(out.filesPreserved == 0);
}

TEST_CASE("an item that does not honour preserve rules replaces everything")
{
    // A self-contained download needs to replace its own files; a preserve rule
    // that skipped one would leave half a package installed.
    Rig rig;
    rig.withExistingInstall();
    rig.item.honourPreserveRules = false;

    CfwInstallService service = rig.service();
    const InstallOutcome out = service.install(rig.item, core::PreserveRules::defaults());

    CHECK(rig.at("/bootloader/hekate_ipl.ini") == "PACK BOOT ENTRIES");
    CHECK(out.filesPreserved == 0);
}

TEST_CASE("staging and the archive are gone once it commits")
{
    Rig rig;
    rig.withExistingInstall();
    CfwInstallService service = rig.service();
    REQUIRE(service.install(rig.item, core::PreserveRules::defaults()).installed());

    CHECK(rig.stagingClean());
    CHECK_FALSE(rig.files.exists(rig.config.markerPath()));
    CHECK_FALSE(rig.files.exists(rig.config.archiveFor(rig.item.id)));
    CHECK(rig.files.listFilesRecursive(rig.config.backupFor(rig.item.id)).empty());
}

// ---------------------------------------------------------------------------
// Refusals before anything is touched
// ---------------------------------------------------------------------------

TEST_CASE("insufficient space is refused before the first byte")
{
    Rig rig;
    rig.withExistingInstall();
    rig.files.freeSpace = 1;

    CfwInstallService service = rig.service();
    const InstallOutcome out = service.install(rig.item, core::PreserveRules::defaults());

    CHECK(isResult(out.result, InstallResult::InsufficientSpace));
    CHECK(rig.http.downloads == 0);
    CHECK(out.cardUnchanged());
    CHECK(rig.at("/atmosphere/package3") == "old-package3");
}

TEST_CASE("unknown free space proceeds rather than refusing")
{
    Rig rig;
    rig.files.freeSpace = std::nullopt;
    CfwInstallService service = rig.service();
    CHECK(service.install(rig.item, core::PreserveRules::defaults()).installed());
}

TEST_CASE("a download that does not verify leaves the card alone")
{
    Rig rig;
    rig.withExistingInstall();
    rig.http.error = infra::HttpError::DigestMismatch;

    CfwInstallService service = rig.service();
    const InstallOutcome out = service.install(rig.item, core::PreserveRules::defaults());

    CHECK(isResult(out.result, InstallResult::VerifyFailed));
    CHECK(rig.archives.extractions == 0);
    CHECK(rig.at("/atmosphere/package3") == "old-package3");
    CHECK(rig.stagingClean());
}

TEST_CASE("a hostile archive is refused with the card untouched")
{
    // The crucial ordering property: extraction happens into staging, so an
    // archive that tries to escape is discovered while the installation is
    // still exactly as the user left it.
    Rig rig;
    rig.withExistingInstall();
    rig.archives.contents["../../../../atmosphere/contents/evil.nsp"] = "PWNED";

    CfwInstallService service = rig.service();
    const InstallOutcome out = service.install(rig.item, core::PreserveRules::defaults());

    CHECK(isResult(out.result, InstallResult::UnsafeArchive));
    CHECK(rig.at("/atmosphere/package3") == "old-package3");
    CHECK(rig.at("/bootloader/hekate_ipl.ini") == "MY OWN BOOT ENTRIES");
    CHECK(out.cardUnchanged());
    CHECK(rig.stagingClean());
}

TEST_CASE("an archive that expands to nothing is an error, not a silent success")
{
    Rig rig;
    rig.archives.contents.clear();

    CfwInstallService service = rig.service();
    CHECK(isResult(service.install(rig.item, core::PreserveRules::defaults()).result,
                   InstallResult::ExtractFailed));
}

// ---------------------------------------------------------------------------
// Failure DURING the merge - the part that can leave a card broken
// ---------------------------------------------------------------------------

TEST_CASE("a rename that fails part way rolls the whole merge back")
{
    Rig rig;
    rig.withExistingInstall();
    // The package's copy of this file cannot be moved out of staging.
    rig.files.renameSourceFailures.insert(rig.config.stagingFor(rig.item.id) +
                                          "/switch/tool/tool.nro");

    CfwInstallService service = rig.service();
    const InstallOutcome out = service.install(rig.item, core::PreserveRules::defaults());

    CHECK(isResult(out.result, InstallResult::RolledBack));

    // Everything back exactly as it was - including the file that HAD already
    // been replaced before the failure.
    CHECK(rig.at("/atmosphere/package3") == "old-package3");
    CHECK(rig.at("/bootloader/hekate_ipl.ini") == "MY OWN BOOT ENTRIES");
    CHECK(rig.at("/switch/tool/tool.nro") == "old-tool");
}

TEST_CASE("a rollback removes files the pack newly created")
{
    // A file with nothing to restore over is not covered by the backup. It has
    // to be removed by name, or a failed install leaves pieces of a pack behind.
    Rig rig;
    rig.files.files["/atmosphere/package3"] = "old-package3";  // only this one exists
    rig.files.renameSourceFailures.insert(rig.config.stagingFor(rig.item.id) +
                                          "/switch/tool/tool.nro");

    CfwInstallService service = rig.service();
    const InstallOutcome out = service.install(rig.item, core::PreserveRules::defaults());

    CHECK(isResult(out.result, InstallResult::RolledBack));
    CHECK(rig.at("/atmosphere/package3") == "old-package3");
    CHECK(rig.at("/bootloader/hekate_ipl.ini") == "<absent>");
    CHECK(rig.at("/switch/tool/tool.nro") == "<absent>");
}

TEST_CASE("a rollback clears staging, the backup and the marker")
{
    Rig rig;
    rig.withExistingInstall();
    rig.files.renameSourceFailures.insert(rig.config.stagingFor(rig.item.id) +
                                          "/switch/tool/tool.nro");

    CfwInstallService service = rig.service();
    (void)service.install(rig.item, core::PreserveRules::defaults());

    CHECK(rig.stagingClean());
    CHECK_FALSE(rig.files.exists(rig.config.markerPath()));
}

TEST_CASE("a rollback that cannot finish says so instead of claiming success")
{
    // The destination is unwritable, so the package cannot be installed AND the
    // original cannot be put back. Reporting a clean undo here would be worse
    // than the failure itself: the user would stop looking at a card that is
    // genuinely missing a file.
    Rig rig;
    rig.withExistingInstall();
    rig.files.renameDestFailures.insert("/switch/tool/tool.nro");

    CfwInstallService service = rig.service();
    const InstallOutcome out = service.install(rig.item, core::PreserveRules::defaults());

    CHECK(isResult(out.result, InstallResult::MergeFailed));
    CHECK_FALSE(out.cardUnchanged());
    CHECK(out.detail.find("could not be put back") != std::string::npos);

    // What COULD be restored still was.
    CHECK(rig.at("/atmosphere/package3") == "old-package3");
}

TEST_CASE("cancelling during the merge restores what was already moved")
{
    Rig rig;
    rig.withExistingInstall();

    int seen = 0;
    CfwInstallService service = rig.service();
    const InstallOutcome out = service.install(
        rig.item, core::PreserveRules::defaults(), [&seen](const InstallProgress& p) {
            if (p.stage == InstallStage::Merging && ++seen >= 2) {
                return false;
            }
            return true;
        });

    CHECK(isResult(out.result, InstallResult::Cancelled));
    CHECK(rig.at("/atmosphere/package3") == "old-package3");
    CHECK(rig.at("/switch/tool/tool.nro") == "old-tool");
    CHECK(rig.stagingClean());
}

TEST_CASE("the marker is written before the merge starts, not after")
{
    // If a merge could begin without a marker, a power cut in that window would
    // be unrecoverable: nothing would say a merge had been in flight.
    Rig rig;
    rig.withExistingInstall();

    bool markerSeenDuringMerge = false;
    CfwInstallService service = rig.service();
    (void)service.install(rig.item, core::PreserveRules::defaults(), [&](const InstallProgress& p) {
        if (p.stage == InstallStage::Merging && rig.files.exists(rig.config.markerPath())) {
            markerSeenDuringMerge = true;
        }
        return true;
    });

    CHECK(markerSeenDuringMerge);
}

// ---------------------------------------------------------------------------
// Recovery after a power cut
// ---------------------------------------------------------------------------

TEST_CASE("nothing in flight means nothing to recover")
{
    Rig rig;
    rig.withExistingInstall();
    CfwInstallService service = rig.service();
    const InstallOutcome out = service.recoverInterruptedMerge();

    CHECK(isResult(out.result, InstallResult::Installed));
    CHECK(rig.at("/atmosphere/package3") == "old-package3");
}

TEST_CASE("a merge interrupted by a power cut is undone at next start")
{
    // Reconstruct the exact state a power cut leaves: the marker is there, the
    // backup holds one displaced file, the destination holds the pack's copy of
    // it, and a second file has been created that never existed before.
    Rig rig;
    const std::string backup = rig.config.backupFor(rig.item.id);
    const std::string staging = rig.config.stagingFor(rig.item.id);

    core::MergeMarker marker;
    marker.createdAtUnix = kNow;
    marker.itemId = rig.item.id;
    marker.itemName = rig.item.name;
    marker.stagingDir = staging;
    marker.destinationRoot = "/";
    marker.backupDir = backup;
    rig.files.files[rig.config.markerPath()] = core::serializeMergeMarker(marker);
    rig.files.files[rig.config.mergeFileListPath()] =
        "atmosphere/package3\nbootloader/hekate_ipl.ini\nswitch/tool/tool.nro\n";

    // The state a power cut actually leaves. The merge had installed both
    // files, so their staged copies are GONE - it moves them out rather than
    // copying. package3 displaced an original, which is in the backup; tool.nro
    // was new, so it has none. hekate_ipl.ini was preserved and therefore never
    // left staging, which is what proves the rollback can tell the two apart.
    rig.files.files[backup + "/atmosphere/package3"] = "old-package3";
    rig.files.files["/atmosphere/package3"] = "new-package3";
    rig.files.files["/switch/tool/tool.nro"] = "new-tool";
    rig.files.files[staging + "/bootloader/hekate_ipl.ini"] = "PACK BOOT ENTRIES";
    rig.files.files["/bootloader/hekate_ipl.ini"] = "MY OWN BOOT ENTRIES";

    CfwInstallService service = rig.service();
    const InstallOutcome out = service.recoverInterruptedMerge();

    CHECK(isResult(out.result, InstallResult::RolledBack));
    CHECK(rig.at("/atmosphere/package3") == "old-package3");               // displaced, restored
    CHECK(rig.at("/switch/tool/tool.nro") == "<absent>");                  // created, removed
    CHECK(rig.at("/bootloader/hekate_ipl.ini") == "MY OWN BOOT ENTRIES");  // preserved, untouched
    CHECK_FALSE(rig.files.exists(rig.config.markerPath()));
    CHECK(rig.stagingClean());
}

TEST_CASE("an unreadable marker is reported, never guessed at")
{
    // Something was in flight and we cannot tell what. Deleting from directories
    // we are guessing about is the one thing not to do.
    Rig rig;
    rig.withExistingInstall();
    rig.files.files[rig.config.markerPath()] = "{ truncated";

    CfwInstallService service = rig.service();
    const InstallOutcome out = service.recoverInterruptedMerge();

    CHECK(isResult(out.result, InstallResult::MergeFailed));
    CHECK(rig.at("/atmosphere/package3") == "old-package3");
    CHECK_FALSE(out.detail.empty());
}

TEST_CASE("a marker with nothing left behind it is just cleared")
{
    Rig rig;
    core::MergeMarker marker;
    marker.createdAtUnix = kNow;
    marker.itemId = rig.item.id;
    marker.stagingDir = rig.config.stagingFor(rig.item.id);
    marker.destinationRoot = "/";
    marker.backupDir = rig.config.backupFor(rig.item.id);
    rig.files.files[rig.config.markerPath()] = core::serializeMergeMarker(marker);

    CfwInstallService service = rig.service();
    const InstallOutcome out = service.recoverInterruptedMerge();

    CHECK(isResult(out.result, InstallResult::RolledBack));
    CHECK_FALSE(rig.files.exists(rig.config.markerPath()));
}

TEST_CASE("an install refuses to start while a merge is unresolved")
{
    // Two overlapping backups could not be unwound independently.
    Rig rig;
    rig.withExistingInstall();
    rig.files.files[rig.config.markerPath()] = "{ anything at all";

    CfwInstallService service = rig.service();
    const InstallOutcome out = service.install(rig.item, core::PreserveRules::defaults());

    CHECK(isResult(out.result, InstallResult::Busy));
    CHECK(rig.http.downloads == 0);
}

// ---------------------------------------------------------------------------
// Destinations
// ---------------------------------------------------------------------------

TEST_CASE("every catalogue destination resolves to a configured directory")
{
    const CfwInstallConfig config;
    for (const core::InstallTarget t :
         {core::InstallTarget::SdRoot, core::InstallTarget::AtmosphereContents,
          core::InstallTarget::Bootloader, core::InstallTarget::FirmwareStaging,
          core::InstallTarget::SwitchApps, core::InstallTarget::Themes}) {
        const std::string resolved = config.resolve(t);
        CHECK_FALSE(resolved.empty());
        CHECK(resolved.front() == '/');
    }
}

TEST_CASE("a non-root destination lands under that directory")
{
    Rig rig;
    rig.item.target = core::InstallTarget::AtmosphereContents;
    rig.item.honourPreserveRules = false;
    rig.archives.contents = {{"0100000000001000/exefs.nsp", "patched"}};

    CfwInstallService service = rig.service();
    REQUIRE(service.install(rig.item, core::PreserveRules::defaults()).installed());

    CHECK(rig.at("/atmosphere/contents/0100000000001000/exefs.nsp") == "patched");
}

TEST_CASE("every install result has a description")
{
    for (const InstallResult r :
         {InstallResult::Installed, InstallResult::InsufficientSpace, InstallResult::DownloadFailed,
          InstallResult::VerifyFailed, InstallResult::UnsafeArchive, InstallResult::ExtractFailed,
          InstallResult::MergeFailed, InstallResult::RolledBack, InstallResult::Cancelled,
          InstallResult::Busy}) {
        CHECK_FALSE(describe(r).empty());
        CHECK(describe(r) != "unknown");
    }
}
