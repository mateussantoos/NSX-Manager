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

# nsx_add_switch_services(<target>)
#
# Adds the libnx `userAppInit`/`userAppExit` override to <target>'s OWN sources.
#
# Directly, and never through a library, because that is the whole point. libnx
# provides a weak empty default and a program overrides it; a static library
# only contributes an object file when the link needs a symbol from it, and
# nothing references `userAppInit` by name. Put this in a library and the
# override is silently dropped, libnx's empty default wins, and Borealis crashes
# on the first uninitialised service call - with no output, because it happens
# before main.
#
# See src/nsx/platform/system/app_init.cpp and cmake/NsxCheckAppInit.cmake.
function(nsx_add_switch_services target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "nsx_add_switch_services: no target ${target}")
    endif()
    target_sources(${target} PRIVATE
        "${NSX_SOURCE_ROOT}/nsx/platform/system/app_init.cpp")

    # Verify it survived the link. A source added is not a symbol linked, and
    # the difference is exactly the bug this guards against.
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
                -DNM=${CMAKE_NM}
                -DELF=$<TARGET_FILE:${target}>
                -P "${CMAKE_SOURCE_DIR}/cmake/NsxCheckAppInit.cmake"
        COMMENT "Checking ${target} initialises its services"
        VERBATIM)

    set_property(GLOBAL APPEND PROPERTY NSX_SWITCH_SERVICE_TARGETS ${target})
endfunction()

# nsx_verify_switch_services()
#
# Configure-time companion to the post-link check above, for the failure that
# check cannot see: a NEW target that links Borealis and never calls
# nsx_add_switch_services at all. There is then no POST_BUILD step to fail, and
# the binary crashes on hardware exactly as nsx-ui-probe.nro did.
#
# Linking Borealis is the precise trigger, which is why this keys on that rather
# than on producing an NRO - nsx-forwarder.nro needs none of these services and
# correctly does not ask for them.
function(nsx_verify_switch_services)
    if(NOT TARGET nsx_ui)
        return()
    endif()
    get_property(declared GLOBAL PROPERTY NSX_SWITCH_SERVICE_TARGETS)

    get_property(targets DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(dir src/nsx/app apps/ui-probe)
        get_property(more DIRECTORY "${CMAKE_SOURCE_DIR}/${dir}" PROPERTY BUILDSYSTEM_TARGETS)
        list(APPEND targets ${more})
    endforeach()

    foreach(target IN LISTS targets)
        if(NOT TARGET ${target})
            continue()
        endif()
        get_target_property(type ${target} TYPE)
        if(NOT type STREQUAL "EXECUTABLE")
            continue()
        endif()
        get_target_property(libs ${target} LINK_LIBRARIES)
        if(NOT libs MATCHES "nsx::ui|nsx_ui")
            continue()
        endif()
        if(NOT target IN_LIST declared)
            message(FATAL_ERROR
                "${target} links Borealis but never called nsx_add_switch_services(). "
                "Borealis calls plGetSharedFontByType() from inside Application::init "
                "and the process is killed there, before main, with no output. "
                "See cmake/NsxLayer.cmake.")
        endif()
    endforeach()
endfunction()
