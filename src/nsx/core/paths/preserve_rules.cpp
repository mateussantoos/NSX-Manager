// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/paths/preserve_rules.hpp"

namespace nsx::core {

namespace detail {
namespace {

char lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

std::string_view trim(std::string_view text)
{
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    return text;
}

/// Drop a leading `/` and collapse `//`, so `/bootloader//x` and
/// `bootloader/x` are the same rule. Patterns are always relative to the
/// destination root.
std::string normalise(std::string_view pattern)
{
    std::string out;
    out.reserve(pattern.size());

    bool lastWasSlash = true;  // strips a leading separator too
    for (const char c : pattern) {
        if (c == '/') {
            if (lastWasSlash) {
                continue;
            }
            lastWasSlash = true;
        }
        else {
            lastWasSlash = false;
        }
        out.push_back(c);
    }
    return out;
}

/// Split on '/', dropping empty pieces. Patterns and paths both arrive
/// normalised, so an empty piece can only come from a trailing separator.
std::vector<std::string_view> components(std::string_view path)
{
    std::vector<std::string_view> out;
    std::string_view::size_type start = 0;

    while (start <= path.size()) {
        const std::string_view::size_type slash = path.find('/', start);
        const std::string_view::size_type stop =
            slash == std::string_view::npos ? path.size() : slash;
        if (stop > start) {
            out.push_back(path.substr(start, stop - start));
        }
        if (slash == std::string_view::npos) {
            break;
        }
        start = slash + 1;
    }
    return out;
}

/// Glob one component against one component. Neither contains a separator, so
/// `*` and `?` cannot cross one and a single backtrack point is sufficient.
bool matchComponent(std::string_view pattern, std::string_view text)
{
    std::size_t p = 0;
    std::size_t s = 0;
    std::size_t star = std::string_view::npos;
    std::size_t starText = 0;

    while (s < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || lower(pattern[p]) == lower(text[s]))) {
            ++p;
            ++s;
        }
        else if (p < pattern.size() && pattern[p] == '*') {
            star = p;
            ++p;
            starText = s;
        }
        else if (star != std::string_view::npos) {
            p = star + 1;
            ++starText;
            s = starText;
        }
        else {
            return false;
        }
    }

    while (p < pattern.size() && pattern[p] == '*') {
        ++p;
    }
    return p == pattern.size();
}

/// Match component lists, where a component of exactly `**` absorbs zero or
/// more path components.
///
/// A dynamic program rather than recursion. The obvious recursive formulation
/// tries every split point at every `**` and goes exponential on a pattern like
/// `**/**/**/**/x` - which is user input, from a file on the SD card. This is
/// O(pattern x path) whatever the pattern looks like.
///
/// Only a component that is EXACTLY `**` crosses separators. `a**b` is treated
/// as `a*b` within its own component, which is the gitignore convention and the
/// one users expect.
bool matchAll(const std::vector<std::string_view>& pattern,
              const std::vector<std::string_view>& path)
{
    // reachable[j] - can the pattern consumed so far account for path[0..j)?
    std::vector<char> reachable(path.size() + 1, 0);
    reachable[0] = 1;

    for (const std::string_view component : pattern) {
        std::vector<char> next(path.size() + 1, 0);

        if (component == "**") {
            // Zero or more: once reachable, every longer prefix is too.
            char carry = 0;
            for (std::size_t j = 0; j <= path.size(); ++j) {
                carry = static_cast<char>(carry || reachable[j]);
                next[j] = carry;
            }
        }
        else {
            for (std::size_t j = 1; j <= path.size(); ++j) {
                if (reachable[j - 1] && matchComponent(component, path[j - 1])) {
                    next[j] = 1;
                }
            }
        }

        reachable.swap(next);
    }

    return reachable[path.size()] != 0;
}

bool match(std::string_view pattern, std::string_view path)
{
    return matchAll(components(pattern), components(path));
}

}  // namespace
}  // namespace detail

bool globMatch(std::string_view pattern, std::string_view path)
{
    return detail::match(pattern, path);
}

PreserveRules PreserveRules::parse(std::string_view text)
{
    PreserveRules out;

    int lineNumber = 0;
    std::string_view rest = text;

    while (!rest.empty() || lineNumber == 0) {
        const std::string_view::size_type newline = rest.find('\n');
        std::string_view line = newline == std::string_view::npos ? rest : rest.substr(0, newline);
        const bool last = newline == std::string_view::npos;
        rest = last ? std::string_view{} : rest.substr(newline + 1);
        ++lineNumber;

        line = detail::trim(line);

        if (line.empty() || line.front() == '#') {
            if (last) {
                break;
            }
            continue;
        }

        Rule rule;
        rule.line = lineNumber;

        if (line.front() == '!') {
            rule.negated = true;
            line.remove_prefix(1);
            line = detail::trim(line);
        }

        if (line.empty()) {
            out.m_warnings.push_back("line " + std::to_string(lineNumber) +
                                     ": '!' with no pattern - ignored");
            if (last) {
                break;
            }
            continue;
        }

        if (line.back() == '/') {
            rule.directory = true;
            line.remove_suffix(1);
        }

        rule.pattern = detail::normalise(line);

        if (rule.pattern.empty()) {
            out.m_warnings.push_back("line " + std::to_string(lineNumber) +
                                     ": empty pattern - ignored");
        }
        else if (rule.pattern.find("..") != std::string::npos) {
            // A preserve rule names something to keep, never something to
            // reach. `..` here would only ever be a mistake or an attempt to
            // describe a path outside the install root.
            out.m_warnings.push_back("line " + std::to_string(lineNumber) +
                                     ": '..' is not allowed in a preserve pattern - ignored");
        }
        else {
            out.m_rules.push_back(std::move(rule));
        }

        if (last) {
            break;
        }
    }

    return out;
}

PreserveRules PreserveRules::defaults()
{
    // Short on purpose. Everything a pack ships is the pack's to replace; these
    // three are the user's, and losing them turns an update into a device that
    // boots differently than it did before.
    return parse(
        "# Defaults applied when no preserve.txt exists.\n"
        "bootloader/hekate_ipl.ini\n"
        "bootloader/ini/\n"
        "emuMMC/emummc.ini\n");
}

bool PreserveRules::preserves(std::string_view path) const
{
    const std::string normalised = detail::normalise(path);

    bool preserved = false;

    // Last match wins, so a later `!` re-allows an earlier rule. Evaluated in
    // file order rather than by specificity: order is something a user can see
    // and reason about, specificity is something they have to guess at.
    for (const Rule& rule : m_rules) {
        bool hit = detail::match(rule.pattern, normalised);

        if (!hit && rule.directory) {
            // `bootloader/ini/` covers everything beneath it.
            hit = detail::match(rule.pattern + "/**", normalised);
        }
        if (!hit && !rule.directory) {
            // A bare directory name still covers its contents, so a user who
            // writes `bootloader/ini` without the slash gets what they meant.
            hit = detail::match(rule.pattern + "/**", normalised);
        }

        if (hit) {
            preserved = !rule.negated;
        }
    }

    return preserved;
}

}  // namespace nsx::core
