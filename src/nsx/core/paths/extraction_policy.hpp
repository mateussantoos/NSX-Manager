// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>
#include <string_view>

#include "nsx/core/paths/preserve_rules.hpp"
#include "nsx/core/paths/traversal_guard.hpp"

namespace nsx::core {

/// @brief What an extractor should do with one archive entry.
/// @since 0.3.0
enum class EntryAction
{
    Write,            ///< Write the file at @ref EntryDecision::destination.
    CreateDirectory,  ///< Create the directory and continue.
    SkipPreserved,    ///< The user asked to keep what is already there.
    Reject            ///< Refuse the entry, and the archive with it.
};

/// @brief The decision for one entry, with everything the caller needs.
/// @since 0.3.0
struct EntryDecision
{
    EntryAction action{EntryAction::Reject};  ///< What to do.
    std::string destination;                  ///< Absolute path; empty unless writing.
    PathError error{PathError::Empty};        ///< Why, when rejected.
    std::string reason;                       ///< One line for the log and the UI.

    /// @brief Whether this entry produces a write.
    /// @return True for a file or a directory.
    [[nodiscard]] bool writes() const
    {
        return action == EntryAction::Write || action == EntryAction::CreateDirectory;
    }
};

/// @brief Decides, per entry, where it goes or why it does not.
///
/// @details The security-critical half of extraction, kept **pure** so it can
///          be tested exhaustively without a filesystem or an archive. The
///          device-side extractor becomes a loop that reads an entry name,
///          asks this, and either writes bytes or does not - it holds no policy
///          of its own and therefore has nothing to get wrong.
///
///          Order matters and is part of the contract:
///
///          1. the traversal guard, which can reject;
///          2. the preserve rules, which can skip.
///
///          Traversal first, always. A preserved path is compared as text, and
///          comparing `../../bootloader/hekate_ipl.ini` against a rule would be
///          asking whether a path that escapes the destination is protected
///          inside it - a question with no correct answer. Rejecting first
///          means the comparison only ever runs on names already known to stay
///          put.
///
/// @see docs/architecture/threat-model.md, ADR-0011
/// @since 0.3.0
class ExtractionPolicy
{
public:
    /// @brief Construct a policy for one destination.
    /// @param root Absolute destination directory.
    /// @param preserve Rules describing what must survive.
    ExtractionPolicy(std::string root, PreserveRules preserve);

    /// @brief Decide what to do with an archive entry.
    /// @param entryName The name exactly as the archive carries it.
    /// @return The action, and the destination or the reason.
    [[nodiscard]] EntryDecision decide(std::string_view entryName) const;

    /// @brief The destination this policy extracts into.
    /// @return The root directory.
    [[nodiscard]] const std::string& root() const { return m_root; }

    /// @brief The preserve rules in force.
    /// @return The rules.
    [[nodiscard]] const PreserveRules& preserveRules() const { return m_preserve; }

private:
    std::string m_root;
    PreserveRules m_preserve;
};

/// @brief A short English description of an entry action.
/// @param action The action to describe.
/// @return A description suitable for a log line.
/// @since 0.3.0
[[nodiscard]] std::string_view describe(EntryAction action);

}  // namespace nsx::core
