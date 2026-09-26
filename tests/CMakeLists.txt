# Host unit tests (ADR-0014).
#
# Everything here is compiled with the HOST compiler from the same
# target_sources lists the Switch build uses. That is the whole point: no
# emulator, no hardware, no second file list to keep in sync.
#
#   cmake --preset host-debug && ctest --preset host-debug --output-on-failure
# SPDX-License-Identifier: GPL-3.0-only

if(NOT TARGET nsx_core)
    message(STATUS "tests: nsx_core has no sources yet - no tests registered")
    return()
endif()

# One binary per module keeps a failure's blast radius small and lets ctest
# parallelise. Uncomment as each module lands.
set(NSX_TEST_MODULES
    semver
    update_policy
    manifest
    manifest_cache
    backoff
    handoff
    # sd_paths
    traversal_guard
    preserve_rules
    catalog
    sha256
    file_logger
)

# Suites that drive a use-case rather than a pure rule, and therefore need the
# portable subset of the upper layers. Registered only when those targets exist,
# so a build that has not enabled them still runs everything else.
set(NSX_TEST_MODULES_DOMAIN
    selfupdate
    cfw_install
    firmware_install
    catalog_service
    cleanup_service
    sysmodule_service
    telemetry_service
)

foreach(mod IN LISTS NSX_TEST_MODULES)
    add_executable(test_${mod} "unit/${mod}_test.cpp")
    target_link_libraries(test_${mod} PRIVATE nsx::core nsx::doctest)
    target_compile_definitions(test_${mod} PRIVATE
        DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
        NSX_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    add_test(NAME ${mod} COMMAND test_${mod})
    set_tests_properties(${mod} PROPERTIES LABELS "unit;core")
endforeach()

if(TARGET nsx_domain)
    foreach(mod IN LISTS NSX_TEST_MODULES_DOMAIN)
        add_executable(test_${mod} "unit/${mod}_test.cpp")
        target_link_libraries(test_${mod} PRIVATE nsx::domain nsx::doctest)
        target_compile_definitions(test_${mod} PRIVATE
            DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
            NSX_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
        add_test(NAME ${mod} COMMAND test_${mod})
        set_tests_properties(${mod} PROPERTIES LABELS "unit;domain")
    endforeach()
else()
    message(STATUS "tests: nsx_domain not built - skipping ${NSX_TEST_MODULES_DOMAIN}")
endif()
