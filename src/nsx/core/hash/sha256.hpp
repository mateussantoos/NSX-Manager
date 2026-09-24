// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace nsx::core {

/// @brief Streaming SHA-256.
///
/// @details Self-contained on purpose. mbedTLS is linked for TLS, but depending
///          on it here would drag a platform library into `core` and make the
///          integrity primitive untestable on a host - which is precisely the
///          code that most needs testing, since every downloaded byte is
///          checked against it before anything executes it.
///
///          Streaming rather than one-shot because a download is hashed
///          **while** it is written, in a single pass: the update path never
///          re-reads a file it just wrote, and never holds a whole NRO in
///          memory.
///
/// @see docs/architecture/update-pipeline.md, ADR-0006
/// @since 0.1.0
class Sha256
{
public:
    /// @brief Length of a SHA-256 digest in bytes.
    static constexpr std::size_t kDigestSize = 32;

    /// @brief A raw digest.
    using Digest = std::array<std::uint8_t, kDigestSize>;

    Sha256();

    /// @brief Feed more data.
    /// @param data Pointer to the bytes; may be null only when @p size is zero.
    /// @param size Number of bytes to consume.
    void update(const void* data, std::size_t size);

    /// @brief Feed more data.
    /// @param data The bytes to consume.
    void update(std::string_view data);

    /// @brief Finish and produce the digest.
    /// @return The 32-byte digest.
    /// @note After this the object must not be reused without @ref reset.
    [[nodiscard]] Digest finish();

    /// @brief Finish and produce the digest as lowercase hex.
    /// @return A 64-character lowercase hexadecimal string.
    [[nodiscard]] std::string finishHex();

    /// @brief Return to the initial state so the object can hash again.
    void reset();

    /// @brief Hash a buffer in one call.
    /// @param data The bytes to hash.
    /// @return The digest.
    [[nodiscard]] static Digest of(std::string_view data);

    /// @brief Hash a buffer in one call and render it as hex.
    /// @param data The bytes to hash.
    /// @return A 64-character lowercase hexadecimal string.
    [[nodiscard]] static std::string hexOf(std::string_view data);

private:
    void compress(const std::uint8_t block[64]);

    std::array<std::uint32_t, 8> m_state{};
    std::array<std::uint8_t, 64> m_buffer{};
    std::uint64_t m_bitCount{};
    std::size_t m_bufferUsed{};
    bool m_finished{};
};

/// @brief Render a digest as lowercase hexadecimal.
/// @param digest The digest to render.
/// @return A 64-character lowercase hexadecimal string.
[[nodiscard]] std::string toHex(const Sha256::Digest& digest);

/// @brief Parse a 64-character lowercase hexadecimal digest.
/// @param hex The text to parse.
/// @param out Receives the parsed digest when parsing succeeds.
/// @return True when @p hex was exactly 64 lowercase hex characters.
/// @note Uppercase is rejected deliberately: the manifest schema mandates
///       lowercase, so accepting both would let two spellings of the same
///       digest disagree with the published contract.
[[nodiscard]] bool fromHex(std::string_view hex, Sha256::Digest& out);

/// @brief Compare two digests without a data-dependent branch.
/// @param a First digest.
/// @param b Second digest.
/// @return True when the digests are identical.
/// @note Constant time with respect to the digest contents. An early-exit
///       comparison leaks, through timing, how many leading bytes matched -
///       which is enough to forge a digest byte by byte.
[[nodiscard]] bool digestsEqual(const Sha256::Digest& a, const Sha256::Digest& b);

}  // namespace nsx::core
