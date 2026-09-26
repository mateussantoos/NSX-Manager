// SPDX-License-Identifier: GPL-3.0-only

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "nsx/core/log/file_logger.hpp"

TEST_SUITE("core::log::file_logger")
{
    TEST_CASE("FileLogger creates log entries with timestamps and levels")
    {
        namespace fs = std::filesystem;
        const fs::path testLog = fs::temp_directory_path() / "test_nsx_logger.log";
        if (fs::exists(testLog)) {
            fs::remove(testLog);
        }

        {
            nsx::core::log::FileLogger logger(testLog.string());
            CHECK(logger.path() == testLog.string());

            logger.info("Application starting");
            logger.warn("Sample warning");
            logger.error("Sample error");
            logger.debug("Debug information");
        }

        REQUIRE(fs::exists(testLog));

        std::ifstream in(testLog);
        std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());

        CHECK(content.find("[INFO] ") != std::string::npos);
        CHECK(content.find("Application starting") != std::string::npos);
        CHECK(content.find("[WARN] ") != std::string::npos);
        CHECK(content.find("Sample warning") != std::string::npos);
        CHECK(content.find("[ERROR]") != std::string::npos);
        CHECK(content.find("Sample error") != std::string::npos);
        CHECK(content.find("[DEBUG]") != std::string::npos);
        CHECK(content.find("Debug information") != std::string::npos);

        fs::remove(testLog);
    }
}
