// SPDX-License-Identifier: GPL-3.0-only

#include "nsx/core/paths/extraction_policy.hpp"

#include <utility>

namespace nsx::core {

ExtractionPolicy::ExtractionPolicy(std::string root, PreserveRules preserve)
    : m_root(std::move(root)), m_preserve(std::move(preserve))
{
}

std::string_view describe(EntryAction action)
{
    switch (action) {
        case EntryAction::Write:
            return "write";
        case EntryAction::CreateDirectory:
            return "create directory";
        case EntryAction::SkipPreserved:
            return "skip, preserved by the user";
        case EntryAction::Reject:
            return "reject";
    }
    return "unknown";
}

EntryDecision ExtractionPolicy::decide(std::string_view entryName) const
{
    EntryDecision out;

    // 1. Traversal first. See the header for why this cannot be reordered.
    const Result<std::string, PathError> resolved = resolveUnder(m_root, entryName);
    if (!resolved.hasValue()) {
        out.action = EntryAction::Reject;
        out.error = resolved.error();
        out.reason = std::string(describe(resolved.error())) + ": " + std::string(entryName);
        return out;
    }

    const std::string& destination = resolved.value();
    const bool directory = !entryName.empty() && entryName.back() == '/';

    // 2. Preserve rules, against the entry's own relative name. Matching the
    // relative name rather than the absolute one keeps a user's patterns
    // independent of where the pack happens to be extracted.
    std::string_view relative = entryName;
    if (directory) {
        relative.remove_suffix(1);
    }

    if (m_preserve.preserves(relative)) {
        out.action = EntryAction::SkipPreserved;
        out.destination = destination;
        out.reason = "preserved by preserve.txt: " + std::string(relative);
        return out;
    }

    out.action = directory ? EntryAction::CreateDirectory : EntryAction::Write;
    out.destination = destination;
    return out;
}

}  // namespace nsx::core
