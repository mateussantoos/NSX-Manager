// SPDX-License-Identifier: GPL-3.0-only
//
// SHA-256 is the integrity primitive: every downloaded byte is checked against
// it before anything is executed, extracted or renamed into place. A wrong
// implementation would fail open, so the published test vectors are the floor,
// not the ceiling.

#include "nsx/core/hash/sha256.hpp"

#include <string>

#include <doctest.h>

using namespace nsx::core;

TEST_CASE("FIPS 180-4 and NIST CAVP vectors")
{
    // The canonical published vectors. If any of these is wrong, nothing else
    // in this file matters.
    CHECK(Sha256::hexOf("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(Sha256::hexOf("abc") ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(Sha256::hexOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK(Sha256::hexOf("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmn"
                        "hijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu") ==
          "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
    CHECK(Sha256::hexOf("a") == "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb");
}

TEST_CASE("one million 'a' characters")
{
    // The long CAVP vector. Exercises the block loop far past any buffer edge.
    Sha256 h;
    const std::string chunk(1000, 'a');
    for (int i = 0; i < 1000; ++i) {
        h.update(chunk);
    }
    CHECK(h.finishHex() == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("the result does not depend on how the input was chunked")
{
    // This is the property the streaming design exists for: a download is
    // hashed as it arrives, in whatever sizes the network hands over.
    const std::string input =
        "The quick brown fox jumps over the lazy dog. "
        "Pack my box with five dozen liquor jugs. "
        "How vexingly quick daft zebras jump!";
    const std::string expected = Sha256::hexOf(input);

    for (const std::size_t step : {std::size_t{1}, std::size_t{7}, std::size_t{63}, std::size_t{64},
                                   std::size_t{65}, std::size_t{127}}) {
        CAPTURE(step);
        Sha256 h;
        for (std::size_t off = 0; off < input.size(); off += step) {
            h.update(std::string_view(input).substr(off, step));
        }
        CHECK(h.finishHex() == expected);
    }
}

TEST_CASE("block boundaries are handled exactly")
{
    // 55/56/57 straddle the padding rule: at 56 bytes the length no longer fits
    // in the final block, so a second block must be emitted. This is the single
    // most common place a hand-written SHA-256 is wrong.
    // Every value below is generated, not recalled:
    //   python3 -c "import hashlib; print(hashlib.sha256(b'a'*57).hexdigest())"
    struct Case
    {
        std::size_t length;
        const char* expected;
    };

    const Case cases[] = {
        {55, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318"},
        {56, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a"},
        {57, "f13b2d724659eb3bf47f2dd6af1accc87b81f09f59f2b75e5c0bed6589dfe8c6"},
        {63, "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34"},
        {64, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb"},
        {65, "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.length);
        CHECK(Sha256::hexOf(std::string(c.length, 'a')) == c.expected);
    }
}

TEST_CASE("an instance can be reset and reused")
{
    Sha256 h;
    h.update("abc");
    CHECK(h.finishHex() == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    h.reset();
    h.update("");
    CHECK(h.finishHex() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_CASE("degenerate input is ignored rather than crashing")
{
    Sha256 h;
    h.update(nullptr, 0);
    h.update(nullptr, 99);  // null with a non-zero size must not dereference
    h.update("", 0);
    CHECK(h.finishHex() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_CASE("hex round-trips")
{
    const Sha256::Digest d = Sha256::of("abc");
    const std::string hex = toHex(d);
    CHECK(hex.size() == 64);

    Sha256::Digest parsed{};
    REQUIRE(fromHex(hex, parsed));
    CHECK(digestsEqual(d, parsed));
}

TEST_CASE("hex parsing rejects anything that is not 64 lowercase hex characters")
{
    Sha256::Digest out{};
    CHECK_FALSE(fromHex("", out));
    CHECK_FALSE(fromHex("abc", out));
    CHECK_FALSE(fromHex(std::string(63, 'a'), out));
    CHECK_FALSE(fromHex(std::string(65, 'a'), out));
    CHECK_FALSE(fromHex(std::string(64, 'g'), out));
    CHECK_FALSE(fromHex(std::string(64, ' '), out));

    // Uppercase is rejected on purpose: update.schema.json mandates lowercase,
    // so accepting both would let two spellings of one digest both "match" a
    // contract that specifies one.
    CHECK_FALSE(fromHex(std::string(64, 'A'), out));
    CHECK(fromHex(std::string(64, 'a'), out));
}

TEST_CASE("digest comparison detects a difference in any position")
{
    const Sha256::Digest a = Sha256::of("payload");
    CHECK(digestsEqual(a, a));

    for (std::size_t i = 0; i < Sha256::kDigestSize; ++i) {
        CAPTURE(i);
        Sha256::Digest b = a;
        b[i] = static_cast<std::uint8_t>(b[i] ^ 0x01U);
        CHECK_FALSE(digestsEqual(a, b));
    }
}

TEST_CASE("a single flipped bit changes the digest completely")
{
    // Not a correctness proof, but a wired-up-wrong implementation - a stuck
    // state variable, a dropped round - usually shows here first.
    const std::string base(4096, 'x');
    std::string altered = base;
    altered[2048] = 'y';

    const Sha256::Digest a = Sha256::of(base);
    const Sha256::Digest b = Sha256::of(altered);
    CHECK_FALSE(digestsEqual(a, b));

    int differingBytes = 0;
    for (std::size_t i = 0; i < Sha256::kDigestSize; ++i) {
        if (a[i] != b[i]) {
            ++differingBytes;
        }
    }
    CHECK(differingBytes > 24);
}

TEST_CASE("regression: verification must reject a truncated download")
{
    // The failure this primitive exists to catch. The predecessor checked a
    // four-byte PK\x03\x04 magic and nothing else (utils.cpp:27-39), so a
    // truncated or substituted archive passed straight through to extraction.
    const std::string full(100000, 'z');
    const std::string truncated = full.substr(0, 99999);
    CHECK_FALSE(digestsEqual(Sha256::of(full), Sha256::of(truncated)));
}
