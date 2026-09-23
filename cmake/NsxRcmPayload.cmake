# NsxRcmPayload.cmake - build apps/rcm-payload/ WITHOUT absorbing it.
#
# The payload is hekate/BDK-derived and GPL-2.0-ONLY (no "or any later version"
# clause), which is incompatible with this GPL-3.0 application. It therefore
# stays a separate program: separate toolchain (devkitARM), separate Makefile,
# separate load address, no shared headers. CMake only shells out to its own
# build and copies the resulting blob into romfs as opaque data.
#
# See ADR-0011 and docs/architecture/licensing.md.
# SPDX-License-Identifier: GPL-3.0-only

set(NSX_RCM_DIR    "${CMAKE_SOURCE_DIR}/apps/rcm-payload")
set(NSX_RCM_OUTPUT "${NSX_RCM_DIR}/output/nsx_rcm.bin")
set(NSX_RCM_STAGED "${NSX_ROMFS_DIR}/nsx_rcm.bin")

if(NOT EXISTS "${NSX_RCM_DIR}/Makefile")
    message(STATUS "rcm payload: apps/rcm-payload/Makefile not present - skipped")
    return()
endif()

if(NOT DEFINED ENV{DEVKITARM})
    message(WARNING "DEVKITARM is not set - the RCM payload cannot be built. "
                    "Configure with -DNSX_BUILD_RCM=OFF to silence this.")
    return()
endif()

add_custom_command(
    OUTPUT  "${NSX_RCM_STAGED}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${NSX_ROMFS_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E env "DEVKITARM=$ENV{DEVKITARM}"
            make -C "${NSX_RCM_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${NSX_RCM_OUTPUT}" "${NSX_RCM_STAGED}"
    COMMENT "Building RCM payload (devkitARM, GPL-2.0-only, isolated)"
    VERBATIM)

add_custom_target(nsx_rcm DEPENDS "${NSX_RCM_STAGED}")
