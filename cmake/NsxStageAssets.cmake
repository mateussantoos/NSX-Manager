# NsxStageAssets.cmake - copy an asset directory into romfs, skipping dotfiles.
#
# Run as a script at BUILD time, not configure time, so a newly added asset is
# picked up without re-running cmake:
#
#   cmake -DSRC=<dir> -DDST=<dir> -P cmake/NsxStageAssets.cmake
#
# `cmake -E copy_directory` would work except that it copies everything,
# including the .gitkeep placeholders that hold empty asset directories in git.
# Those would then ship inside the NRO - small, but it means the romfs contents
# are not exactly the declared list, and "what is actually in the build?" stops
# having a clean answer.
#
# SPDX-License-Identifier: GPL-3.0-only

if(NOT DEFINED SRC OR NOT DEFINED DST)
    message(FATAL_ERROR "usage: cmake -DSRC=<dir> -DDST=<dir> -P NsxStageAssets.cmake")
endif()

if(NOT EXISTS "${SRC}")
    return()
endif()

file(GLOB_RECURSE assets RELATIVE "${SRC}" "${SRC}/*")

set(copied 0)
foreach(rel IN LISTS assets)
    get_filename_component(name "${rel}" NAME)

    # Skip git placeholders and any other dotfile - none of them are runtime data.
    if(name MATCHES "^\\.")
        continue()
    endif()

    configure_file("${SRC}/${rel}" "${DST}/${rel}" COPYONLY)
    math(EXPR copied "${copied} + 1")
endforeach()

if(copied GREATER 0)
    get_filename_component(label "${DST}" NAME)
    message(STATUS "romfs/${label}: ${copied} file(s)")
endif()
