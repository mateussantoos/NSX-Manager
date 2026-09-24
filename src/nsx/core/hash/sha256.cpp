// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/hash/sha256.hpp"

#include <cstring>

namespace nsx::core {

namespace detail {
namespace {

// FIPS 180-4 section 4.2.2: the first 32 bits of the fractional parts of the
// cube roots of the first 64 primes.
constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U};

// FIPS 180-4 section 5.3.3: fractional parts of the square roots of the first
// eight primes.
constexpr std::array<std::uint32_t, 8> kInitialState = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U,
                                                        0xa54ff53aU, 0x510e527fU, 0x9b05688cU,
                                                        0x1f83d9abU, 0x5be0cd19U};

constexpr std::uint32_t rotr(std::uint32_t x, unsigned n)
{
    return (x >> n) | (x << (32U - n));
}

constexpr std::uint32_t ch(std::uint32_t x, std::uint32_t y, std::uint32_t z)
{
    return (x & y) ^ (~x & z);
}

constexpr std::uint32_t maj(std::uint32_t x, std::uint32_t y, std::uint32_t z)
{
    return (x & y) ^ (x & z) ^ (y & z);
}

constexpr std::uint32_t bigSigma0(std::uint32_t x)
{
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}

constexpr std::uint32_t bigSigma1(std::uint32_t x)
{
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}

constexpr std::uint32_t smallSigma0(std::uint32_t x)
{
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3U);
}

constexpr std::uint32_t smallSigma1(std::uint32_t x)
{
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10U);
}

/// Read big-endian, byte by byte: the algorithm is defined that way, and doing
/// it explicitly means the result does not depend on the host's endianness.
constexpr std::uint32_t loadBigEndian(const std::uint8_t* p)
{
    return (static_cast<std::uint32_t>(p[0]) << 24U) | (static_cast<std::uint32_t>(p[1]) << 16U) |
           (static_cast<std::uint32_t>(p[2]) << 8U) | static_cast<std::uint32_t>(p[3]);
}

constexpr char kHexDigits[] = "0123456789abcdef";

}  // namespace
}  // namespace detail

Sha256::Sha256()
{
    reset();
}

void Sha256::reset()
{
    m_state = detail::kInitialState;
    m_buffer.fill(0);
    m_bitCount = 0;
    m_bufferUsed = 0;
    m_finished = false;
}

void Sha256::compress(const std::uint8_t block[64])
{
    std::array<std::uint32_t, 64> w{};

    for (std::size_t i = 0; i < 16; ++i) {
        w[i] = detail::loadBigEndian(block + (i * 4));
    }
    for (std::size_t i = 16; i < 64; ++i) {
        w[i] =
            detail::smallSigma1(w[i - 2]) + w[i - 7] + detail::smallSigma0(w[i - 15]) + w[i - 16];
    }

    std::uint32_t a = m_state[0];
    std::uint32_t b = m_state[1];
    std::uint32_t c = m_state[2];
    std::uint32_t d = m_state[3];
    std::uint32_t e = m_state[4];
    std::uint32_t f = m_state[5];
    std::uint32_t g = m_state[6];
    std::uint32_t h = m_state[7];

    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t t1 =
            h + detail::bigSigma1(e) + detail::ch(e, f, g) + detail::kRoundConstants[i] + w[i];
        const std::uint32_t t2 = detail::bigSigma0(a) + detail::maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    m_state[0] += a;
    m_state[1] += b;
    m_state[2] += c;
    m_state[3] += d;
    m_state[4] += e;
    m_state[5] += f;
    m_state[6] += g;
    m_state[7] += h;
}

void Sha256::update(const void* data, std::size_t size)
{
    if (m_finished || size == 0 || data == nullptr) {
        return;
    }

    const auto* p = static_cast<const std::uint8_t*>(data);
    m_bitCount += static_cast<std::uint64_t>(size) * 8U;

    // Top up a partially filled block first, then consume whole blocks straight
    // from the caller's buffer, then keep whatever tail remains. This is what
    // makes chunk boundaries irrelevant to the result - the reason the tests
    // deliberately feed the same input in awkward splits.
    if (m_bufferUsed > 0) {
        const std::size_t need = 64 - m_bufferUsed;
        const std::size_t take = (size < need) ? size : need;
        std::memcpy(m_buffer.data() + m_bufferUsed, p, take);
        m_bufferUsed += take;
        p += take;
        size -= take;

        if (m_bufferUsed == 64) {
            compress(m_buffer.data());
            m_bufferUsed = 0;
        }
    }

    while (size >= 64) {
        compress(p);
        p += 64;
        size -= 64;
    }

    if (size > 0) {
        std::memcpy(m_buffer.data(), p, size);
        m_bufferUsed = size;
    }
}

