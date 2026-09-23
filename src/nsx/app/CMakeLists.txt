# Layer 6 - app. The composition root: the only place that knows about every
# other layer, and the only place allowed to construct concrete types.
# SPDX-License-Identifier: GPL-3.0-only

set(NSX_APP_SOURCES
    # main.cpp
    # bootstrap.cpp
    # container.cpp
)

if(NOT NSX_APP_SOURCES)
    message(STATUS "nsx-manager.nro: no sources yet - target skipped")
    return()
endif()

add_executable(nsx-manager ${NSX_APP_SOURCES})
target_link_libraries(nsx-manager PRIVATE nsx::ui nsx::domain nsx::infra nsx::platform nsx::core)
target_include_directories(nsx-manager PRIVATE "${NSX_SOURCE_ROOT}" "${NSX_GENERATED_DIR}")

nx_generate_nacp(nsx-manager.nacp
    NAME    "NSX Manager"
    AUTHOR  "M.S. | Designer & Developer"
    VERSION "${NSX_VERSION_RAW}")

nx_create_nro(nsx-manager
    NACP  nsx-manager.nacp
    ICON  "${CMAKE_SOURCE_DIR}/assets/icon.jpg"
    ROMFS "${NSX_ROMFS_DIR}")

add_dependencies(nsx-manager nsx_romfs)
