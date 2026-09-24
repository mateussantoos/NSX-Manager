// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace nsx::core {

/// @brief What a user asked to keep across a CFW pack install.
///
/// @details Parsed from `/config/nsx-manager/preserve.txt`, one pattern a line.
///          A CFW pack is an archive that overwrites large parts of the SD
///          card, and some of what it overwrites is the user's: `hekate_ipl.ini`
///          with their boot entries, their `emummc.ini`, their overlays, their
///          themes. Losing those to an update is the single most common
///          complaint about tools of this kind.
///
///          Syntax, kept deliberately small because a config file nobody can
///          predict is worse than one that cannot express everything:
///
///          - `#` begins a comment; blank lines are ignored.
///          - A trailing `/` means a directory and everything under it.
///          - `*` matches within one path component.
///          - `**` matches across separators.
///          - A leading `!` **negates**, re-allowing something an earlier
///            pattern preserved. Last match wins, so order is meaningful.
///          - Paths are relative to the SD root and use `/`.
///
///          Matching is **case-insensitive**, because the file system is. A
///          user who writes `Bootloader/hekate_ipl.ini` means the same file as
///          `bootloader/hekate_ipl.ini`, and on FAT it *is* the same file - a
///          case-sensitive comparison would silently fail to preserve it.
///
/// @see docs/reference/sd-layout.md
/// @since 0.3.0
class PreserveRules
{
public:
    /// @brief A single parsed line.
    /// @since 0.3.0
    struct Rule
    {
        std::string pattern;  ///< The pattern, without `!` and normalised.
        bool negated{};       ///< Re-allows overwriting rather than preserving.
        bool directory{};     ///< Written with a trailing `/`.
        int line{};           ///< 1-based source line, for diagnostics.
    };

    /// @brief Parse the contents of a `preserve.txt`.
    ///
    /// @details **Never fails.** An unparseable line is dropped and reported
    ///          through @ref warnings rather than rejecting the file: refusing
    ///          to install a pack because one line of a user's config has a
    ///          typo is worse than ignoring that line, and the safe direction
    ///          for a dropped rule is that something gets overwritten which the
    ///          user can restore, not that the install does not happen.
    ///
    /// @param text The file contents.
    /// @return The parsed rules.
    /// @since 0.3.0
    [[nodiscard]] static PreserveRules parse(std::string_view text);

    /// @brief The defaults applied when the user has no `preserve.txt`.
    /// @details Deliberately short: the boot configuration and the emuMMC
    ///          pointer. Everything else is the pack's to replace.
    /// @return The built-in rule set.
    /// @since 0.3.0
    [[nodiscard]] static PreserveRules defaults();

    /// @brief Whether a path must survive the install.
    /// @param path Path relative to the destination root, using `/`.
    /// @return True when an existing file at @p path must not be overwritten.
    /// @details Last match wins, so a later `!` re-allows an earlier rule.
    [[nodiscard]] bool preserves(std::string_view path) const;

    /// @brief The parsed rules, in file order.
    /// @return The rules.
    [[nodiscard]] const std::vector<Rule>& rules() const { return m_rules; }

    /// @brief Lines that could not be understood, as human-readable text.
    /// @return One message per dropped line; empty when everything parsed.
    [[nodiscard]] const std::vector<std::string>& warnings() const { return m_warnings; }

    /// @brief Whether any rule was parsed.
    /// @return True when there is nothing to preserve.
    [[nodiscard]] bool empty() const { return m_rules.empty(); }

private:
    std::vector<Rule> m_rules;
    std::vector<std::string> m_warnings;
};

/// @brief Match a path against one glob pattern.
/// @param pattern The pattern; supports `*`, `**` and `?`.
/// @param path The path to test.
/// @return True on a match.
/// @details Case-insensitive for ASCII, matching the file system. Exposed
///          separately so the matcher can be tested without building a rule set.
/// @since 0.3.0
[[nodiscard]] bool globMatch(std::string_view pattern, std::string_view path);

}  // namespace nsx::core
