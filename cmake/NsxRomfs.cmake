# NsxRomfs.cmake - stage everything the app reads from romfs:/.
#
# The contents of the NRO are an explicit list, not "whatever was in a source
# directory". The predecessor's Makefile ROMFS target copied trees around with
# `cp -ruf` and then deleted files by wildcard, which meant nobody could say
# what actually shipped.
# SPDX-License-Identifier: GPL-3.0-only

set(NSX_ROMFS_DEPS "")

# --- static assets ---------------------------------------------------------
# Staged through a script rather than `cmake -E copy_directory` so the .gitkeep
# placeholders that hold empty directories in git do not end up inside the NRO.
foreach(dir images sounds data i18n)
    if(EXISTS "${CMAKE_SOURCE_DIR}/assets/${dir}")
        list(APPEND NSX_ROMFS_COPY_CMDS
            COMMAND "${CMAKE_COMMAND}"
                    -DSRC=${CMAKE_SOURCE_DIR}/assets/${dir}
                    -DDST=${NSX_ROMFS_DIR}/${dir}
                    -P "${CMAKE_SOURCE_DIR}/cmake/NsxStageAssets.cmake")
    endif()
endforeach()

# --- the in-app changelog is generated from CHANGELOG.md, never hand-copied --
list(APPEND NSX_ROMFS_COPY_CMDS
    COMMAND "${CMAKE_COMMAND}" -E copy
            "${CMAKE_SOURCE_DIR}/CHANGELOG.md" "${NSX_ROMFS_DIR}/data/changelog.md")

# --- the forwarder ships inside the app so a broken one can be re-deployed --
if(TARGET nsx-forwarder)
    list(APPEND NSX_ROMFS_COPY_CMDS
        COMMAND "${CMAKE_COMMAND}" -E copy
                "$<TARGET_FILE_DIR:nsx-forwarder>/nsx-forwarder.nro"
                "${NSX_ROMFS_DIR}/nsx-forwarder.nro")
    list(APPEND NSX_ROMFS_DEPS nsx-forwarder)
endif()

if(TARGET nsx_rcm)
    list(APPEND NSX_ROMFS_DEPS nsx_rcm)
endif()

add_custom_target(nsx_romfs ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${NSX_ROMFS_DIR}"
    ${NSX_ROMFS_COPY_CMDS}
    DEPENDS ${NSX_ROMFS_DEPS}
    COMMENT "Staging romfs"
    VERBATIM)
