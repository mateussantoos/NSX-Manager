# NsxRomfsStamp.cmake - summarise the staged romfs into a stamp file.
#
#   cmake -DDIR=<romfs dir> -DSTAMP=<file> -P cmake/NsxRomfsStamp.cmake
#
# WHY THIS EXISTS
#
# devkitPro's nx_create_nro() takes ROMFS either as an asset target or as a
# plain directory. Given a directory it appends `--romfsdir=` to the elf2nro
# command line and adds NOTHING to that command's DEPENDS - only the target form
# registers file-level dependencies through DKP_ASSET_FILES.
#
# The consequence is silent and expensive: edit a translation string, rebuild,
# and the NRO still carries the old one. Everything reports success. You find
# out by copying the build to a console and seeing the previous text.
#
# So: hash the staged tree, and rewrite this stamp only when the digest list
# actually changes. NsxRomfs.cmake then makes the NRO targets depend on it (see
# nsx_rebuild_nro_on_romfs_change), which relinks them - and only then.
# Rewriting the stamp unconditionally would relink every binary on every build.
#
# SPDX-License-Identifier: GPL-3.0-only

if(NOT DEFINED DIR OR NOT DEFINED STAMP)
    message(FATAL_ERROR "usage: cmake -DDIR=<dir> -DSTAMP=<file> -P NsxRomfsStamp.cmake")
endif()

set(digest "")

if(EXISTS "${DIR}")
    file(GLOB_RECURSE staged RELATIVE "${DIR}" "${DIR}/*")
    list(SORT staged)
    foreach(rel IN LISTS staged)
        file(SHA256 "${DIR}/${rel}" hash)
        string(APPEND digest "${rel} ${hash}\n")
    endforeach()
endif()

set(previous "")
if(EXISTS "${STAMP}")
    file(READ "${STAMP}" previous)
endif()

if(NOT previous STREQUAL digest)
    file(WRITE "${STAMP}" "${digest}")
    string(LENGTH "${digest}" _len)
    if(_len GREATER 0)
        list(LENGTH staged _count)
        message(STATUS "romfs: contents changed (${_count} file(s)) - NROs will be rebuilt")
    endif()
endif()
