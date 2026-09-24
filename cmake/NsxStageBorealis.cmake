# NsxStageBorealis.cmake - stage the Borealis runtime resources into romfs.
#
#   cmake -DSRC=<borealis/resources> -DDST=<romfs dir> -P cmake/NsxStageBorealis.cmake
#
# Borealis resolves its assets through BOREALIS_ASSET(), which the wrapper
# defines as "romfs:/". Without these files it does not fail to link, it fails
# to *render* - fontStash gets -1 for every font and nothing draws. That makes
# this the kind of omission that only shows up on hardware, so the copy list is
# explicit and the build fails loudly when a required file is missing.
#
# DELIBERATELY NOT a copy of the whole directory. `resources/` belongs to the
# fork's own example application, and contains its strings (main.json:
# "Borealis Example App", popup.json, installer.json, custom_layout.json) and
# its icon. Those are not framework resources and have no business inside
# nsx-manager.nro - `brls.json` is the only i18n file Borealis itself reads.
#
# SPDX-License-Identifier: GPL-3.0-only

if(NOT DEFINED SRC OR NOT DEFINED DST)
    message(FATAL_ERROR "usage: cmake -DSRC=<dir> -DDST=<dir> -P NsxStageBorealis.cmake")
endif()

if(NOT EXISTS "${SRC}")
    message(FATAL_ERROR
        "Borealis resources not found at ${SRC}\n"
        "The submodule is missing. Run: git submodule update --init --recursive")
endif()

# Borealis loads these by hard-coded path in library/lib/application.cpp:366,377.
# A missing font is not a warning there - it is a blank screen.
set(REQUIRED
    inter/Inter-Switch.ttf
    material/MaterialIcons-Regular.ttf)

# Shipped because the fonts are shipped. Inter is OFL-1.1 and Material Icons is
# Apache-2.0; both require the licence to travel with the work, and both are
# embedded in the NRO as data. See NOTICE.
set(LICENCES
    inter/LICENSE.txt
    material/LICENSE.txt)

set(copied 0)

foreach(rel IN LISTS REQUIRED)
    if(NOT EXISTS "${SRC}/${rel}")
        message(FATAL_ERROR
            "Borealis requires ${rel}, which is not in ${SRC}.\n"
            "It is loaded by path at runtime, so a build without it renders nothing.")
    endif()
    configure_file("${SRC}/${rel}" "${DST}/${rel}" COPYONLY)
    math(EXPR copied "${copied} + 1")
endforeach()

foreach(rel IN LISTS LICENCES)
    if(EXISTS "${SRC}/${rel}")
        configure_file("${SRC}/${rel}" "${DST}/${rel}" COPYONLY)
        math(EXPR copied "${copied} + 1")
    else()
        message(WARNING "Borealis ${rel} is missing - a shipped font has lost its licence text")
    endif()
endforeach()

# --------------------------------------------------------------------------
# Framework strings, per locale we actually ship.
#
# Borealis merges every *.json in romfs:/i18n/<locale>/ into one namespace keyed
# by filename, so its `brls.json` sits beside our own files rather than
# replacing them. Driven by the locales already staged from assets/i18n, so a
# locale we do not ship does not acquire framework strings nobody reads.
# --------------------------------------------------------------------------
set(DEFAULT_LOCALE "en-US")

file(GLOB staged_locales RELATIVE "${DST}/i18n" "${DST}/i18n/*")

set(locales "${DEFAULT_LOCALE}")
foreach(loc IN LISTS staged_locales)
    if(IS_DIRECTORY "${DST}/i18n/${loc}")
        list(APPEND locales "${loc}")
    endif()
endforeach()
list(REMOVE_DUPLICATES locales)

foreach(loc IN LISTS locales)
    if(EXISTS "${SRC}/i18n/${loc}/brls.json")
        configure_file("${SRC}/i18n/${loc}/brls.json" "${DST}/i18n/${loc}/brls.json" COPYONLY)
        math(EXPR copied "${copied} + 1")
    elseif(loc STREQUAL DEFAULT_LOCALE)
        # Borealis falls back to en-US for anything the current locale lacks,
        # so this one is not optional.
        message(FATAL_ERROR "Borealis has no i18n/${DEFAULT_LOCALE}/brls.json in ${SRC}")
    else()
        message(STATUS "romfs/i18n/${loc}: no Borealis strings upstream - framework text "
                       "will fall back to ${DEFAULT_LOCALE}")
    endif()
endforeach()

message(STATUS "romfs (borealis): ${copied} file(s)")
