# NsxVersion.cmake - the ONLY place a version number enters the build.
#
# The predecessor duplicated its version string in five places (Makefile, the
# curl user-agent, the splash footer, a tools-tab badge and a changelog list),
# so bumping it reliably missed at least one. Here, /VERSION is read once,
# validated, and projected into every consumer:
#
#   /VERSION -> NSX_VERSION_RAW -> project(VERSION) -> version.hpp -> NACP
#
# Runnable standalone for verification:  cmake -P cmake/NsxVersion.cmake
# SPDX-License-Identifier: GPL-3.0-only

if(DEFINED NSX_VERSION_INCLUDED)
    return()
endif()
set(NSX_VERSION_INCLUDED TRUE)

set(NSX_VERSION_FILE "${CMAKE_CURRENT_LIST_DIR}/../VERSION")

if(NOT EXISTS "${NSX_VERSION_FILE}")
    message(FATAL_ERROR "VERSION file not found at ${NSX_VERSION_FILE}")
endif()

file(STRINGS "${NSX_VERSION_FILE}" NSX_VERSION_RAW LIMIT_COUNT 1)
string(STRIP "${NSX_VERSION_RAW}" NSX_VERSION_RAW)

# SemVer 2.0.0. Deliberately strict: no leading "v", no leading zeros, and a
# build-metadata suffix is rejected outright because it must never reach a tag.
set(NSX_SEMVER_RE "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)(-[0-9A-Za-z.-]+)?$")

if(NOT NSX_VERSION_RAW MATCHES "${NSX_SEMVER_RE}")
    message(FATAL_ERROR
        "VERSION must be SemVer 2.0.0 without a leading 'v' (e.g. 0.1.0 or 1.0.0-rc.1).\n"
        "  file:  ${NSX_VERSION_FILE}\n"
        "  value: '${NSX_VERSION_RAW}'")
endif()

set(NSX_VERSION_MAJOR      "${CMAKE_MATCH_1}")
set(NSX_VERSION_MINOR      "${CMAKE_MATCH_2}")
set(NSX_VERSION_PATCH      "${CMAKE_MATCH_3}")
set(NSX_VERSION_CORE       "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
string(REGEX REPLACE "^-" "" NSX_VERSION_PRERELEASE "${CMAKE_MATCH_4}")

# Provenance. Both degrade gracefully: a tarball build without git still works.
find_package(Git QUIET)
set(NSX_GIT_SHA "unknown")
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.."
        OUTPUT_VARIABLE NSX_GIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no
        WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.."
        OUTPUT_VARIABLE NSX_GIT_DIRTY
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(NSX_GIT_DIRTY)
        set(NSX_GIT_SHA "${NSX_GIT_SHA}-dirty")
    endif()
endif()

# SOURCE_DATE_EPOCH support keeps release builds reproducible.
if(DEFINED ENV{SOURCE_DATE_EPOCH})
    string(TIMESTAMP NSX_BUILD_DATE "%Y-%m-%d" UTC)
else()
    string(TIMESTAMP NSX_BUILD_DATE "%Y-%m-%d" UTC)
endif()

message(STATUS "NSX Manager version ${NSX_VERSION_RAW} (git ${NSX_GIT_SHA}, built ${NSX_BUILD_DATE})")

# `cmake -P` runs with no project; print and exit so this file is verifiable
# on its own without configuring the whole tree.
if(CMAKE_SCRIPT_MODE_FILE)
    message(STATUS "  major=${NSX_VERSION_MAJOR} minor=${NSX_VERSION_MINOR} "
                   "patch=${NSX_VERSION_PATCH} prerelease='${NSX_VERSION_PRERELEASE}'")
endif()
