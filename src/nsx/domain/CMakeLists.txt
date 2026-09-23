# Layer - domain. See docs/architecture/layering.md for what it may depend on.
# SPDX-License-Identifier: GPL-3.0-only

set(NSX_DOMAIN_SOURCES
    # Modules are listed explicitly - never globbed. Globbing hides a new file
    # from an unchanged build directory and makes the build non-reproducible.
    PARENT_SCOPE)
