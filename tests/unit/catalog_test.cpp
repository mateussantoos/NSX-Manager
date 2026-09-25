// SPDX-License-Identifier: GPL-3.0-only
//
// The content catalogue.
//
// This file decides what the application will download and extract onto
// someone's SD card, so it gets the same treatment as update.json: strict about
// everything that protects the device, tolerant of everything that does not, and
// never throwing whatever it is handed.

#include "nsx/core/catalog/catalog.hpp"

#include <fstream>
#include <sstream>

#include <doctest.h>

using namespace nsx::core;

namespace {

std::string fixture(const std::string& name)
{
    const std::string path = std::string(NSX_FIXTURE_DIR) + "/catalog/" + name;
    std::ifstream in(path, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "fixture not found: " << path);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

Catalog valid()
{
    const Result<Catalog, CatalogError> r = parseCatalog(fixture("valid.json"));
    REQUIRE(r.hasValue());
    return r.value();
}

/// doctest stringifies a compared value by looking for `toString` through ADL.
/// It finds `nsx::core::toString(ContentKind)`, which returns a
/// `std::string_view` it cannot concatenate, so the comparison fails to
/// compile. Comparing through a bool keeps the enums out of that machinery -
/// the same reason the semver suite compares orderings through helpers.
bool isKind(ContentKind a, ContentKind b)
{
    return a == b;
}

bool isTarget(InstallTarget a, InstallTarget b)
{
    return a == b;
}

}  // namespace

// ---------------------------------------------------------------------------
// The golden fixture
// ---------------------------------------------------------------------------

TEST_CASE("valid.json parses")
{
    const Catalog c = valid();
    CHECK(c.schemaVersion == 1);
    CHECK(c.updatedAt == "2026-09-24T12:00:00Z");

    // Five entries in the file; two are aimed at things this version does not
    // understand and are dropped. See the tolerance tests below.
    CHECK(c.items.size() == 3);
}

TEST_CASE("an item carries everything needed to fetch and verify it")
{
    const Catalog c = valid();
    const CatalogItem* pack = c.find("atmosphere-pack");
    REQUIRE(pack != nullptr);

    CHECK(isKind(pack->kind, ContentKind::CfwPack));
    CHECK(isTarget(pack->target, InstallTarget::SdRoot));
    CHECK(pack->versionText == "1.7.1");
    REQUIRE(pack->version.has_value());
    CHECK(pack->version->toString() == "1.7.1");
    CHECK(pack->size == 48210944);
    CHECK(toHex(pack->sha256) ==
          "bfd5a8ed7357ec4de9597c2858902db1a0e3050438fcb5f2b37c1de0e4496b49");
    CHECK(pack->summary.find("Hekate") != std::string::npos);
    REQUIRE(pack->minAtmosphere.has_value());
    CHECK(pack->minAtmosphere->toString() == "1.7.0");
}

TEST_CASE("items can be selected by kind")
{
    const Catalog c = valid();
    CHECK(c.ofKind(ContentKind::CfwPack).size() == 1);
    CHECK(c.ofKind(ContentKind::Firmware).size() == 1);
    CHECK(c.ofKind(ContentKind::Tool).size() == 1);
    CHECK(c.ofKind(ContentKind::Theme).empty());
}

TEST_CASE("an unknown id finds nothing rather than misbehaving")
{
    CHECK(valid().find("does-not-exist") == nullptr);
    CHECK(valid().find("") == nullptr);
}

// ---------------------------------------------------------------------------
// Forward compatibility: one bad entry must not cost the user the rest
// ---------------------------------------------------------------------------

TEST_CASE("an unknown kind drops that item and keeps the others")
{
    // The catalogue is shared by every client version. If adding a content type
    // broke older builds, the catalogue could never gain one.
    const Catalog c = valid();
    CHECK(c.find("future-content") == nullptr);
    CHECK(c.find("atmosphere-pack") != nullptr);
}

TEST_CASE("an unknown destination drops that item too")
{
    // The destination set is closed on purpose. An item aimed somewhere this
    // version does not write is skipped rather than guessed at.
    const Catalog c = valid();
    CHECK(c.find("unknown-destination") == nullptr);
    CHECK(c.items.size() == 3);
}

TEST_CASE("unknown top-level and item fields are tolerated")
{
    const std::string doc = R"({
      "schema_version": 1,
      "curated_by": "someone",
      "items": [{
        "id": "x", "name": "X", "kind": "tool", "target": "switch-apps",
        "url": "https://example.invalid/x.zip", "size": 1,
        "sha256": "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08",
        "future_field": {"nested": true}
      }]
    })";
    const Result<Catalog, CatalogError> r = parseCatalog(doc);
    REQUIRE(r.hasValue());
    CHECK(r.value().items.size() == 1);
}

