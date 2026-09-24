# nsx-forwarder.nro - the second binary.
#
# A running .nro cannot overwrite itself, so the app stages the new binary,
# chainloads this forwarder, and the forwarder performs the swap and chainloads
# back. See ADR-0007 and docs/architecture/self-update-forwarder.md.
#
# It links nsx_core ONLY. In particular it shares nsx::core::Handoff with the
# app, so the handoff file has exactly one parser - the predecessor had two
# divergent ad-hoc parsers (utils.cpp:555 and app-forwarder/main.cpp:26).
# SPDX-License-Identifier: GPL-3.0-only

set(NSX_FORWARDER_SOURCES
    src/main.cpp
    # src/swap.cpp
    # src/recovery.cpp
)

list(TRANSFORM NSX_FORWARDER_SOURCES PREPEND "${CMAKE_CURRENT_LIST_DIR}/")

if(NOT NSX_FORWARDER_SOURCES)
    message(STATUS "nsx-forwarder.nro: no sources yet - target skipped")
    return()
endif()

add_executable(nsx-forwarder ${NSX_FORWARDER_SOURCES})
target_link_libraries(nsx-forwarder PRIVATE nsx::core)
target_include_directories(nsx-forwarder PRIVATE "${NSX_SOURCE_ROOT}" "${NSX_GENERATED_DIR}")

# The handoff path is compiled in as an ABSOLUTE path. The predecessor opened
# "forwarder.conf" relative to the CWD, which only worked because hbmenu happens
# to chdir, and silently yielded empty strings otherwise.
target_compile_definitions(nsx-forwarder PRIVATE
    NSX_HANDOFF_PATH="/config/nsx-manager/staging/handoff.json")

nx_generate_nacp(nsx-forwarder.nacp
    NAME    "NSX Manager (Repair)"
    AUTHOR  "M.S. | Designer & Developer"
    VERSION "${NSX_VERSION_RAW}")

nx_create_nro(nsx-forwarder NACP nsx-forwarder.nacp ICON "${CMAKE_SOURCE_DIR}/assets/icon.jpg")
