# nsx-ui-probe.nro - a throwaway binary that boots the Borealis shell.
#
# Built by default so CI keeps the UI link line honest: nsx_ui is a static
# library, and a static library nothing links still compiles while its symbols
# are never resolved. Without a binary that actually links it, a broken Borealis
# wrapper would build green.
#
# Separate from nsx-manager.nro deliberately - see the comment in src/main.cpp.
# Never published: the release workflow uploads by explicit name pattern
# (dist/nsx-manager-*.nro, dist/nsx-forwarder-*.nro) and this matches neither.
#
# Delete this directory once main.cpp calls nsx::ui::runShell().
# SPDX-License-Identifier: GPL-3.0-only

if(NOT TARGET nsx_ui)
    message(STATUS "nsx-ui-probe.nro: nsx_ui has no sources yet - target skipped")
    return()
endif()

add_executable(nsx-ui-probe "${CMAKE_CURRENT_LIST_DIR}/src/main.cpp")
target_link_libraries(nsx-ui-probe PRIVATE nsx::ui)

# Brings up romfs, pl, setsys, set and nifm before main. Added to this
# target's own sources rather than linked, for the reason in NsxLayer.cmake.
nsx_add_switch_services(nsx-ui-probe)
target_include_directories(nsx-ui-probe PRIVATE "${NSX_SOURCE_ROOT}" "${NSX_GENERATED_DIR}")

nx_generate_nacp(nsx-ui-probe.nacp
    NAME    "NSX UI probe"
    AUTHOR  "M.S. | Designer & Developer"
    VERSION "${NSX_VERSION_RAW}")

# Carries the same romfs as the application: it is testing that staging too,
# and a probe that shipped different resources would prove nothing about the
# binary that matters.
nx_create_nro(nsx-ui-probe
    NACP  nsx-ui-probe.nacp
    ICON  "${CMAKE_SOURCE_DIR}/assets/icon.jpg"
    ROMFS "${NSX_ROMFS_DIR}")

if(TARGET nsx_romfs)
    add_dependencies(nsx-ui-probe nsx_romfs)
endif()
