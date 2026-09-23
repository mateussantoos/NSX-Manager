// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/version/semver.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <ranges>
#include <vector>

namespace nsx::core {

namespace detail {
namespace {

constexpr bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

constexpr bool isIdentifierChar(char c)
{
    return isDigit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-';
}

/// Every character is a digit, and there is at least one.
bool isNumericIdentifier(std::string_view s)
{
    return !s.empty() && std::ranges::all_of(s, isDigit);
}

/// Parse one numeric component. Rejects leading zeros and 32-bit overflow
/// rather than wrapping or throwing.
Result<std::uint32_t, SemVerError> parseNumber(std::string_view text)
{
    if (text.empty()) {
        return Result<std::uint32_t, SemVerError>::err(SemVerError::NonNumericField);
    }
    if (!std::ranges::all_of(text, isDigit)) {
        return Result<std::uint32_t, SemVerError>::err(SemVerError::NonNumericField);
    }
    if (text.size() > 1 && text.front() == '0') {
        return Result<std::uint32_t, SemVerError>::err(SemVerError::LeadingZero);
    }

    std::uint64_t acc = 0;
    for (const char c : text) {
        acc = (acc * 10) + static_cast<std::uint64_t>(c - '0');
        if (acc > std::numeric_limits<std::uint32_t>::max()) {
            return Result<std::uint32_t, SemVerError>::err(SemVerError::NumericOverflow);
        }
    }
    return Result<std::uint32_t, SemVerError>::ok(static_cast<std::uint32_t>(acc));
}

/// Dot-separated identifiers, each non-empty and alphanumeric-or-hyphen.
bool isValidDotSeparated(std::string_view s)
{
    if (s.empty()) {
        return false;
    }
    std::size_t start = 0;
    while (true) {
        const std::size_t dot = s.find('.', start);
        const std::string_view part =
            s.substr(start, dot == std::string_view::npos ? std::string_view::npos : dot - start);
        if (part.empty() || !std::ranges::all_of(part, isIdentifierChar)) {
            return false;
        }
        if (dot == std::string_view::npos) {
            return true;
        }
        start = dot + 1;
    }
}

std::vector<std::string_view> split(std::string_view s, char sep)
{
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t at = s.find(sep, start);
        if (at == std::string_view::npos) {
            parts.push_back(s.substr(start));
            return parts;
        }
        parts.push_back(s.substr(start, at - start));
        start = at + 1;
    }
}

/// SemVer section 11 precedence for the prerelease field.
std::strong_ordering comparePrerelease(std::string_view a, std::string_view b)
{
    // An absent prerelease ranks ABOVE a present one: 1.0.0 > 1.0.0-rc.1.
    if (a.empty() && b.empty()) {
        return std::strong_ordering::equal;
    }
    if (a.empty()) {
        return std::strong_ordering::greater;
    }
    if (b.empty()) {
        return std::strong_ordering::less;
    }

    const std::vector<std::string_view> left = split(a, '.');
    const std::vector<std::string_view> right = split(b, '.');
    const std::size_t shared = std::min(left.size(), right.size());

    for (std::size_t i = 0; i < shared; ++i) {
        const bool leftNumeric = isNumericIdentifier(left[i]);
        const bool rightNumeric = isNumericIdentifier(right[i]);

        if (leftNumeric && rightNumeric) {
            // Compare by value, not lexically: rc.9 < rc.10.
            if (left[i].size() != right[i].size()) {
                return left[i].size() <=> right[i].size();
            }
            if (const auto c = left[i] <=> right[i]; c != 0) {
                return c;
            }
        }
        else if (leftNumeric != rightNumeric) {
            // Numeric identifiers always rank below alphanumeric ones.
            return leftNumeric ? std::strong_ordering::less : std::strong_ordering::greater;
        }
        else if (const auto c = left[i] <=> right[i]; c != 0) {
            return c;
        }
    }

    // All shared identifiers equal: more identifiers wins.
    return left.size() <=> right.size();
}

}  // namespace
}  // namespace detail

std::string SemVer::toString() const
{
    std::string out =
        std::to_string(major) + '.' + std::to_string(minor) + '.' + std::to_string(patch);
    if (!prerelease.empty()) {
        out += '-' + prerelease;
    }
    if (!build.empty()) {
        out += '+' + build;
    }
    return out;
}

