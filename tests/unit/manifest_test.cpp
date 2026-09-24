// SPDX-License-Identifier: GPL-3.0-only
//
// The golden fixtures in tests/fixtures/manifests/ are this parser's
// specification. Their README states what each one must produce, and CI runs
// the same expectations through tools/release/validate_manifest.py - so the
// Python side of the release pipeline and the C++ client cannot drift apart
// about what a valid manifest is.

#include "nsx/core/update/manifest.hpp"

#include <fstream>
#include <sstream>
#include <string>

#include <doctest.h>

using namespace nsx::core;

namespace {

std::string fixture(const std::string& name)
{
    const std::string path = std::string(NSX_FIXTURE_DIR) + "/manifests/" + name;
    std::ifstream in(path, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "fixture not found: " << path);
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

ManifestError errorFrom(const std::string& name)
{
    const Result<UpdateManifest, ManifestError> r = parseManifest(fixture(name));
    REQUIRE_FALSE(r.hasValue());
    return r.error();
}

/// A minimal valid manifest, built field by field so a test can corrupt one
/// thing and leave everything else correct.
std::string makeManifest(const std::string& overrides = "")
{
    std::string base = R"({
      "schema_version": 1,
      "product": "nsx-manager",
      "version": "0.2.0",
      "tag": "v0.2.0",
      "published_at": "2026-10-01T12:00:00Z",
      "channel": "stable",
      "mandatory": false,
      "min_supported": "0.1.0",
      "assets": [
        {
          "name": "nsx-manager-0.2.0.nro",
          "kind": "app-nro",
          "url": "https://github.com/mateussantoos/nsx-manager/releases/download/v0.2.0/nsx-manager-0.2.0.nro",
          "size": 12880849,
          "sha256": "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"
        }
      ])";
    base += overrides.empty() ? "\n}" : ",\n" + overrides + "\n}";
    return base;
}

}  // namespace

// ---------------------------------------------------------------------------
// The golden fixtures - see tests/fixtures/manifests/README.md
// ---------------------------------------------------------------------------

TEST_CASE("fixture valid.json parses")
{
    const Result<UpdateManifest, ManifestError> r = parseManifest(fixture("valid.json"));
    REQUIRE(r.hasValue());
    const UpdateManifest& m = r.value();

    CHECK(m.schemaVersion == 1);
    CHECK(m.version.toString() == "0.2.0");
    CHECK(m.tag == "v0.2.0");
    CHECK(m.channel == Channel::Stable);
    CHECK_FALSE(m.mandatory);
    CHECK(m.minSupported.toString() == "0.1.0");
    REQUIRE(m.minAtmosphere.has_value());
    CHECK(m.minAtmosphere->toString() == "1.7.0");
    CHECK(m.assets.size() == 2);

    const ManifestAsset* app = m.findAsset(AssetKind::AppNro);
    REQUIRE(app != nullptr);
    CHECK(app->name == "nsx-manager-0.2.0.nro");
    CHECK(app->size == 12880849);
    CHECK(toHex(app->sha256) == "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08");

    CHECK(m.findAsset(AssetKind::ForwarderNro) != nullptr);
    CHECK(m.findAsset(AssetKind::SdOverlayZip) == nullptr);
}

TEST_CASE("fixture truncated.json is rejected, not thrown from")
{
    CHECK_NOTHROW((void)parseManifest(fixture("truncated.json")));
    CHECK(errorFrom("truncated.json") == ManifestError::NotJson);
}

TEST_CASE("fixture future-schema.json is refused rather than guessed at")
{
    // The forward-compatibility escape hatch: a client that does not know the
    // format must stop, not interpret what it recognises and ignore the rest.
    CHECK(errorFrom("future-schema.json") == ManifestError::UnsupportedSchemaVersion);
}

TEST_CASE("fixture bad-schema.json is rejected")
{
    const ManifestError e = errorFrom("bad-schema.json");
    CHECK((e == ManifestError::WrongType || e == ManifestError::InvalidVersion ||
           e == ManifestError::InvalidTag));
}

TEST_CASE("fixture missing-sha.json is rejected")
{
    // An asset with no digest can never be verified, so it must never become
    // downloadable.
    CHECK(errorFrom("missing-sha.json") == ManifestError::MissingField);
}

TEST_CASE("fixture hostile-paths.json is rejected")
{
    // Contains an asset named "../../atmosphere/package3" and another served
    // over plaintext http. Either alone is disqualifying.
    const ManifestError e = errorFrom("hostile-paths.json");
    CHECK((e == ManifestError::InvalidAssetName || e == ManifestError::InsecureUrl));
}

// ---------------------------------------------------------------------------
// Field-level validation
// ---------------------------------------------------------------------------

TEST_CASE("a minimal well-formed manifest parses")
{
    const Result<UpdateManifest, ManifestError> r = parseManifest(makeManifest());
    REQUIRE(r.hasValue());
    CHECK(r.value().assets.size() == 1);
}

TEST_CASE("asset names that could escape a directory are rejected")
{
    // A backslash must be escaped for JSON: four here become \\ in the
    // document and a single \ once decoded.
    for (const char* name : {"../evil.nro", "..\\\\evil.nro", "sub/dir.nro", ".hidden", "",
                             "a/../b.nro", "nro with space", "semi;colon"}) {
        CAPTURE(name);
        std::string doc = makeManifest();
        const std::string from = "\"nsx-manager-0.2.0.nro\"";
        const std::string to = std::string("\"") + name + "\"";
        doc.replace(doc.find(from), from.size(), to);

        const Result<UpdateManifest, ManifestError> r = parseManifest(doc);
        REQUIRE_FALSE(r.hasValue());
        CHECK(r.error() == ManifestError::InvalidAssetName);
    }
}

