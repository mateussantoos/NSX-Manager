# NsxCaBundle.cmake - embed the pinned Mozilla CA bundle as a compile-time blob.
#
# devkitPro's switch-curl is mbedTLS-backed and has NO system trust store. That
# absence is why the predecessor disabled certificate verification on every
# request rather than supplying an anchor. We compile one in, so there is no
# "certificates missing" branch for an insecure fallback to hide in.
#
# See ADR-0006 and docs/architecture/threat-model.md.
# SPDX-License-Identifier: GPL-3.0-only

set(NSX_CACERT_PEM    "${CMAKE_SOURCE_DIR}/third_party/cacert/cacert.pem")
set(NSX_CACERT_SHA    "${CMAKE_SOURCE_DIR}/third_party/cacert/cacert.pem.sha256")
set(NSX_CA_HEADER     "${NSX_GENERATED_DIR}/nsx/infra/http/ca_bundle.hpp")
set(NSX_PEM_TO_HEADER "${CMAKE_SOURCE_DIR}/tools/cacert/pem_to_header.py")

find_package(Python3 COMPONENTS Interpreter QUIET)

if(NOT EXISTS "${NSX_CACERT_PEM}")
    message(FATAL_ERROR
        "The CA bundle is missing: ${NSX_CACERT_PEM}\n"
        "It is gitignored on purpose - only its hash is committed, so the trust\n"
        "anchor is reviewed as a one-line diff. Fetch and verify it with:\n"
        "    tools/cacert/fetch.sh")
endif()

if(NOT Python3_Interpreter_FOUND)
    message(FATAL_ERROR "Python 3 is required to embed the CA bundle")
endif()

# Verify the bundle against its committed hash at CONFIGURE time. A build must
# never embed a trust anchor nobody reviewed.
file(SHA256 "${NSX_CACERT_PEM}" NSX_CACERT_ACTUAL)
file(STRINGS "${NSX_CACERT_SHA}" NSX_CACERT_LINE LIMIT_COUNT 1)
string(REGEX MATCH "^[0-9a-f]+" NSX_CACERT_EXPECTED "${NSX_CACERT_LINE}")

if(NOT NSX_CACERT_ACTUAL STREQUAL NSX_CACERT_EXPECTED)
    message(FATAL_ERROR
        "CA bundle hash mismatch - refusing to embed it.\n"
        "  expected: ${NSX_CACERT_EXPECTED}\n"
        "  actual:   ${NSX_CACERT_ACTUAL}\n"
        "If Mozilla published a new bundle, run 'tools/cacert/fetch.sh --update'\n"
        "and review the change.")
endif()

message(STATUS "CA bundle verified: ${NSX_CACERT_EXPECTED}")

add_custom_command(
    OUTPUT  "${NSX_CA_HEADER}"
    COMMAND "${Python3_EXECUTABLE}" "${NSX_PEM_TO_HEADER}"
            --pem "${NSX_CACERT_PEM}" --out "${NSX_CA_HEADER}"
    DEPENDS "${NSX_CACERT_PEM}" "${NSX_PEM_TO_HEADER}"
    COMMENT "Embedding CA bundle -> nsx/infra/http/ca_bundle.hpp"
    VERBATIM)

# ALL, because the header is a build input for the infra layer and generating it
# unconditionally means `cmake --build` always leaves a complete, inspectable
# tree - rather than the header only appearing once some other target happens to
# depend on it.
add_custom_target(nsx_ca_bundle ALL DEPENDS "${NSX_CA_HEADER}")