// ---------------------------------------------------------------------------
// What must be rejected
// ---------------------------------------------------------------------------

TEST_CASE("malformed documents are rejected, never thrown from")
{
    CHECK_NOTHROW((void)parseCatalog(fixture("truncated.json")));
    CHECK(parseCatalog(fixture("truncated.json")).error() == CatalogError::NotJson);
    CHECK(parseCatalog(fixture("future-schema.json")).error() ==
          CatalogError::UnsupportedSchemaVersion);
    CHECK(parseCatalog("[]").error() == CatalogError::NotObject);
    CHECK(parseCatalog("{}").error() == CatalogError::MissingField);
    CHECK(parseCatalog(R"({"schema_version": 1})").error() == CatalogError::MissingField);
}

TEST_CASE("a plaintext url is refused")
{
    // The digest is the real guarantee, but http means a network attacker picks
    // which bytes we hash in the first place.
    CHECK(parseCatalog(fixture("insecure-url.json")).error() == CatalogError::InsecureUrl);
}

TEST_CASE("a missing or malformed digest is refused")
{
    CHECK(parseCatalog(fixture("missing-sha.json")).error() == CatalogError::InvalidDigest);

    for (const char* bad : {"", "not-hex", "ABCD"}) {
        CAPTURE(bad);
        const std::string doc = std::string(R"({"schema_version":1,"items":[{
            "id":"x","name":"X","kind":"tool","target":"switch-apps",
            "url":"https://example.invalid/x.zip","size":1,"sha256":")") +
                                bad + R"("}]})";
        CHECK(parseCatalog(doc).error() == CatalogError::InvalidDigest);
    }
}

TEST_CASE("a zero or non-integer size is refused")
{
    // Without a size there is no truncation check and no ceiling on what a
    // server may send.
    for (const char* bad : {"0", "-1", "\"big\"", "1.5"}) {
        CAPTURE(bad);
        const std::string doc =
            std::string(R"({"schema_version":1,"items":[{
            "id":"x","name":"X","kind":"tool","target":"switch-apps",
            "url":"https://example.invalid/x.zip","size":)") +
            bad +
            R"(,"sha256":"9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"}]})";
        CHECK(parseCatalog(doc).error() == CatalogError::InvalidSize);
    }
}

TEST_CASE("a hostile identifier is refused")
{
    // An id reaches log lines and settings files, and could reach a path.
    for (const char* bad : {"../escape", "has/slash", "has space", "UPPER", "", ".hidden",
                            "has:colon", "has\\backslash"}) {
        CAPTURE(bad);
        const std::string doc = std::string(R"({"schema_version":1,"items":[{
            "id":")") + bad +
                                R"(","name":"X","kind":"tool","target":"switch-apps",
            "url":"https://example.invalid/x.zip","size":1,
            "sha256":"9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"}]})";
        CHECK(parseCatalog(doc).error() == CatalogError::InvalidId);
    }
}

TEST_CASE("two items claiming one id is an error, not a silent winner")
{
    // Taking the first or the last would make the outcome depend on file order.
    CHECK(parseCatalog(fixture("duplicate-id.json")).error() == CatalogError::DuplicateId);
}

TEST_CASE("a catalogue this version can install nothing from is reported as such")
{
    const std::string doc = R"({"schema_version":1,"items":[{
        "id":"x","name":"X","kind":"holographic-shader","target":"sd-root",
        "url":"https://example.invalid/x.zip","size":1,
        "sha256":"9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"}]})";
    CHECK(parseCatalog(doc).error() == CatalogError::NoItems);
    CHECK(parseCatalog(R"({"schema_version":1,"items":[]})").error() == CatalogError::NoItems);
}

TEST_CASE("a plaintext notes url is refused too")
{
    const std::string doc = R"({"schema_version":1,"items":[{
        "id":"x","name":"X","kind":"tool","target":"switch-apps",
        "url":"https://example.invalid/x.zip","size":1,
        "sha256":"9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08",
        "notes_url":"http://example.invalid/notes"}]})";
    CHECK(parseCatalog(doc).error() == CatalogError::InsecureUrl);
}

// ---------------------------------------------------------------------------
// Versions we do not control
// ---------------------------------------------------------------------------

