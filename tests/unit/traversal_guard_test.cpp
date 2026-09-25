// SPDX-License-Identifier: GPL-3.0-only
//
// The zip-slip guard.
//
// This is the single most security-sensitive pure function in the project. The
// predecessor called chdir("/") and extracted downloaded archives straight over
// the SD card root (utils.cpp:173), which means any hostile entry name in any
// downloaded pack could write anywhere - /atmosphere/, /bootloader/, the
// application itself.
//
// So the tests here are deliberately exhaustive rather than representative. An
// entry name that gets through is not a bug in a feature, it is arbitrary file
// write on someone's console.

#include "nsx/core/paths/traversal_guard.hpp"

#include <cstdint>
#include <string>

#include <doctest.h>

using namespace nsx::core;

namespace {

constexpr const char* kRoot = "/config/nsx-manager/staging/pack";

bool accepted(const std::string& entry)
{
    return validateEntryName(entry).hasValue();
}

PathError rejection(const std::string& entry)
{
    const Result<std::monostate, PathError> r = validateEntryName(entry);
    REQUIRE_FALSE(r.hasValue());
    return r.error();
}

}  // namespace

// ---------------------------------------------------------------------------
// What a real archive contains, and must keep working
// ---------------------------------------------------------------------------

TEST_CASE("ordinary entry names are accepted")
{
    for (const char* good : {
             "hekate_ipl.ini",
             "atmosphere/contents/0100000000001000/exefs.nsp",
             "bootloader/payloads/fusee.bin",
             "switch/.overlays/ovlmenu.ovl",
             "a",
             "deeply/nested/but/perfectly/ordinary/file.txt",
             "unicode/ação-日本語.txt",
             "spaces are fine.txt",
             "dots.in.the.middle.are.fine",
             "-leading-hyphen",
             "trailing/directory/",
         }) {
        CAPTURE(good);
        CHECK(accepted(good));
    }
}

TEST_CASE("a hidden file is not traversal")
{
    // `.overlays` and `.gitkeep` start with a dot without being a `.` component.
    CHECK(accepted(".overlays/ovlmenu.ovl"));
    CHECK(accepted("switch/.config"));
    CHECK(accepted("..leading-dots-in-a-name"));
}

// ---------------------------------------------------------------------------
// Parent traversal - the primitive itself
// ---------------------------------------------------------------------------

TEST_CASE("every shape of parent traversal is refused")
{
    for (const char* evil : {
             "../evil",
             "../../evil",
             "../../../../../../../../atmosphere/contents/evil.nsp",
             "a/../evil",
             "a/b/../../evil",
             "a/../../evil",
             "..",
             "../",
             "a/..",
             "a/../",
             "ok/../ok/../../escape",
             "atmosphere/../../bootloader/hekate_ipl.ini",
         }) {
        CAPTURE(evil);
        CHECK(rejection(evil) == PathError::ParentTraversal);
    }
}

TEST_CASE("a rewritable traversal is still refused, not rewritten")
{
    // `a/../b` resolves to `b`, which is harmless. It is refused anyway: an
    // entry whose name does not say where it lands is the precondition for
    // every zip-slip bug, and accepting the harmless case means owning the
    // normalisation that distinguishes it from the dangerous one.
    CHECK_FALSE(accepted("a/../b"));
    CHECK(rejection("a/../b") == PathError::ParentTraversal);
}

// ---------------------------------------------------------------------------
// Absolute paths and volumes
// ---------------------------------------------------------------------------

TEST_CASE("absolute entry names are refused")
{
    CHECK(rejection("/atmosphere/contents/evil.nsp") == PathError::AbsolutePath);
    CHECK(rejection("/") == PathError::AbsolutePath);
    CHECK(rejection("//evil") == PathError::AbsolutePath);
}

