# Layer - infra. See docs/architecture/layering.md for what it may depend on.
#
# Split in two. NSX_INFRA_HOST_SOURCES is the subset that compiles with a host
# compiler - no curl, no libnx - and is what lets the update orchestration in
# `domain` be unit tested without a console (ADR-0014). Everything else is
# device-only and is added on top for the Switch build.
#
# Sources are listed explicitly and never globbed, and are anchored to this
# directory because nsx_add_layer() runs from the root.
# SPDX-License-Identifier: GPL-3.0-only

# Error taxonomy and the https-only rule. Pure string and enum handling, so the
# vocabulary the UI renders is available to a host test.
set(NSX_INFRA_HOST_SOURCES
    http/tls_config.cpp
    archive/archive_error.cpp
)

# Needs libcurl, zipper and the generated CA bundle header.
set(NSX_INFRA_DEVICE_SOURCES
    http/curl_client.cpp
    archive/zip_extractor.cpp
)

set(NSX_INFRA_SOURCES ${NSX_INFRA_HOST_SOURCES} ${NSX_INFRA_DEVICE_SOURCES})

list(TRANSFORM NSX_INFRA_HOST_SOURCES PREPEND "${CMAKE_CURRENT_LIST_DIR}/")
list(TRANSFORM NSX_INFRA_SOURCES      PREPEND "${CMAKE_CURRENT_LIST_DIR}/")
set(NSX_INFRA_HOST_SOURCES ${NSX_INFRA_HOST_SOURCES} PARENT_SCOPE)
set(NSX_INFRA_SOURCES      ${NSX_INFRA_SOURCES}      PARENT_SCOPE)
