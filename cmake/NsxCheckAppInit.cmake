# NsxCheckAppInit.cmake - fail the build if a Switch binary lost userAppInit.
#
#   cmake -DNM=<nm> -DELF=<file> -P cmake/NsxCheckAppInit.cmake
#
# This exists because the failure it catches is invisible everywhere else. libnx
# provides a weak, empty `userAppInit` and a program overrides it. If the
# override ends up in a static library that nothing references by symbol, the
# linker leaves it there, libnx's empty default is used, and the binary builds
# and links perfectly - then dies on the first call into an uninitialised
# service, before main, with a system error dialog and no output at all.
#
# That happened: nsx-ui-probe.nro crashed on hardware because Borealis's
# switch_wrapper.o was sitting unused inside libnsx_borealis.a. Nothing in the
# build said so. This does.
#
# SPDX-License-Identifier: GPL-3.0-only

if(NOT DEFINED NM OR NOT DEFINED ELF)
    message(FATAL_ERROR "usage: cmake -DNM=<nm> -DELF=<file> -P NsxCheckAppInit.cmake")
endif()

if(NOT EXISTS "${ELF}")
    message(FATAL_ERROR "check_app_init: ${ELF} does not exist")
endif()

execute_process(
    COMMAND "${NM}" "${ELF}"
    OUTPUT_VARIABLE symbols
    ERROR_VARIABLE nm_error
    RESULT_VARIABLE nm_status
    OUTPUT_STRIP_TRAILING_WHITESPACE)

if(NOT nm_status EQUAL 0)
    message(FATAL_ERROR "check_app_init: ${NM} failed: ${nm_error}")
endif()

get_filename_component(name "${ELF}" NAME)

if(NOT symbols MATCHES "[ \t]T userAppInit")
    message(FATAL_ERROR
        "${name} does not define userAppInit.\n"
        "\n"
        "libnx's empty default will be used, so pl, setsys, set, nifm and romfs "
        "will all be uninitialised. Borealis calls plGetSharedFontByType() from "
        "inside Application::init and the process will be killed there, before "
        "main, with no output.\n"
        "\n"
        "Call nsx_add_switch_services(<target>) for this target. It must add the "
        "source to the TARGET, not to a library - see cmake/NsxLayer.cmake.")
endif()

# The override is only worth having if it actually brings the services up.
foreach(required plInitialize setsysInitialize setInitialize nifmInitialize)
    if(NOT symbols MATCHES "[ \t]${required}")
        message(FATAL_ERROR
            "${name} defines userAppInit but never calls ${required}.\n"
            "Borealis and the update check both depend on it being initialised.")
    endif()
endforeach()

message(STATUS "check_app_init: ${name} initialises its services")