TEST_CASE("a volume prefix is refused wherever it appears")
{
    // sdmc:/x is absolute ON THE CONSOLE regardless of position, because the
    // devoptab resolves the prefix rather than the leading separator.
    for (const char* evil : {"sdmc:/atmosphere/evil", "C:/evil", "C:evil", "a/sdmc:/evil",
                             "romfs:/nsx-forwarder.nro", "already/deep/C:evil"}) {
        CAPTURE(evil);
        CHECK(rejection(evil) == PathError::VolumePrefix);
    }
}

// ---------------------------------------------------------------------------
// Separators and bytes that break the check itself
// ---------------------------------------------------------------------------

TEST_CASE("backslashes are refused")
{
    // A separator on a host, ambiguous on FatFs. `..\..\x` traverses on one and
    // is an odd but legal filename on the other; neither outcome is acceptable
    // for a name we are about to write to.
    CHECK(rejection("..\\..\\evil") == PathError::BackslashSeparator);
    CHECK(rejection("a\\b") == PathError::BackslashSeparator);
    CHECK(rejection("trailing\\") == PathError::BackslashSeparator);
}

TEST_CASE("a null byte is refused, and refused before anything else is concluded")
{
    // "safe.txt\0../../evil" reads as safe to anything using a C string and
    // carries the payload past it. std::string_view keeps the whole thing,
    // which is exactly why this has to be checked over the full length.
    const std::string smuggled = std::string("safe.txt") + '\0' + "../../evil";
    REQUIRE(smuggled.size() > 8);
    CHECK(rejection(smuggled) == PathError::NullByte);

    CHECK(rejection(std::string("\0", 1)) == PathError::NullByte);
    CHECK(rejection(std::string("a") + '\0') == PathError::NullByte);
}

TEST_CASE("control characters are refused")
{
    CHECK(rejection("bell\x07.txt") == PathError::ControlCharacter);
    CHECK(rejection("newline\n.txt") == PathError::ControlCharacter);
    CHECK(rejection("\x1b[2Jclear-the-screen") == PathError::ControlCharacter);
    CHECK(rejection("delete\x7f.txt") == PathError::ControlCharacter);
}

// ---------------------------------------------------------------------------
// Structure
// ---------------------------------------------------------------------------

TEST_CASE("empty and dot components are refused rather than collapsed")
{
    CHECK(rejection("a//b") == PathError::EmptyComponent);
    CHECK(rejection("a///b") == PathError::EmptyComponent);
    CHECK(rejection("a//") == PathError::EmptyComponent);
    CHECK(rejection("./a") == PathError::DotComponent);
    CHECK(rejection("a/./b") == PathError::DotComponent);
    CHECK(rejection(".") == PathError::DotComponent);
    CHECK(rejection("a/.") == PathError::DotComponent);
}

TEST_CASE("an empty name is refused")
{
    CHECK(rejection("") == PathError::Empty);
    CHECK(rejection("/") == PathError::AbsolutePath);
}

TEST_CASE("a trailing dot or space is refused because FAT strips it")
{
    // `foo.` and `foo` are the same file on FAT. An archive carrying both
    // overwrites one with the other while every name comparison reports them
    // as different entries.
    CHECK(rejection("foo.") == PathError::TrailingDotOrSpace);
    CHECK(rejection("foo ") == PathError::TrailingDotOrSpace);
    CHECK(rejection("dir./file") == PathError::TrailingDotOrSpace);
    CHECK(rejection("dir /file") == PathError::TrailingDotOrSpace);
    CHECK(rejection("a/b.") == PathError::TrailingDotOrSpace);

    // ... but only at the end of a component.
    CHECK(accepted("foo.txt"));
    CHECK(accepted("a b/c d.txt"));
}

TEST_CASE("length limits are enforced per component and overall")
{
    CHECK(accepted(std::string(kMaxComponentLength, 'a')));
    CHECK(rejection(std::string(kMaxComponentLength + 1, 'a')) == PathError::ComponentTooLong);

    std::string deep;
    while (deep.size() <= kMaxPathLength) {
        deep += "aaaaaaaa/";
    }
    CHECK(rejection(deep) == PathError::PathTooLong);
}

// ---------------------------------------------------------------------------
// resolveUnder - the function an extractor actually calls
// ---------------------------------------------------------------------------

