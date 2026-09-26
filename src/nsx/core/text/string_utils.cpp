// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/text/string_utils.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <numeric>

namespace nsx::core::text {

std::string formatBytes(std::uint64_t bytes)
{
    constexpr std::uint64_t kKiB = 1024ULL;
    constexpr std::uint64_t kMiB = kKiB * 1024ULL;
    constexpr std::uint64_t kGiB = kMiB * 1024ULL;
    constexpr std::uint64_t kTiB = kGiB * 1024ULL;

    std::array<char, 64> buf{};
    if (bytes >= kTiB) {
        (void)std::snprintf(buf.data(), buf.size(), "%.2f TB",
                            static_cast<double>(bytes) / static_cast<double>(kTiB));
    }
    else if (bytes >= kGiB) {
        (void)std::snprintf(buf.data(), buf.size(), "%.2f GB",
                            static_cast<double>(bytes) / static_cast<double>(kGiB));
    }
    else if (bytes >= kMiB) {
        (void)std::snprintf(buf.data(), buf.size(), "%.2f MB",
                            static_cast<double>(bytes) / static_cast<double>(kMiB));
    }
    else if (bytes >= kKiB) {
        (void)std::snprintf(buf.data(), buf.size(), "%.2f KB",
                            static_cast<double>(bytes) / static_cast<double>(kKiB));
    }
    else {
        (void)std::snprintf(buf.data(), buf.size(), "%llu B",
                            static_cast<unsigned long long>(bytes));
    }
    return {buf.data()};
}

std::string_view trimLeft(std::string_view str) noexcept
{
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front())) != 0) {
        str.remove_prefix(1);
    }
    return str;
}

std::string_view trimRight(std::string_view str) noexcept
{
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back())) != 0) {
        str.remove_suffix(1);
    }
    return str;
}

std::string_view trim(std::string_view str) noexcept
{
    return trimRight(trimLeft(str));
}

std::string sanitize(std::string_view str)
{
    std::string out;
    out.reserve(str.size());
    for (const char c : str) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc >= 32 && uc <= 126) {
            out.push_back(c);
        }
    }
    return out;
}

std::vector<std::string> split(std::string_view str, char delimiter)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start < str.size()) {
        const std::size_t pos = str.find(delimiter, start);
        if (pos == std::string_view::npos) {
            parts.emplace_back(str.substr(start));
            break;
        }
        parts.emplace_back(str.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

std::string join(const std::vector<std::string>& elements, std::string_view delimiter)
{
    if (elements.empty()) {
        return {};
    }
    std::string out = elements.front();
    for (std::size_t i = 1; i < elements.size(); ++i) {
        out.append(delimiter);
        out.append(elements[i]);
    }
    return out;
}

}  // namespace nsx::core::text
