# Layer - domain. See docs/architecture/layering.md for what it may depend on.
#
# Split the same way infra is. The use-case itself (update_service) talks only
# to the ports in ports.hpp, so it compiles on a host and the whole update flow
# - cache, backoff, manifest, download, handoff - is exercised in tests against
# fakes. The adapters that bind those ports to libcurl and to the SD card are
# device-only.
#
# Sources are listed explicitly and never globbed, and are anchored to this
# directory because nsx_add_layer() runs from the root.
# SPDX-License-Identifier: GPL-3.0-only

set(NSX_DOMAIN_HOST_SOURCES
    selfupdate/update_service.cpp
    selfupdate/system_clock.cpp
    cfw/cfw_install_service.cpp
    firmware/firmware_install_service.cpp
    catalog/catalog_service.cpp
    maintenance/cleanup_service.cpp
    sysmodule/sysmodule_service.cpp
    network/telemetry_service.cpp
    motd/motd_service.cpp
)

set(NSX_DOMAIN_DEVICE_SOURCES
    selfupdate/sd_file_store.cpp
    selfupdate/curl_gateway.cpp
    cfw/zip_archive_gateway.cpp
)

set(NSX_DOMAIN_SOURCES ${NSX_DOMAIN_HOST_SOURCES} ${NSX_DOMAIN_DEVICE_SOURCES})

list(TRANSFORM NSX_DOMAIN_HOST_SOURCES PREPEND "${CMAKE_CURRENT_LIST_DIR}/")
list(TRANSFORM NSX_DOMAIN_SOURCES      PREPEND "${CMAKE_CURRENT_LIST_DIR}/")
set(NSX_DOMAIN_HOST_SOURCES ${NSX_DOMAIN_HOST_SOURCES} PARENT_SCOPE)
set(NSX_DOMAIN_SOURCES      ${NSX_DOMAIN_SOURCES}      PARENT_SCOPE)