TEST_CASE("a valid entry resolves under the root")
{
    const Result<std::string, PathError> r = resolveUnder(kRoot, "atmosphere/contents/x.nsp");
    REQUIRE(r.hasValue());
    CHECK(r.value() == std::string(kRoot) + "/atmosphere/contents/x.nsp");
}

TEST_CASE("a directory entry keeps its trailing separator")
{
    const Result<std::string, PathError> r = resolveUnder(kRoot, "atmosphere/contents/");
    REQUIRE(r.hasValue());
    CHECK(r.value() == std::string(kRoot) + "/atmosphere/contents/");
}

TEST_CASE("a root with a trailing separator does not produce a doubled one")
{
    const Result<std::string, PathError> r = resolveUnder("/config/pack/", "a/b.txt");
    REQUIRE(r.hasValue());
    CHECK(r.value() == "/config/pack/a/b.txt");
    CHECK(r.value().find("//") == std::string::npos);
}

TEST_CASE("resolveUnder refuses everything validateEntryName refuses")
{
    for (const char* evil : {"../evil", "/absolute", "sdmc:/x", "a\\b", "a//b", "foo.", ""}) {
        CAPTURE(evil);
        CHECK_FALSE(resolveUnder(kRoot, evil).hasValue());
    }
}

TEST_CASE("an empty root is refused")
{
    CHECK(resolveUnder("", "a.txt").error() == PathError::Empty);
}

TEST_CASE("THE INVARIANT: anything resolveUnder accepts lands inside the root")
{
    // The property the extractor depends on, stated once and checked against
    // every name in this file - the hostile ones included, since those must
    // either be refused or land inside, and never anything else.
    for (const char* entry : {
             "ordinary.txt",
             "a/b/c.txt",
             "dir/",
             "unicode/ação.txt",
             "../evil",
             "../../evil",
             "a/../evil",
             "/absolute/evil",
             "sdmc:/evil",
             "a\\b",
             "a//b",
             "./a",
             "foo.",
             "..",
             "",
             "a/./b",
             "atmosphere/../../bootloader/x",
         }) {
        CAPTURE(entry);
        const Result<std::string, PathError> r = resolveUnder(kRoot, entry);
        if (r.hasValue()) {
            CHECK(isWithin(kRoot, r.value()));
        }
    }
}

// ---------------------------------------------------------------------------
// isWithin
// ---------------------------------------------------------------------------

TEST_CASE("containment is decided by component, never by string prefix")
{
    // The bug a prefix test produces: /switch/nsx is a string prefix of
    // /switch/nsx-evil while being no parent of it.
    CHECK(isWithin("/switch/nsx", "/switch/nsx/file"));
    CHECK_FALSE(isWithin("/switch/nsx", "/switch/nsx-evil/file"));
    CHECK_FALSE(isWithin("/switch/nsx", "/switch/nsxevil"));
}

TEST_CASE("a directory contains itself")
{
    CHECK(isWithin("/switch/nsx", "/switch/nsx"));
    CHECK(isWithin("/switch/nsx", "/switch/nsx/"));
    CHECK(isWithin("/switch/nsx/", "/switch/nsx"));
}

TEST_CASE("a parent is not inside its child")
{
    CHECK_FALSE(isWithin("/switch/nsx/deep", "/switch/nsx"));
    CHECK_FALSE(isWithin("/switch/nsx", "/switch"));
    CHECK_FALSE(isWithin("/switch/nsx", "/"));
}

TEST_CASE("a parent reference below the root is refused even when handed in directly")
{
    // isWithin may be called on a path this module did not build.
    CHECK_FALSE(isWithin("/switch/nsx", "/switch/nsx/../../evil"));
    CHECK_FALSE(isWithin("/switch/nsx", "/switch/nsx/.."));
    CHECK_FALSE(isWithin("/switch/nsx", "/switch/nsx/a/../../../evil"));
    CHECK_FALSE(isWithin("/switch/nsx", "/switch/nsx//evil"));
}