Result<SemVer, SemVerError> parseSemVer(std::string_view text)
{
    using R = Result<SemVer, SemVerError>;

    if (!text.empty() && (text.front() == 'v' || text.front() == 'V')) {
        text.remove_prefix(1);
    }
    if (text.empty()) {
        return R::err(SemVerError::Empty);
    }

    SemVer out;

    // Build metadata first: it may itself contain hyphens, so splitting on '+'
    // before '-' avoids mistaking part of it for a prerelease.
    if (const std::size_t plus = text.find('+'); plus != std::string_view::npos) {
        const std::string_view build = text.substr(plus + 1);
        if (!detail::isValidDotSeparated(build)) {
            return R::err(SemVerError::InvalidBuild);
        }
        out.build = std::string(build);
        text = text.substr(0, plus);
    }

    if (const std::size_t dash = text.find('-'); dash != std::string_view::npos) {
        const std::string_view pre = text.substr(dash + 1);
        if (!detail::isValidDotSeparated(pre)) {
            return R::err(SemVerError::InvalidPrerelease);
        }
        out.prerelease = std::string(pre);
        text = text.substr(0, dash);
    }

    const std::vector<std::string_view> parts = detail::split(text, '.');
    if (parts.size() < 2) {
        return R::err(SemVerError::MissingMinor);
    }
    if (parts.size() < 3) {
        return R::err(SemVerError::MissingPatch);
    }
    if (parts.size() > 3) {
        return R::err(SemVerError::TrailingGarbage);
    }

    const std::array<std::uint32_t*, 3> fields = {&out.major, &out.minor, &out.patch};
    for (std::size_t i = 0; i < 3; ++i) {
        const Result<std::uint32_t, SemVerError> n = detail::parseNumber(parts[i]);
        if (!n) {
            return R::err(n.error());
        }
        *fields[i] = n.value();
    }

    return R::ok(std::move(out));
}

std::optional<SemVer> tryParseSemVer(std::string_view text)
{
    // Strict first: the overwhelmingly common case, and it costs one pass.
    if (const Result<SemVer, SemVerError> strict = parseSemVer(text); strict) {
        return strict.value();
    }

    if (!text.empty() && (text.front() == 'v' || text.front() == 'V')) {
        text.remove_prefix(1);
    }

    // Take the leading `N(.N)*` run and ignore whatever follows. This is what
    // turns an upstream tag like "4.2.1 (2026-09-23)" into something orderable
    // instead of an uncaught exception.
    std::size_t end = 0;
    while (end < text.size() && (detail::isDigit(text[end]) || text[end] == '.')) {
        ++end;
    }
    std::string_view head = text.substr(0, end);
    while (!head.empty() && head.back() == '.') {
        head.remove_suffix(1);
    }
    if (head.empty() || !detail::isDigit(head.front())) {
        return std::nullopt;
    }

    SemVer out;
    const std::array<std::uint32_t*, 3> fields = {&out.major, &out.minor, &out.patch};
    const std::vector<std::string_view> parts = detail::split(head, '.');

    for (std::size_t i = 0; i < parts.size() && i < 3; ++i) {
        if (parts[i].empty()) {
            return std::nullopt;
        }
        // Leading zeros are tolerated here; strictness is parseSemVer's job.
        std::uint64_t acc = 0;
        for (const char c : parts[i]) {
            if (!detail::isDigit(c)) {
                return std::nullopt;
            }
            acc = (acc * 10) + static_cast<std::uint64_t>(c - '0');
            if (acc > std::numeric_limits<std::uint32_t>::max()) {
                return std::nullopt;
            }
        }
        *fields[i] = static_cast<std::uint32_t>(acc);
    }

    return out;
}

std::strong_ordering compare(const SemVer& a, const SemVer& b)
{
    if (const auto c = a.major <=> b.major; c != 0) {
        return c;
    }
    if (const auto c = a.minor <=> b.minor; c != 0) {
        return c;
    }
    if (const auto c = a.patch <=> b.patch; c != 0) {
        return c;
    }
    // Build metadata is explicitly excluded from precedence (SemVer section 10).
    return detail::comparePrerelease(a.prerelease, b.prerelease);
}

bool isNewerThan(const SemVer& candidate, const SemVer& current)
{
    return compare(candidate, current) > 0;
}

std::string_view describe(SemVerError error)
{
    switch (error) {
        case SemVerError::Empty:
            return "version string is empty";
        case SemVerError::MissingMinor:
            return "missing minor component";
        case SemVerError::MissingPatch:
            return "missing patch component";
        case SemVerError::NonNumericField:
            return "a version component is not a number";
        case SemVerError::LeadingZero:
            return "a version component has a leading zero";
        case SemVerError::NumericOverflow:
            return "a version component is too large";
        case SemVerError::InvalidPrerelease:
            return "malformed prerelease identifier";
        case SemVerError::InvalidBuild:
            return "malformed build metadata";
        case SemVerError::TrailingGarbage:
            return "unexpected trailing characters";
    }
    return "unknown error";
}

}  // namespace nsx::core