TEST_CASE("an unparseable upstream version is displayed, not rejected")
{
    // Upstream projects tag things like "2026.09" and "v4.2.1-nsx". A version
    // we cannot order is still worth showing the user.
    const std::string doc = R"({"schema_version":1,"items":[{
        "id":"x","name":"X","kind":"tool","target":"switch-apps","version":"weird-build",
        "url":"https://example.invalid/x.zip","size":1,
        "sha256":"9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"}]})";
    const Result<Catalog, CatalogError> r = parseCatalog(doc);
    REQUIRE(r.hasValue());
    CHECK(r.value().items[0].versionText == "weird-build");
    CHECK_FALSE(r.value().items[0].version.has_value());
}

// ---------------------------------------------------------------------------
// preserve.txt applies where it matters
// ---------------------------------------------------------------------------

TEST_CASE("a cfw pack honours preserve rules by default, other kinds do not")
{
    // A pack overwrites the user's whole card; a tool lands in its own folder.
    // Getting this default backwards would either destroy user configuration or
    // silently skip files a self-contained download needs to replace.
    const Catalog c = valid();
    CHECK(c.find("atmosphere-pack")->honourPreserveRules);
    CHECK_FALSE(c.find("ovlloader")->honourPreserveRules);
    CHECK_FALSE(c.find("firmware-19.0.1")->honourPreserveRules);
}

TEST_CASE("the catalogue can override the preserve default explicitly")
{
    const std::string doc = R"({"schema_version":1,"items":[{
        "id":"x","name":"X","kind":"cfw-pack","target":"sd-root",
        "url":"https://example.invalid/x.zip","size":1,
        "sha256":"9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08",
        "honour_preserve_rules":false}]})";
    const Result<Catalog, CatalogError> r = parseCatalog(doc);
    REQUIRE(r.hasValue());
    CHECK_FALSE(r.value().items[0].honourPreserveRules);
}

// ---------------------------------------------------------------------------
// Vocabulary
// ---------------------------------------------------------------------------

TEST_CASE("every error, kind and target has a description")
{
    for (const CatalogError e :
         {CatalogError::NotJson, CatalogError::NotObject, CatalogError::UnsupportedSchemaVersion,
          CatalogError::MissingField, CatalogError::WrongType, CatalogError::InvalidId,
          CatalogError::DuplicateId, CatalogError::InsecureUrl, CatalogError::InvalidDigest,
          CatalogError::InvalidSize, CatalogError::NoItems}) {
        CHECK_FALSE(describe(e).empty());
        CHECK(describe(e) != "unknown catalogue error");
    }
    for (const ContentKind k :
         {ContentKind::CfwPack, ContentKind::Firmware, ContentKind::Tool, ContentKind::Cheats,
          ContentKind::Translation, ContentKind::Theme}) {
        CHECK(std::string(toString(k)) != "unknown");
    }
    for (const InstallTarget t :
         {InstallTarget::SdRoot, InstallTarget::AtmosphereContents, InstallTarget::Bootloader,
          InstallTarget::FirmwareStaging, InstallTarget::SwitchApps, InstallTarget::Themes}) {
        CHECK(std::string(toString(t)) != "unknown");
    }
}

TEST_CASE("every wire spelling round-trips through the parser")
{
    // toString and the parser must agree, or a catalogue generated from our own
    // vocabulary would be rejected by our own reader.
    for (const ContentKind k :
         {ContentKind::CfwPack, ContentKind::Firmware, ContentKind::Tool, ContentKind::Cheats,
          ContentKind::Translation, ContentKind::Theme}) {
        for (const InstallTarget t :
             {InstallTarget::SdRoot, InstallTarget::AtmosphereContents, InstallTarget::Bootloader,
              InstallTarget::FirmwareStaging, InstallTarget::SwitchApps, InstallTarget::Themes}) {
            const std::string doc = std::string(R"({"schema_version":1,"items":[{
                "id":"x","name":"X","kind":")") +
                                    std::string(toString(k)) + R"(","target":")" +
                                    std::string(toString(t)) +
                                    R"(","url":"https://example.invalid/x.zip","size":1,
                "sha256":"9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"}]})";
            CAPTURE(std::string(toString(k)));
            CAPTURE(std::string(toString(t)));
            const Result<Catalog, CatalogError> r = parseCatalog(doc);
            REQUIRE(r.hasValue());
            CHECK(isKind(r.value().items[0].kind, k));
            CHECK(isTarget(r.value().items[0].target, t));
        }
    }
}