void Sha256::update(std::string_view data)
{
    update(data.data(), data.size());
}

Sha256::Digest Sha256::finish()
{
    if (!m_finished) {
        const std::uint64_t bits = m_bitCount;

        // FIPS 180-4 section 5.1.1: append 0x80, pad with zeros until 56 bytes
        // mod 64, then the length as a big-endian 64-bit count of BITS.
        const std::uint8_t one = 0x80;
        update(&one, 1);
        m_bitCount = bits;  // padding must not count toward the length

        const std::uint8_t zero = 0x00;
        while (m_bufferUsed != 56) {
            update(&zero, 1);
            m_bitCount = bits;
        }

        std::array<std::uint8_t, 8> lengthBytes{};
        for (std::size_t i = 0; i < 8; ++i) {
            lengthBytes[7 - i] = static_cast<std::uint8_t>((bits >> (i * 8U)) & 0xFFU);
        }
        std::memcpy(m_buffer.data() + 56, lengthBytes.data(), 8);
        compress(m_buffer.data());
        m_bufferUsed = 0;
        m_finished = true;
    }

    Digest out{};
    for (std::size_t i = 0; i < 8; ++i) {
        out[(i * 4) + 0] = static_cast<std::uint8_t>((m_state[i] >> 24U) & 0xFFU);
        out[(i * 4) + 1] = static_cast<std::uint8_t>((m_state[i] >> 16U) & 0xFFU);
        out[(i * 4) + 2] = static_cast<std::uint8_t>((m_state[i] >> 8U) & 0xFFU);
        out[(i * 4) + 3] = static_cast<std::uint8_t>(m_state[i] & 0xFFU);
    }
    return out;
}

std::string Sha256::finishHex()
{
    return toHex(finish());
}

Sha256::Digest Sha256::of(std::string_view data)
{
    Sha256 h;
    h.update(data);
    return h.finish();
}

std::string Sha256::hexOf(std::string_view data)
{
    return toHex(of(data));
}

std::string toHex(const Sha256::Digest& digest)
{
    std::string out;
    out.reserve(Sha256::kDigestSize * 2);
    for (const std::uint8_t byte : digest) {
        out.push_back(detail::kHexDigits[(byte >> 4U) & 0x0FU]);
        out.push_back(detail::kHexDigits[byte & 0x0FU]);
    }
    return out;
}

bool fromHex(std::string_view hex, Sha256::Digest& out)
{
    if (hex.size() != Sha256::kDigestSize * 2) {
        return false;
    }

    for (std::size_t i = 0; i < Sha256::kDigestSize; ++i) {
        std::uint8_t value = 0;
        for (std::size_t nibble = 0; nibble < 2; ++nibble) {
            const char c = hex[(i * 2) + nibble];
            std::uint8_t digit = 0;
            if (c >= '0' && c <= '9') {
                digit = static_cast<std::uint8_t>(c - '0');
            }
            else if (c >= 'a' && c <= 'f') {
                digit = static_cast<std::uint8_t>((c - 'a') + 10);
            }
            else {
                // Uppercase included: the manifest schema mandates lowercase,
                // and accepting both would let two spellings of one digest
                // disagree with the published contract.
                return false;
            }
            value = static_cast<std::uint8_t>((value << 4U) | digit);
        }
        out[i] = value;
    }
    return true;
}

bool digestsEqual(const Sha256::Digest& a, const Sha256::Digest& b)
{
    // OR the differences together and test once. An early return would leak,
    // through timing, how many leading bytes matched - enough to forge a digest
    // one byte at a time.
    std::uint8_t diff = 0;
    for (std::size_t i = 0; i < Sha256::kDigestSize; ++i) {
        diff = static_cast<std::uint8_t>(diff | (a[i] ^ b[i]));
    }
    return diff == 0;
}

}  // namespace nsx::core
