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

if(EXISTS "${CMAKE_SOURCE_DIR}/assets/splash.png")
    list(APPEND NSX_ROMFS_COPY_CMDS
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CMAKE_SOURCE_DIR}/assets/splash.png"
                "${NSX_ROMFS_DIR}/splash.png")
endif()

# --- Borealis runtime resources -------------------------------------------
# Fonts and framework strings, which Borealis loads by hard-coded path through
# BOREALIS_ASSET() - the wrapper defines that as "romfs:/". Without them the UI
# links and boots and then draws nothing, which is only visible on hardware.
#
# AFTER the static assets above, because the locale list is driven by the
# locales those staged: Borealis's brls.json has to land beside our own strings
# in the same romfs:/i18n/<locale>/ directory.
list(APPEND NSX_ROMFS_COPY_CMDS
    COMMAND "${CMAKE_COMMAND}"
            -DSRC=${CMAKE_SOURCE_DIR}/third_party/borealis/resources
            -DDST=${NSX_ROMFS_DIR}
            -P "${CMAKE_SOURCE_DIR}/cmake/NsxStageBorealis.cmake")

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

    # Depend on the NRO target, not the executable. nx_create_nro() produces a
    # separate target named <name>_nro that turns the .elf into a .nro; waiting
    # only on `nsx-forwarder` waits for the ELF, and the copy then races the
    # NRO step. It happened to win on an incremental build and lost on a clean
    # one - the worst kind of build bug to leave in.
    if(TARGET nsx-forwarder_nro)
        list(APPEND NSX_ROMFS_DEPS nsx-forwarder_nro)
    else()
        list(APPEND NSX_ROMFS_DEPS nsx-forwarder)
    endif()
endif()

if(TARGET nsx_rcm)
    list(APPEND NSX_ROMFS_DEPS nsx_rcm)
endif()

# --- the stamp, always last -----------------------------------------------
# Summarises what was actually staged. See NsxRomfsStamp.cmake for why an NRO
# does not otherwise notice that its romfs changed.
set(NSX_ROMFS_STAMP "${CMAKE_BINARY_DIR}/romfs.stamp")

list(APPEND NSX_ROMFS_COPY_CMDS
    COMMAND "${CMAKE_COMMAND}"
            -DDIR=${NSX_ROMFS_DIR}
            -DSTAMP=${NSX_ROMFS_STAMP}
            -P "${CMAKE_SOURCE_DIR}/cmake/NsxRomfsStamp.cmake")

add_custom_target(nsx_romfs ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${NSX_ROMFS_DIR}"
    ${NSX_ROMFS_COPY_CMDS}
    DEPENDS ${NSX_ROMFS_DEPS}
    BYPRODUCTS "${NSX_ROMFS_STAMP}"
    COMMENT "Staging romfs"
    VERBATIM)

# nsx_rebuild_nro_on_romfs_change(<target>)
#
# Makes <target> relink when the staged romfs changes, so the .nro built from it
# is rebuilt too. nx_create_nro()'s command already depends on the ELF target,
# so a relink is enough to carry the new romfs through.
#
# Implemented with OBJECT_DEPENDS rather than add_dependencies() because an
# ordering dependency on a custom target does not make an up-to-date output
# stale - it only sequences them, which is exactly the trap this whole file
# exists to document.
function(nsx_rebuild_nro_on_romfs_change target)
    if(NOT TARGET ${target})
        return()
    endif()
    get_target_property(_srcs ${target} SOURCES)
    if(NOT _srcs)
        return()
    endif()
    foreach(src IN LISTS _srcs)
        set_property(SOURCE "${src}" TARGET_DIRECTORY ${target}
                     APPEND PROPERTY OBJECT_DEPENDS "${NSX_ROMFS_STAMP}")
    endforeach()
endfunction()

# Every target whose NRO embeds the romfs.
foreach(_nro_target nsx-manager)
    nsx_rebuild_nro_on_romfs_change(${_nro_target})
endforeach()
