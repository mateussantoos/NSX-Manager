# NsxLayer.cmake - helpers that make the layered tree (ADR-0003) mechanical.
# SPDX-License-Identifier: GPL-3.0-only

# nsx_add_layer(<name> SOURCES <files...> DEPENDS <targets...>)
#
# Creates the static library `nsx_<name>` with one include root (src/) so every
# include reads `#include "nsx/<layer>/<module>/<file>.hpp"`.
#
# When SOURCES is empty the target is skipped with a STATUS message instead of
# failing configure. That is what lets this scaffold configure cleanly before
# any module has landed - and it costs nothing afterwards, because CI's
# build jobs fail loudly if an expected target is missing.
function(nsx_add_layer NAME)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS;DEFINES" ${ARGN})

    if(NOT ARG_SOURCES)
        message(STATUS "nsx_${NAME}: no sources yet - target skipped")
        return()
    endif()

    add_library(nsx_${NAME} STATIC ${ARG_SOURCES})
    add_library(nsx::${NAME} ALIAS nsx_${NAME})

    target_include_directories(nsx_${NAME}
        PUBLIC
            "${NSX_SOURCE_ROOT}"
            "${NSX_GENERATED_DIR}")

    target_compile_features(nsx_${NAME} PUBLIC cxx_std_20)

    # Drop nsx_* dependencies whose target does not exist yet. A layer with no
    # sources is skipped by this same function, and CMake would otherwise pass
    # the unknown name straight to the linker as `-lnsx_platform` - which fails
    # with "cannot find -lnsx_platform" rather than anything that points at the
    # real cause. Layers land one at a time; the build should tolerate that.
    set(RESOLVED_DEPS "")
    foreach(dep IN LISTS ARG_DEPENDS)
        if(dep MATCHES "^nsx_" AND NOT TARGET ${dep})
            message(STATUS "nsx_${NAME}: skipping ${dep} - not built yet")
        else()
            list(APPEND RESOLVED_DEPS ${dep})
        endif()
    endforeach()

    if(RESOLVED_DEPS)
        target_link_libraries(nsx_${NAME} PUBLIC ${RESOLVED_DEPS})
    endif()
    if(ARG_DEFINES)
        target_compile_definitions(nsx_${NAME} PUBLIC ${ARG_DEFINES})
    endif()

    set_target_properties(nsx_${NAME} PROPERTIES
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON)

    # Warnings are errors for first-party code only; third_party/ is exempt.
    target_compile_options(nsx_${NAME} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic -Wshadow
            -Wnon-virtual-dtor -Wcast-align -Wunused -Woverloaded-virtual
            -Wconversion -Wsign-conversion -Wdouble-promotion>
        $<$<AND:$<BOOL:${NSX_WERROR}>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-Werror>)
endfunction()
