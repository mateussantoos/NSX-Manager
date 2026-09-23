# Layer - domain. See docs/architecture/layering.md for what it may depend on.
#
# Sources are listed explicitly and never globbed, and are anchored to this
# directory because nsx_add_layer() runs from the root.
# SPDX-License-Identifier: GPL-3.0-only

set(NSX_DOMAIN_SOURCES
)

list(TRANSFORM NSX_DOMAIN_SOURCES PREPEND "${CMAKE_CURRENT_LIST_DIR}/")
set(NSX_DOMAIN_SOURCES ${NSX_DOMAIN_SOURCES} PARENT_SCOPE)
