# third_party - vendored dependencies, wrapped so the rest of the tree sees
# ordinary CMake targets.
#
# Upstream Borealis ships only `library/borealis.mk`, so this wrapper is the one
# piece of real migration work CMake costs us (ADR-0002). It is written once and
# then frozen; it lives HERE and not inside the submodule so `git submodule
# update` never conflicts with it.
#
# IMPORTANT - the globs below are deliberately NON-recursive, one per directory,
# because that is exactly what the devkitPro Makefile does:
#
#     CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
#
# A recursive glob over library/lib would additionally pull in fmt's six
# test/fuzzing translation units, which upstream never compiles. Keep these
# lists in one-to-one correspondence with library/borealis.mk.
#
# SPDX-License-Identifier: GPL-3.0-only

# --------------------------------------------------------------------------
# nlohmann/json - vendored single header
# --------------------------------------------------------------------------
add_library(nsx_json INTERFACE)
add_library(nsx::json ALIAS nsx_json)
target_include_directories(nsx_json SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

# --------------------------------------------------------------------------
# doctest - vendored single header, host tests only, never shipped on device
# --------------------------------------------------------------------------
add_library(nsx_doctest INTERFACE)
add_library(nsx::doctest ALIAS nsx_doctest)
target_include_directories(nsx_doctest SYSTEM INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/doctest")

# --------------------------------------------------------------------------
# zipper - minizip wrapper
#
# The predecessor built this with `SOURCES := lib/zipper/source`, i.e. the
# top-level source directory only. source/aes/ is therefore NOT compiled -
# minizip's AES support is conditional and unused here. Matching that keeps us
# byte-for-byte equivalent to a configuration that is known to link.
# --------------------------------------------------------------------------
set(ZIPPER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/zipper")

if(NOT EXISTS "${ZIPPER_DIR}/source")
    message(FATAL_ERROR
        "third_party/zipper is empty. Run:\n"
        "    git submodule update --init --recursive")
endif()

file(GLOB ZIPPER_SRC CONFIGURE_DEPENDS
    "${ZIPPER_DIR}/source/*.c"
    "${ZIPPER_DIR}/source/*.cpp")

add_library(nsx_zipper STATIC ${ZIPPER_SRC})
add_library(nsx::zipper ALIAS nsx_zipper)
target_include_directories(nsx_zipper SYSTEM PUBLIC "${ZIPPER_DIR}/include")
target_link_libraries(nsx_zipper PUBLIC z)
target_compile_options(nsx_zipper PRIVATE -w)   # vendored: we do not own its style

# --------------------------------------------------------------------------
# Borealis - the UI framework (ADR-0010)
#
# Mirrors library/borealis.mk:
#   SOURCES  = lib
#              lib/extern/nanovg
#              lib/extern/libretro-common/{compat,encodings,features}
#              lib/extern/nxfmtwrapper
#   INCLUDES = include
#              lib/extern/fmt/include
#              include/borealis/extern
#   LIBS     = -lglfw3 -lEGL -lglad -lglapi -ldrm_nouveau -lm
# --------------------------------------------------------------------------
set(BRLS "${CMAKE_CURRENT_SOURCE_DIR}/borealis/library")

if(NOT EXISTS "${BRLS}/borealis.mk")
    message(FATAL_ERROR
        "third_party/borealis is empty. Run:\n"
        "    git submodule update --init --recursive")
endif()

file(GLOB BRLS_SRC CONFIGURE_DEPENDS
    "${BRLS}/lib/*.cpp"
    "${BRLS}/lib/*.c"                                   # switch_wrapper.c
    "${BRLS}/lib/extern/nanovg/*.c"
    "${BRLS}/lib/extern/libretro-common/compat/*.c"
    "${BRLS}/lib/extern/libretro-common/encodings/*.c"
    "${BRLS}/lib/extern/libretro-common/features/*.c"
    # format.cpp is a one-line shim that #includes ../fmt/src/format.cc.
    # It exists because devkitPro's base Makefile does not treat .cc as C++.
    # fmt/src is therefore compiled through this file and must never be
    # globbed directly, or format.cc lands in the link twice.
    "${BRLS}/lib/extern/nxfmtwrapper/*.cpp")

list(LENGTH BRLS_SRC BRLS_SRC_COUNT)
if(BRLS_SRC_COUNT LESS 30)
    message(FATAL_ERROR
        "Borealis source glob produced only ${BRLS_SRC_COUNT} files - upstream "
        "layout has changed. Re-check this wrapper against library/borealis.mk.")
endif()
message(STATUS "Borealis: ${BRLS_SRC_COUNT} translation units")

add_library(nsx_borealis STATIC ${BRLS_SRC})
add_library(nsx::borealis ALIAS nsx_borealis)

target_include_directories(nsx_borealis SYSTEM PUBLIC
    "${BRLS}/include"
    "${BRLS}/lib/extern/fmt/include"
    "${BRLS}/include/borealis/extern")   # resolves <libretro-common/compat/strl.h>

target_compile_definitions(nsx_borealis PUBLIC BOREALIS_RESOURCES="romfs:/")

# glad, glfw3, EGL, glapi and drm_nouveau come from devkitPro portlibs - they
# are NOT vendored in the Borealis tree.
target_link_libraries(nsx_borealis PUBLIC glfw3 EGL glad glapi drm_nouveau m)

target_compile_features(nsx_borealis PUBLIC cxx_std_20)
target_compile_options(nsx_borealis PRIVATE -w)   # vendored: we do not own its style
