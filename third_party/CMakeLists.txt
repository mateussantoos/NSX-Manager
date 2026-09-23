# third_party - vendored dependencies, wrapped so the rest of the tree sees
# ordinary CMake targets.
#
# Upstream Borealis ships only `library/borealis.mk`, so this wrapper is the one
# piece of real migration work CMake costs us (ADR-0002). It is written once and
# then frozen; it lives HERE and not inside the submodule so `git submodule
# update` never conflicts with it.
# SPDX-License-Identifier: GPL-3.0-only

# --------------------------------------------------------------------------
# nlohmann/json - header only
# --------------------------------------------------------------------------
add_library(nsx_json INTERFACE)
add_library(nsx::json ALIAS nsx_json)
target_include_directories(nsx_json SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/nlohmann")

# --------------------------------------------------------------------------
# zipper - minizip wrapper
# --------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/zipper/CMakeLists.txt")
    add_subdirectory(zipper)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/zipper/source")
    file(GLOB ZIPPER_SRC CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/zipper/source/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/zipper/source/*.cpp")
    add_library(nsx_zipper STATIC ${ZIPPER_SRC})
    add_library(nsx::zipper ALIAS nsx_zipper)
    target_include_directories(nsx_zipper SYSTEM PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/zipper/include")
    target_link_libraries(nsx_zipper PUBLIC z)
else()
    message(WARNING
        "third_party/zipper is empty. Run:  git submodule update --init --recursive")
endif()

# --------------------------------------------------------------------------
# Borealis - the UI framework (ADR-0010)
# Reproduces library/borealis.mk. The .cc extension on nxfmtwrapper is the
# reason upstream needs a custom mk in the first place.
# --------------------------------------------------------------------------
set(BRLS "${CMAKE_CURRENT_SOURCE_DIR}/borealis/library")

if(NOT EXISTS "${BRLS}/borealis.mk")
    message(WARNING
        "third_party/borealis is empty. Run:  git submodule update --init --recursive")
    return()
endif()

file(GLOB_RECURSE BRLS_SRC CONFIGURE_DEPENDS
    "${BRLS}/lib/*.cpp"
    "${BRLS}/lib/extern/nanovg/*.c"
    "${BRLS}/lib/extern/libretro-common/compat/*.c"
    "${BRLS}/lib/extern/libretro-common/encodings/*.c"
    "${BRLS}/lib/extern/libretro-common/features/*.c"
    "${BRLS}/lib/extern/nxfmtwrapper/*.cc"
    "${BRLS}/lib/extern/glad/*.c")

add_library(nsx_borealis STATIC ${BRLS_SRC})
add_library(nsx::borealis ALIAS nsx_borealis)

target_include_directories(nsx_borealis SYSTEM PUBLIC
    "${BRLS}/include"
    "${BRLS}/include/borealis/extern"
    "${BRLS}/lib/extern/fmt/include"
    "${BRLS}/lib/extern/nanovg"
    "${BRLS}/lib/extern/libretro-common/include")

target_compile_definitions(nsx_borealis PUBLIC
    BOREALIS_RESOURCES="romfs:/"
    __SWITCH__)

target_link_libraries(nsx_borealis PUBLIC glfw3 EGL glad glapi drm_nouveau m)
target_compile_features(nsx_borealis PUBLIC cxx_std_20)

# Vendored code is exempt from our warning policy - we do not own its style.
target_compile_options(nsx_borealis PRIVATE -w)
