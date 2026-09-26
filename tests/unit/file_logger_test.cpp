// SPDX-License-Identifier: GPL-3.0-only

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "nsx/core/log/file_logger.hpp"

TEST_SUITE("core::log::file_logger")
{
    TEST_CASE("FileLogger creates formatted structured log entries")
    {
        namespace fs = std::filesystem;
        const fs::path testLog = fs::temp_directory_path() / "test_nsx_logger_fmt.log";
        const fs::path backupLog = fs::temp_directory_path() / "test_nsx_logger_fmt.log.1";
        std::error_code ec;
        fs::remove(testLog, ec);
        fs::remove(backupLog, ec);

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

        CHECK(content.find("[INFO]") != std::string::npos);
        CHECK(content.find("Application starting") != std::string::npos);
        CHECK(content.find("[WARN]") != std::string::npos);
        CHECK(content.find("Sample warning") != std::string::npos);
        CHECK(content.find("[ERROR]") != std::string::npos);
        CHECK(content.find("Sample error") != std::string::npos);
        CHECK(content.find("[DEBUG]") != std::string::npos);
        CHECK(content.find("Debug information") != std::string::npos);

        fs::remove(testLog, ec);
    }

    TEST_CASE("FileLogger rotates when size cap is exceeded")
    {
        namespace fs = std::filesystem;
        const fs::path testLog = fs::temp_directory_path() / "test_nsx_logger_rot.log";
        const fs::path backupLog = fs::temp_directory_path() / "test_nsx_logger_rot.log.1";
        std::error_code ec;
        fs::remove(testLog, ec);
        fs::remove(backupLog, ec);

        // Cap size to 60 bytes so rotation triggers on second write
        {
            nsx::core::log::FileLogger logger(testLog.string(), 60);
            logger.info("First line of sufficient length to trigger rotation on subsequent write");
            logger.info("Second line that causes the previous file to be rotated into .1");
        }

        CHECK(fs::exists(testLog));
        CHECK(fs::exists(backupLog));

        fs::remove(testLog, ec);
        fs::remove(backupLog, ec);
    }

    TEST_CASE("FileLogger handles unwritable destination gracefully")
    {
        // Should not throw or crash when file is impossible to write
        nsx::core::log::FileLogger invalidLogger("/invalid_path_unlikely_to_exist/test.log");
        CHECK_NOTHROW(invalidLogger.info("This should not crash"));
    }
}
