// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "nsx/core/result/result.hpp"

namespace nsx::core {

/// @brief Why a version string was rejected.
/// @since 0.1.0
enum class SemVerError
{
    Empty,              ///< Nothing to parse.
    MissingMinor,       ///< Only a major component was present.
    MissingPatch,       ///< Only major and minor were present.
    NonNumericField,    ///< A numeric field contained something else.
    LeadingZero,        ///< `01.0.0` - forbidden by SemVer section 2.
    NumericOverflow,    ///< A field exceeded 32 bits.
    InvalidPrerelease,  ///< An empty or non-alphanumeric prerelease identifier.
    InvalidBuild,       ///< An empty or non-alphanumeric build identifier.
    TrailingGarbage     ///< Unparsed characters remained.
};

/// @brief A parsed [Semantic Version 2.0.0](https://semver.org/spec/v2.0.0.html).
/// @since 0.1.0
struct SemVer
{
    std::uint32_t major{};   ///< Major component.
    std::uint32_t minor{};   ///< Minor component.
    std::uint32_t patch{};   ///< Patch component.
    std::string prerelease;  ///< Without its leading `-`; empty when absent.
    std::string build;       ///< Without its leading `+`; ignored in comparison.

    /// @brief Render back to a string.
    /// @return The version, never with a leading `v`.
    [[nodiscard]] std::string toString() const;
};

/// @brief Parse a strict SemVer 2.0.0 string.
///
/// @details Accepts one optional leading `v` or `V`, because release tags carry
///          it. Everything else must conform: three numeric components, no
///          leading zeros, no trailing text.
///
///          **Never throws.** The predecessor's comparison called `std::stoi`
///          on a digit-stripped tag and terminated the application with an
///          uncaught `std::out_of_range` whenever a tag carried a date
///          (`main_frame.cpp:53-73`). This function rejects rather than
///          guessing, and rejects rather than throwing.
///
/// @param text The version text.
/// @return The parsed version, or the reason it was rejected.
/// @since 0.1.0
[[nodiscard]] Result<SemVer, SemVerError> parseSemVer(std::string_view text);

/// @brief Parse leniently, for version strings we do not control.
///
/// @details Upstream projects tag things like `4.2.1-nsx`, `v1.2` or `2026.09`.
///          A missing minor or patch defaults to zero and trailing text is
///          ignored, so a usable ordering can still be derived. Use
///          @ref parseSemVer for our own versions, where strictness is correct.
///
/// @param text The version text.
/// @return The parsed version, or `std::nullopt` when nothing usable was found.
/// @since 0.1.0
[[nodiscard]] std::optional<SemVer> tryParseSemVer(std::string_view text);

/// @brief Order two versions by SemVer precedence.
///
/// @details Implements SemVer section 11: numeric components first, then
///          prerelease precedence, where an absent prerelease ranks **above** a
///          present one, and identifiers compare numerically when both are
///          numeric and lexically otherwise. Build metadata is ignored.
///
/// @param a Left operand.
/// @param b Right operand.
/// @return The ordering of @p a relative to @p b.
/// @since 0.1.0
[[nodiscard]] std::strong_ordering compare(const SemVer& a, const SemVer& b);

/// @brief Whether @p candidate is a genuine upgrade over @p current.
/// @param candidate The version being offered.
/// @param current The installed version.
/// @return True only when @p candidate orders strictly above @p current.
/// @since 0.1.0
[[nodiscard]] bool isNewerThan(const SemVer& candidate, const SemVer& current);

/// @brief A human-readable reason for a parse failure.
/// @param error The error to describe.
/// @return A short English description, suitable for a log line.
/// @since 0.1.0
[[nodiscard]] std::string_view describe(SemVerError error);

}  // namespace nsx::core
