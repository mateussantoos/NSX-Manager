# NSX Manager - root build.
#
# Three binaries come out of this repository:
#   nsx-manager.nro    the application            (devkitA64, this CMake tree)
#   nsx-forwarder.nro  the self-update helper     (devkitA64, this CMake tree)
#   nsx_rcm.bin        the RCM payload            (devkitARM, its own Makefile,
#                                                  GPL-2.0-only, never linked -
#                                                  see ADR-0011)
#
# Why CMake and not the devkitPro Makefile: ADR-0002.
# SPDX-License-Identifier: GPL-3.0-only

cmake_minimum_required(VERSION 3.24)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(NsxVersion)

project(nsx-manager
    VERSION     ${NSX_VERSION_CORE}
    DESCRIPTION "On-console manager for Nintendo Switch custom firmware and homebrew"
    HOMEPAGE_URL "https://github.com/mateussantoos/nsx-manager"
    LANGUAGES   C CXX)

# --------------------------------------------------------------------------
# Options
# --------------------------------------------------------------------------
option(NSX_BUILD_TESTS  "Build host unit tests (requires a host compiler)" OFF)
option(NSX_BUILD_APP    "Build the Switch application and forwarder"       ON)
option(NSX_BUILD_RCM    "Build the RCM payload via its devkitARM Makefile" ON)
option(NSX_WERROR       "Treat warnings as errors in first-party code"     ON)

set(NSX_SOURCE_ROOT  "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(NSX_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(NSX_ROMFS_DIR     "${CMAKE_BINARY_DIR}/romfs")

# nx_create_nro() validates the romfs directory at CONFIGURE time, but its
# contents are staged at BUILD time by nsx_romfs. Create it here so a clean
# build directory configures - otherwise the first configure after `rm -rf
# build/` fails with "cannot find romfs dir", which points nowhere useful.
file(MAKE_DIRECTORY "${NSX_ROMFS_DIR}")

include(NsxLayer)
include(CTest)

# --------------------------------------------------------------------------
# Generated headers - the single source of truth reaching the code
# --------------------------------------------------------------------------
configure_file(
    "${NSX_SOURCE_ROOT}/nsx/core/version/version.hpp.in"
    "${NSX_GENERATED_DIR}/nsx/core/version/version.hpp"
    @ONLY)

# --------------------------------------------------------------------------
# Detect the toolchain we are under
# --------------------------------------------------------------------------
if(NINTENDO_SWITCH OR DEFINED ENV{DEVKITPRO} AND CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
    set(NSX_TARGET_SWITCH ON)
else()
    set(NSX_TARGET_SWITCH OFF)
endif()

if(NOT NSX_TARGET_SWITCH)
    # Host build: the Switch binaries cannot be produced, so they are off by
    # construction. The portable subset of every layer still builds - see the
    # else() branch below. This is the configuration CI runs unit tests in.
    set(NSX_BUILD_APP OFF)
    set(NSX_BUILD_RCM OFF)
    message(STATUS "Host toolchain detected - building the portable layers and tests")
endif()

# --------------------------------------------------------------------------
# Layers. Order matters: each may only link the ones above it (ADR-0003),
# which tools/lint/check_layering.sh enforces at the #include level too.
# --------------------------------------------------------------------------
add_subdirectory(third_party)
add_subdirectory(src/nsx/core)
nsx_add_layer(core SOURCES ${NSX_CORE_SOURCES} DEPENDS nsx::json)

if(NSX_BUILD_APP)
    include(NsxPortlibs)
    include(NsxCaBundle)

    add_subdirectory(src/nsx/platform)
    nsx_add_layer(platform SOURCES ${NSX_PLATFORM_SOURCES} DEPENDS nsx_core)

    add_subdirectory(src/nsx/infra)
    nsx_add_layer(infra    SOURCES ${NSX_INFRA_SOURCES}
                           DEPENDS nsx_core nsx_platform nsx::portlibs curl z mbedtls nsx::zipper)
    # infra/http includes the generated ca_bundle.hpp.
    if(TARGET nsx_infra AND TARGET nsx_ca_bundle)
        add_dependencies(nsx_infra nsx_ca_bundle)
    endif()

    add_subdirectory(src/nsx/domain)
    nsx_add_layer(domain   SOURCES ${NSX_DOMAIN_SOURCES} DEPENDS nsx_core nsx_infra)

    add_subdirectory(src/nsx/ui)
    nsx_add_layer(ui       SOURCES ${NSX_UI_SOURCES}     DEPENDS nsx_core nsx_domain nsx::borealis)

    add_subdirectory(src/nsx/app)
    add_subdirectory(apps/forwarder)
    add_subdirectory(apps/ui-probe)
else()
    # Host build. Not every layer is portable, but the portable SUBSET of infra
    # and domain is - and it is the important one: the update orchestration and
    # the error taxonomy the UI renders. Building it here is what lets
    # tests/unit/selfupdate_test.cpp drive a whole update - cache, backoff,
    # manifest, download, handoff - against fakes, with no console involved.
    # The adapters that touch libcurl and the SD card stay out (ADR-0014).
    add_subdirectory(src/nsx/infra)
    nsx_add_layer(infra  SOURCES ${NSX_INFRA_HOST_SOURCES}  DEPENDS nsx_core)

    add_subdirectory(src/nsx/domain)
    nsx_add_layer(domain SOURCES ${NSX_DOMAIN_HOST_SOURCES} DEPENDS nsx_core nsx_infra)
endif()

if(NSX_BUILD_RCM)
    include(NsxRcmPayload)
endif()

if(NSX_BUILD_TESTS AND NOT NSX_TARGET_SWITCH)
    add_subdirectory(tests)
endif()

# --------------------------------------------------------------------------
# romfs assembly. Everything the running app reads from romfs:/ is staged
# here, so the contents of the NRO are an explicit list rather than whatever
# happened to be in a source directory.
# --------------------------------------------------------------------------
if(NSX_BUILD_APP)
    include(NsxRomfs)
endif()

message(STATUS "")
message(STATUS "NSX Manager ${NSX_VERSION_RAW}")
message(STATUS "  switch target : ${NSX_TARGET_SWITCH}")
message(STATUS "  app/forwarder : ${NSX_BUILD_APP}")
message(STATUS "  rcm payload   : ${NSX_BUILD_RCM}")
message(STATUS "  host tests    : ${NSX_BUILD_TESTS}")
message(STATUS "")
