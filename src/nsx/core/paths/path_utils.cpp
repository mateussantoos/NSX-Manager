// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/paths/path_utils.hpp"

#include <algorithm>
#include <cctype>

namespace nsx::core::paths {

std::string normalize(std::string_view path)
{
    if (path.empty()) {
        return {};
    }

    std::string out;
    out.reserve(path.size());

    // Replace backslashes with forward slashes and collapse duplicates
    bool lastWasSlash = false;
    for (char c : path) {
        char ch = (c == '\\') ? '/' : c;
        if (ch == '/') {
            if (!lastWasSlash) {
                out.push_back(ch);
                lastWasSlash = true;
            }
        }
        else {
            out.push_back(ch);
            lastWasSlash = false;
        }
    }

    // Strip trailing slash unless it's just "/" or "sdmc:/"
    if (out.size() > 1 && out.back() == '/' && out != "/" && out != "sdmc:/") {
        out.pop_back();
    }

    return out;
}

bool hasExtension(std::string_view path, std::string_view extension) noexcept
{
    if (extension.empty() || path.size() < extension.size()) {
        return false;
    }

    if (extension.front() != '.') {
        if (path.size() <= extension.size() || path[path.size() - extension.size() - 1] != '.') {
            return false;
        }
    }

    const std::string_view suffix = path.substr(path.size() - extension.size());
    return std::equal(suffix.begin(), suffix.end(), extension.begin(), extension.end(),
                      [](char a, char b) {
                          return std::tolower(static_cast<unsigned char>(a)) ==
                                 std::tolower(static_cast<unsigned char>(b));
                      });
}

std::string join(std::string_view base, std::string_view relative)
{
    if (base.empty()) {
        return normalize(relative);
    }
    if (relative.empty()) {
        return normalize(base);
    }

    std::string normalizedBase = normalize(base);
    std::string normalizedRel = normalize(relative);

    if (normalizedBase.back() == '/') {
        if (normalizedRel.front() == '/') {
            normalizedRel.erase(0, 1);
        }
        return normalizedBase + normalizedRel;
    }

    if (normalizedRel.front() == '/') {
        return normalizedBase + normalizedRel;
    }

    return normalizedBase + "/" + normalizedRel;
}

}  // namespace nsx::core::paths
