// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <string_view>

#include "nsx/core/result/result.hpp"

namespace nsx::platform {

/// @brief Why a reboot or shutdown command could not be carried out.
/// @since 0.4.0
enum class RebootError
{
    PayloadNotFound,     ///< The RCM payload was not found in romfs.
    PayloadWriteFailed,  ///< Could not write the payload to the SD card.
    ConfigFailed,        ///< Could not configure Atmosphere reboot-to-payload.
    ServiceFailed        ///< The power service refused the shutdown/reboot command.
};

/// @brief A short English description of a reboot failure.
/// @param error The error to describe.
/// @return Suitable description for logs or UI.
/// @since 0.4.0
[[nodiscard]] std::string_view describe(RebootError error);

/// @brief Reboot the console directly to an RCM payload (e.g. Hekate).
///
/// @details Extracts `romfs:/nsx_rcm.bin` to `/payload.bin` and
///          `/atmosphere/reboot_payload.bin`, configures Atmosphere's
///          reboot payload setting (SPL 65001), and commands a reboot
///          via SPSM / BPC.
///
/// @param sourcePayloadPath Where the payload binary lives. Defaults to "romfs:/nsx_rcm.bin".
/// @return Success or specific failure reason.
/// @since 0.4.0
[[nodiscard]] core::Result<std::monostate, RebootError> rebootToPayload(
    std::string_view sourcePayloadPath = "romfs:/nsx_rcm.bin");

/// @brief Reboot the console normally back into Horizon/CFW.
/// @return Success or specific failure reason.
/// @since 0.4.0
[[nodiscard]] core::Result<std::monostate, RebootError> reboot();

/// @brief Shut down the console completely.
/// @return Success or specific failure reason.
/// @since 0.4.0
[[nodiscard]] core::Result<std::monostate, RebootError> shutdown();

}  // namespace nsx::platform
