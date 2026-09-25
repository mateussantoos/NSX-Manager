# Layer - ui. See docs/architecture/layering.md for what it may depend on.
#
# The only layer that knows Borealis exists. Nothing here is host-compilable -
# it links glfw3, EGL, glad, glapi and drm_nouveau - so there is no HOST subset
# the way infra and domain have one (ADR-0017).
#
# Sources are listed explicitly and never globbed, and are anchored to this
# directory because nsx_add_layer() runs from the root.
# SPDX-License-Identifier: GPL-3.0-only

set(NSX_UI_SOURCES
    app_shell/shell.cpp
    theme/dark_theme.cpp
    tabs/home_tab.cpp
    tabs/update_tab.cpp
    tabs/cfw_tab.cpp
    tabs/firmware_tab.cpp
    tabs/tools_tab.cpp
    tabs/settings_tab.cpp
    widgets/background_job.cpp
    widgets/linear_list_item.cpp
    widgets/progress_dialog.cpp
)

list(TRANSFORM NSX_UI_SOURCES PREPEND "${CMAKE_CURRENT_LIST_DIR}/")
set(NSX_UI_SOURCES ${NSX_UI_SOURCES} PARENT_SCOPE)