TEST_CASE("containment is case-sensitive, which is the refusing direction")
{
    // FAT is case-insensitive, so these are the same directory on hardware.
    // Reporting "not within" refuses, and refusing cannot be used to escape -
    // escaping needs `..`, which validateEntryName rejects outright.
    CHECK_FALSE(isWithin("/switch/nsx", "/SWITCH/NSX/file"));
}

TEST_CASE("empty arguments are not within anything")
{
    CHECK_FALSE(isWithin("", "/a"));
    CHECK_FALSE(isWithin("/a", ""));
    CHECK_FALSE(isWithin("", ""));
}

// ---------------------------------------------------------------------------
// The invariant, against everything a hostile archive could name
// ---------------------------------------------------------------------------

TEST_CASE("fuzz: nothing accepted can escape, and nothing accepted holds a parent reference")
{
    // Deterministic, so a failure is reproducible from the seed printed below.
    // The alphabet is entirely made of the pieces that matter - separators,
    // dots, volume markers, NUL, control bytes - so a random string from it is
    // far more likely to be an attack than a random string from ASCII.
    static constexpr char kAlphabet[] = {'a',  'b', '/', '/',  '.',  '.',    '.',
                                         '\\', ':', ' ', '\0', '\n', '\x1b', '-'};
    constexpr std::size_t kAlphabetSize = sizeof(kAlphabet);

    std::uint64_t state = 0x9E3779B97F4A7C15ULL;
    auto next = [&state]() {
        state ^= state << 13u;
        state ^= state >> 7u;
        state ^= state << 17u;
        return state;
    };

    const std::string root = kRoot;
    std::size_t accepted_count = 0;

    for (int iteration = 0; iteration < 200000; ++iteration) {
        const std::size_t length = 1 + (next() % 24u);
        std::string entry;
        entry.reserve(length);
        for (std::size_t i = 0; i < length; ++i) {
            entry.push_back(kAlphabet[next() % kAlphabetSize]);
        }

        const Result<std::string, PathError> resolved = resolveUnder(root, entry);
        if (!resolved.hasValue()) {
            continue;
        }
        ++accepted_count;

        const std::string& out = resolved.value();

        // Component-wise, NOT a substring search. `..` inside a name is an
        // ordinary filename - `..leading-dots` is accepted on purpose above -
        // and a substring test would call that an escape. The first version of
        // this loop did exactly that and the fuzzer found it in 503 iterations
        // with the entry `.b ..a`, which is one legitimate file.
        bool hasParentComponent = false;
        for (std::size_t start = 0; start <= out.size();) {
            const std::size_t slash = out.find('/', start);
            const std::size_t stop = slash == std::string::npos ? out.size() : slash;
            const std::string_view component(out.data() + start, stop - start);
            if (component == ".." || component == ".") {
                hasParentComponent = true;
                break;
            }
            if (slash == std::string::npos) {
                break;
            }
            start = slash + 1;
        }

        // Two independent statements of the same guarantee. If either can be
        // broken the extractor can be made to write outside its destination.
        if (!isWithin(root, out) || hasParentComponent) {
            FAIL("escaped with entry: " << entry << " -> " << out << " (iteration " << iteration
                                        << ")");
        }
    }

    // A fuzz run that accepted nothing would pass while testing nothing.
    CHECK(accepted_count > 1000);
    MESSAGE("fuzz accepted " << accepted_count << " of 200000 generated names");
}

TEST_CASE("every path error has a description")
{
    for (const PathError e :
         {PathError::Empty, PathError::AbsolutePath, PathError::VolumePrefix,
          PathError::ParentTraversal, PathError::BackslashSeparator, PathError::NullByte,
          PathError::ControlCharacter, PathError::EmptyComponent, PathError::DotComponent,
          PathError::TrailingDotOrSpace, PathError::ComponentTooLong, PathError::PathTooLong}) {
        CHECK_FALSE(describe(e).empty());
        CHECK(describe(e) != "unknown path error");
    }
}
