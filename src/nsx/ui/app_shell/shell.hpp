// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string>

/// @brief Borealis views. The only layer allowed to know the UI framework exists.
namespace nsx::ui {

/// @brief Why the shell could not start.
/// @since 0.2.0
enum class ShellError
{
    None,          ///< It started.
    RomfsMissing,  ///< The fonts Borealis draws with are not in romfs.
    BorealisInit   ///< Borealis refused to initialise; see its log.
};

/// @brief A short English description of a startup failure.
/// @param error The error to describe.
/// @return A description suitable for a log line or a console fallback.
/// @since 0.2.0
[[nodiscard]] std::string_view describe(ShellError error);

/// @brief Whether the resources Borealis draws with are actually present.
///
/// @details Borealis resolves its fonts through `BOREALIS_ASSET()`, which this
///          build defines as `romfs:/`, and it does **not** fail when one is
///          missing - `loadFont` returns -1, `Application::init` still
///          succeeds, and the console renders a black screen with no
///          indication why. Checking first turns that into a message.
///
///          See `cmake/NsxStageBorealis.cmake`, which puts them there.
///
/// @return True when every required resource is readable.
/// @since 0.2.0
[[nodiscard]] bool resourcesPresent();

/// @brief Start Borealis, show the shell, and run until the user exits.
///
/// @details Blocks for the lifetime of the UI. Returns once the user quits or
///          the applet is closed.
///
///          Deliberately minimal for now: this exists to prove the link line
///          (glfw3, EGL, glad, glapi, drm_nouveau), the romfs staging and the
///          i18n wiring on hardware. The tabs and the update view are built on
///          top of a shell that is known to boot, not at the same time as one.
///
/// @return @ref ShellError::None when the UI ran.
/// @since 0.2.0
[[nodiscard]] ShellError runShell();

}  // namespace nsx::ui
