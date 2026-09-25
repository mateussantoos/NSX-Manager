// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "nsx/domain/ports/ports.hpp"

namespace nsx::domain {

/// @brief Metadata and enable state for an Atmosphere sysmodule.
/// @since 0.4.0
struct SysmoduleInfo
{
    std::string titleId;      ///< 16-hex character Title ID (e.g. "0100000000001000").
    std::string name;         ///< Human-readable name or fallback to title ID.
    std::string author;       ///< Author if present in toolbox.json.
    std::string description;  ///< Description if present in toolbox.json.
    bool enabled{false};      ///< Whether flags/boot2.flag is present.
};

/// @brief Manages Atmosphere sysmodules under /atmosphere/contents.
///
/// @details Sysmodules live in `/atmosphere/contents/<title_id>/`.
///          Atmosphere launches them on startup if `flags/boot2.flag` exists.
///          Metadata (name, author, description) is read from `toolbox.json` if available,
///          with fallback to known title ID mappings or directory names.
/// @since 0.4.0
class SysmoduleService
{
public:
    explicit SysmoduleService(FileStore& files, std::string contentsRoot = "/atmosphere/contents");

    /// @brief Enumerate all sysmodules found under contents root.
    /// @return List of sysmodule info.
    [[nodiscard]] std::vector<SysmoduleInfo> listSysmodules() const;

    /// @brief Check if a specific sysmodule is enabled on boot.
    /// @param titleId The 16-hex title ID.
    /// @return True if flags/boot2.flag exists.
    [[nodiscard]] bool isEnabled(std::string_view titleId) const;

    /// @brief Toggle or set the boot enable state of a sysmodule.
    /// @param titleId The 16-hex title ID.
    /// @param enable True to create flags/boot2.flag, false to remove it.
    /// @return True if operation succeeded.
    [[nodiscard]] bool setEnabled(std::string_view titleId, bool enable);

    /// @brief The configured contents root directory.
    [[nodiscard]] const std::string& contentsRoot() const { return m_contentsRoot; }

private:
    FileStore& m_files;
    std::string m_contentsRoot;
};

}  // namespace nsx::domain
