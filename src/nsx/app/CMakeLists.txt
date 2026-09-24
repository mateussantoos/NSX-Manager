# Layer 6 - app. The composition root: the only place that knows about every
# other layer, and the only place allowed to construct concrete types.
# SPDX-License-Identifier: GPL-3.0-only

set(NSX_APP_SOURCES
    main.cpp
    # bootstrap.cpp
    # container.cpp
)

list(TRANSFORM NSX_APP_SOURCES PREPEND "${CMAKE_CURRENT_LIST_DIR}/")

if(NOT NSX_APP_SOURCES)
    message(STATUS "nsx-manager.nro: no sources yet - target skipped")
    return()
endif()

add_executable(nsx-manager ${NSX_APP_SOURCES})

# Link the layers that actually exist. nsx_add_layer() skips a layer with no
# sources, so naming ui/domain/platform unconditionally would fail the
# configure until every one of them has landed - and the point of the
# composition root is to grow one layer at a time.
set(NSX_APP_LIBS nsx::core)
foreach(layer platform infra domain ui)
    if(TARGET nsx_${layer})
        list(APPEND NSX_APP_LIBS nsx::${layer})
    endif()
endforeach()

target_link_libraries(nsx-manager PRIVATE ${NSX_APP_LIBS})

# Brings up romfs, pl, setsys, set and nifm before main. Added to this
# target's own sources rather than linked, for the reason in NsxLayer.cmake.
nsx_add_switch_services(nsx-manager)
target_include_directories(nsx-manager PRIVATE "${NSX_SOURCE_ROOT}" "${NSX_GENERATED_DIR}")

# The CA bundle header is generated, so the app must wait for it.
if(TARGET nsx_ca_bundle)
    add_dependencies(nsx-manager nsx_ca_bundle)
endif()

nx_generate_nacp(nsx-manager.nacp
    NAME    "NSX Manager"
    AUTHOR  "M.S. | Designer & Developer"
    VERSION "${NSX_VERSION_RAW}")

nx_create_nro(nsx-manager
    NACP  nsx-manager.nacp
    ICON  "${CMAKE_SOURCE_DIR}/assets/icon.jpg"
    ROMFS "${NSX_ROMFS_DIR}")

if(TARGET nsx_romfs)
    add_dependencies(nsx-manager nsx_romfs)
endif()
