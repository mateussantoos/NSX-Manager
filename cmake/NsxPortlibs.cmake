# NsxPortlibs.cmake - expose devkitPro's Switch portlibs as one CMake target.
#
# devkitPro's Switch.cmake sets up the compiler and the NRO helpers, but it does
# NOT put $DEVKITPRO/portlibs/switch/include on the search path. Linking a bare
# `z` therefore compiles until the first `#include <zlib.h>` and then fails with
# "No such file or directory", which points at the wrong problem.
#
# This module resolves each library explicitly, so a missing portlib fails at
# CONFIGURE time naming the exact pacman package to install, rather than during
# compilation of a vendored third-party file.
#
# SPDX-License-Identifier: GPL-3.0-only

if(TARGET nsx::portlibs)
    return()
endif()

if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "DEVKITPRO is not set - source the devkitPro environment")
endif()

set(NSX_PORTLIBS_ROOT "$ENV{DEVKITPRO}/portlibs/switch")

if(NOT EXISTS "${NSX_PORTLIBS_ROOT}/include")
    message(FATAL_ERROR
        "Switch portlibs not found at ${NSX_PORTLIBS_ROOT}\n"
        "Install them with:\n"
        "    dkp-pacman -S switch-zlib switch-curl switch-mbedtls switch-glfw \\\n"
        "                  switch-glad switch-mesa switch-libdrm_nouveau\n"
        "Or use the container, which already has them: docker compose run --rm nsx switch")
endif()

add_library(nsx_portlibs INTERFACE)
add_library(nsx::portlibs ALIAS nsx_portlibs)

target_include_directories(nsx_portlibs SYSTEM INTERFACE "${NSX_PORTLIBS_ROOT}/include")
target_link_directories(nsx_portlibs INTERFACE "${NSX_PORTLIBS_ROOT}/lib")

# nsx_require_portlib(<link name> <header> <pacman package>)
#
# Verifies the library and its header are actually present and reports the
# package to install when they are not.
function(nsx_require_portlib name header package)
    string(TOUPPER "${name}" upper)

    find_library(NSX_PORTLIB_${upper}
        NAMES "${name}"
        PATHS "${NSX_PORTLIBS_ROOT}/lib"
        NO_DEFAULT_PATH)

    find_path(NSX_PORTLIB_${upper}_INCLUDE
        NAMES "${header}"
        PATHS "${NSX_PORTLIBS_ROOT}/include"
        NO_DEFAULT_PATH)

    if(NOT NSX_PORTLIB_${upper} OR NOT NSX_PORTLIB_${upper}_INCLUDE)
        message(FATAL_ERROR
            "Missing Switch portlib '${name}' (header ${header}).\n"
            "Install it with:  dkp-pacman -S ${package}")
    endif()

    mark_as_advanced(NSX_PORTLIB_${upper} NSX_PORTLIB_${upper}_INCLUDE)
endfunction()

# Everything the application links, checked up front. The pairings mirror the
# link lines in third_party/CMakeLists.txt and the infra layer.
nsx_require_portlib(z        zlib.h            switch-zlib)
nsx_require_portlib(curl     curl/curl.h       switch-curl)
nsx_require_portlib(mbedtls  mbedtls/ssl.h     switch-mbedtls)
nsx_require_portlib(glfw3    GLFW/glfw3.h      switch-glfw)
nsx_require_portlib(glad     glad/glad.h       switch-glad)
nsx_require_portlib(EGL      EGL/egl.h         switch-mesa)

message(STATUS "Switch portlibs: ${NSX_PORTLIBS_ROOT}")
