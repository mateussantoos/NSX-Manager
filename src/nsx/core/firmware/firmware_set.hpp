// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nsx::core {

/// @brief Whether a filename is content a firmware set is made of.
///
/// @details Nintendo firmware is a flat directory of NCA archives. Daybreak
///          reads that directory and nothing else in it, so these are the only
///          names this application ever puts there - and, below, the only ones
///          it is willing to delete.
///
/// @param name A bare filename, not a path.
/// @return True for `.nca` and `.cnmt.nca`.
/// @since 0.3.0
[[nodiscard]] bool isFirmwareContent(std::string_view name);

/// @brief What an existing firmware directory contains.
/// @since 0.3.0
struct FirmwareDirectoryScan
{
    std::size_t firmwareFiles{};          ///< Files this application recognises.
    std::vector<std::string> unexpected;  ///< Everything else, relative paths, sorted.

    /// @brief Whether the directory holds only firmware content.
    /// @return True when nothing unrecognised is present.
    [[nodiscard]] bool onlyFirmware() const { return unexpected.empty(); }

    /// @brief Whether there is anything at all to clear.
    /// @return True when the directory is empty.
    [[nodiscard]] bool empty() const { return firmwareFiles == 0 && unexpected.empty(); }
};

/// @brief Classify the contents of a firmware staging directory.
///
/// @details Deciding whether `/firmware/` may be cleared is the whole reason
///          this exists, and it is a decision worth making carefully. That
///          directory is not ours: it is a well-known path other tools and
///          guides use, and a user may well have put something there.
///
///          A previous firmware set left behind is ours to remove - mixing two
///          versions is how Daybreak installs a broken system. Anything else is
///          not, and the safe answer is to stop and name the file rather than
///          delete something whose purpose we cannot infer.
///
///          Pure, so the judgement is testable without a card.
///
/// @param names Relative paths of everything under the directory.
/// @return What was found.
/// @since 0.3.0
[[nodiscard]] FirmwareDirectoryScan scanFirmwareDirectory(const std::vector<std::string>& names);

/// @brief Whether a set of extracted names looks like usable firmware.
/// @param names Relative paths produced by the extraction.
/// @return True when there is at least one NCA and nothing unexpected.
/// @details An archive that expands to no NCA at all is not firmware, whatever
///          the catalogue called it, and handing Daybreak an empty directory
///          would waste a reboot to find that out.
/// @since 0.3.0
[[nodiscard]] bool looksLikeFirmware(const std::vector<std::string>& names);

}  // namespace nsx::core