TEST_CASE("non-GitHub and plaintext URLs are rejected")
{
    for (const char* url :
         {"http://github.com/x/y/releases/download/v1/a.nro", "https://evil.example.com/a.nro",
          "ftp://github.com/a.nro", "file:///etc/passwd", "https://github.com"}) {
        CAPTURE(url);
        std::string doc = makeManifest();
        const std::size_t at = doc.find("\"https://github.com/mateussantoos");
        const std::size_t end = doc.find('"', at + 1);
        doc.replace(at, (end - at) + 1, std::string("\"") + url + "\"");

        const Result<UpdateManifest, ManifestError> r = parseManifest(doc);
        REQUIRE_FALSE(r.hasValue());
        CHECK(r.error() == ManifestError::InsecureUrl);
    }
}

TEST_CASE("digests must be 64 lowercase hex characters")
{
    const std::string good = "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08";
    for (const std::string bad : {std::string(""), std::string(63, 'a'), std::string(65, 'a'),
                                  std::string(64, 'A'), std::string(64, 'z')}) {
        CAPTURE(bad.size());
        std::string doc = makeManifest();
        doc.replace(doc.find(good), good.size(), bad);
        const Result<UpdateManifest, ManifestError> r = parseManifest(doc);
        REQUIRE_FALSE(r.hasValue());
        CHECK(r.error() == ManifestError::InvalidDigest);
    }
}

TEST_CASE("sizes must be a positive integer")
{
    for (const char* size : {"0", "-1", "3.5", "true", "\"12\""}) {
        CAPTURE(size);
        std::string doc = makeManifest();
        doc.replace(doc.find("12880849"), 8, size);
        const Result<UpdateManifest, ManifestError> r = parseManifest(doc);
        REQUIRE_FALSE(r.hasValue());
        CHECK((r.error() == ManifestError::InvalidSize || r.error() == ManifestError::WrongType));
    }
}

TEST_CASE("the tag must agree with the version")
{
    std::string doc = makeManifest();
    doc.replace(doc.find("\"v0.2.0\""), 8, "\"v9.9.9\"");
    CHECK(parseManifest(doc).error() == ManifestError::InvalidTag);

    std::string noPrefix = makeManifest();
    noPrefix.replace(noPrefix.find("\"v0.2.0\""), 8, "\"0.2.0\"");
    CHECK(parseManifest(noPrefix).error() == ManifestError::InvalidTag);
}

TEST_CASE("timestamps must be ISO-8601 UTC")
{
    for (const char* ts :
         {"yesterday", "2026-10-01", "2026-10-01T12:00:00+01:00", "2026-10-01 12:00:00Z", ""}) {
        CAPTURE(ts);
        std::string doc = makeManifest();
        const std::string from = "\"2026-10-01T12:00:00Z\"";
        doc.replace(doc.find(from), from.size(), std::string("\"") + ts + "\"");
        CHECK(parseManifest(doc).error() == ManifestError::InvalidTimestamp);
    }
}

TEST_CASE("a manifest for another product is rejected")
{
    std::string doc = makeManifest();
    doc.replace(doc.find("\"nsx-manager\""), 13, "\"some-other-app\"");
    CHECK(parseManifest(doc).error() == ManifestError::WrongProduct);
}

TEST_CASE("a manifest with no application asset is useless and rejected")
{
    std::string doc = makeManifest();
    doc.replace(doc.find("\"app-nro\""), 9, "\"sd-overlay-zip\"");
    CHECK(parseManifest(doc).error() == ManifestError::NoAppAsset);
}

// ---------------------------------------------------------------------------
// Forward compatibility within schema version 1
// ---------------------------------------------------------------------------

TEST_CASE("unknown top-level fields are tolerated")
{
    const Result<UpdateManifest, ManifestError> r =
        parseManifest(makeManifest(R"("future_field": {"anything": [1, 2, 3]})"));
    CHECK(r.hasValue());
}

TEST_CASE("an unknown asset kind is skipped, not fatal")
{
    // A future release adding an asset type must not break this client - but
    // the assets it DOES understand must still come through.
    std::string doc = makeManifest();
    const std::string closing = "      ]";
    const std::string extra = R"(,
        {
          "name": "nsx-manager-0.2.0.sig",
          "kind": "minisign-signature",
          "url": "https://github.com/mateussantoos/nsx-manager/releases/download/v0.2.0/x.sig",
          "size": 128,
          "sha256": "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"
        }
      ])";
    doc.replace(doc.find(closing), closing.size(), extra);

    const Result<UpdateManifest, ManifestError> r = parseManifest(doc);
    REQUIRE(r.hasValue());
    CHECK(r.value().assets.size() == 1);
    CHECK(r.value().findAsset(AssetKind::AppNro) != nullptr);
}

TEST_CASE("every error has a description and kinds render to the wire spelling")
{
    for (const ManifestError e :
         {ManifestError::NotJson, ManifestError::NotObject, ManifestError::UnsupportedSchemaVersion,
          ManifestError::WrongProduct, ManifestError::MissingField, ManifestError::WrongType,
          ManifestError::InvalidVersion, ManifestError::InvalidTag, ManifestError::InvalidTimestamp,
          ManifestError::InvalidChannel, ManifestError::NoAssets, ManifestError::InvalidAssetName,
          ManifestError::InsecureUrl, ManifestError::InvalidDigest, ManifestError::InvalidSize,
          ManifestError::NoAppAsset}) {
        CHECK_FALSE(describe(e).empty());
        CHECK(describe(e) != "unknown error");
    }
    CHECK(toString(AssetKind::AppNro) == "app-nro");
    CHECK(toString(AssetKind::ForwarderNro) == "forwarder-nro");
    CHECK(toString(AssetKind::SdOverlayZip) == "sd-overlay-zip");
}
