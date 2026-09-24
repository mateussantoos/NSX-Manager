# Layer 1 - core. Pure C++20. No libnx, no curl, no Borealis.
#
# Compiled TWICE: once for the Switch, once for the host test build. Anything
# added here must therefore compile on both, and must not reach for a platform
# API. That constraint is the entire reason the test suite can run on CI without
# a console. See ADR-0014.
#
# Sources are listed explicitly and never globbed: a glob does not re-run when a
# file is added, so an unchanged build directory silently omits new code.
# SPDX-License-Identifier: GPL-3.0-only

set(NSX_CORE_SOURCES
    version/semver.cpp
    update/manifest.cpp
    update/update_policy.cpp
    update/handoff.cpp
    update/manifest_cache.cpp
    update/backoff.cpp
    # paths/sd_paths.cpp
    paths/traversal_guard.cpp
    paths/preserve_rules.cpp
    paths/extraction_policy.cpp
    catalog/catalog.cpp
    cfw/merge_marker.cpp
    firmware/firmware_set.cpp
    # text/format.cpp
    # json/safe_get.cpp
    hash/sha256.cpp
    # net/url.cpp
    # net/mega_link.cpp
    # log/logger.cpp
)

# nsx_add_layer() is invoked from the ROOT CMakeLists, so relative paths would
# resolve against the root. Anchor them here instead.
list(TRANSFORM NSX_CORE_SOURCES PREPEND "${CMAKE_CURRENT_LIST_DIR}/")
set(NSX_CORE_SOURCES ${NSX_CORE_SOURCES} PARENT_SCOPE)
