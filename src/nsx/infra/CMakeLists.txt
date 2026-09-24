# Layer - infra. See docs/architecture/layering.md for what it may depend on.
#
# Sources are listed explicitly and never globbed, and are anchored to this
# directory because nsx_add_layer() runs from the root.
# SPDX-License-Identifier: GPL-3.0-only

set(NSX_INFRA_SOURCES
    http/tls_config.cpp
    http/curl_client.cpp
)

list(TRANSFORM NSX_INFRA_SOURCES PREPEND "${CMAKE_CURRENT_LIST_DIR}/")
set(NSX_INFRA_SOURCES ${NSX_INFRA_SOURCES} PARENT_SCOPE)
