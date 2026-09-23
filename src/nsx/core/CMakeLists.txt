# Layer 1 - core. Pure C++20. No libnx, no curl, no Borealis.
# Compiled TWICE: once for the Switch, once for the host test build. Anything
# added here must therefore compile on both. See ADR-0014.
# SPDX-License-Identifier: GPL-3.0-only

set(NSX_CORE_SOURCES
    # result/result.cpp
    # version/semver.cpp
    # update/manifest.cpp
    # update/update_policy.cpp
    # update/handoff.cpp
    # paths/sd_paths.cpp
    # paths/traversal_guard.cpp
    # text/format.cpp
    # json/safe_get.cpp
    # hash/sha256.cpp
    # net/url.cpp
    # net/mega_link.cpp
    # log/logger.cpp
    PARENT_SCOPE)
