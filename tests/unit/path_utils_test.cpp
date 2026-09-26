// SPDX-License-Identifier: GPL-3.0-only

#include <doctest/doctest.h>

#include "nsx/core/paths/path_utils.hpp"

TEST_SUITE("core::paths::path_utils")
{
    TEST_CASE("normalize sanitizes slashes and redundant elements")
    {
        using nsx::core::paths::normalize;

        CHECK(normalize("switch\\nsx-manager\\\\app.nro") == "switch/nsx-manager/app.nro");
        CHECK(normalize("/switch/nsx-manager/") == "/switch/nsx-manager");
        CHECK(normalize("sdmc:/") == "sdmc:/");
        CHECK(normalize("/") == "/");
    }

    TEST_CASE("hasExtension correctly matches file extensions")
    {
        using nsx::core::paths::hasExtension;

        CHECK(hasExtension("/switch/app.nro", ".nro"));
        CHECK(hasExtension("/switch/app.NRO", ".nro"));
        CHECK(hasExtension("/switch/app.nro", "nro"));
        CHECK(hasExtension("/bootloader/payloads/fusee.bin", ".bin"));
        CHECK_FALSE(hasExtension("/switch/app.nro", ".bin"));
        CHECK_FALSE(hasExtension("/switch/nro", ".nro"));
    }

    TEST_CASE("join combines path segments cleanly")
    {
        using nsx::core::paths::join;

        CHECK(join("/switch", "nsx-manager/app.nro") == "/switch/nsx-manager/app.nro");
        CHECK(join("/switch/", "/nsx-manager/app.nro") == "/switch/nsx-manager/app.nro");
        CHECK(join("sdmc:/switch", "test.bin") == "sdmc:/switch/test.bin");
    }
}
