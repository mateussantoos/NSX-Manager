# Host unit tests (ADR-0014).
#
# Everything here links nsx_core ONLY, compiled with the HOST compiler from the
# same target_sources list the Switch build uses. That is the whole point: no
# emulator, no hardware, no second file list to keep in sync.
#
#   cmake --preset host-debug && ctest --preset host-debug --output-on-failure
# SPDX-License-Identifier: GPL-3.0-only

if(NOT TARGET nsx_core)
    message(STATUS "tests: nsx_core has no sources yet - no tests registered")
    return()
endif()

add_library(nsx_doctest INTERFACE)
target_include_directories(nsx_doctest SYSTEM INTERFACE
    "${CMAKE_SOURCE_DIR}/third_party/doctest")

# One binary per module keeps a failure's blast radius small and lets ctest
# parallelise. Uncomment as each module lands.
set(NSX_TEST_MODULES
    # semver
    # update_policy
    # manifest
    # handoff
    # sd_paths
    # traversal_guard
    # sha256
    # text_format
    # json_safe_get
    # mega_link
)

foreach(mod IN LISTS NSX_TEST_MODULES)
    add_executable(test_${mod} "unit/${mod}_test.cpp")
    target_link_libraries(test_${mod} PRIVATE nsx::core nsx_doctest)
    target_compile_definitions(test_${mod} PRIVATE
        DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
        NSX_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME ${mod} COMMAND test_${mod})
    set_tests_properties(${mod} PROPERTIES LABELS "unit;core")
endforeach()
