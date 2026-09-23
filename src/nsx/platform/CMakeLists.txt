# Layer - platform. See docs/architecture/layering.md for what it may depend on.
#
# Sources are listed explicitly and never globbed, and are anchored to this
# directory because nsx_add_layer() runs from the root.
# SPDX-License-Identifier: GPL-3.0-only

set(NSX_PLATFORM_SOURCES
)

list(TRANSFORM NSX_PLATFORM_SOURCES PREPEND "${CMAKE_CURRENT_LIST_DIR}/")
set(NSX_PLATFORM_SOURCES ${NSX_PLATFORM_SOURCES} PARENT_SCOPE)
